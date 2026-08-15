#include "localsend/core.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#include <chrono>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <map>
#include <mutex>
#include <thread>

#include "json.hpp"
#include "localsend/cache.h"
#include "localsend/crypto.h"
#include "localsend/discovery.h"
#include "localsend/pairing.h"
#include "localsend/storage.h"
#include "localsend/transport.h"

namespace localsend {

namespace {

constexpr size_t kChunkSize = 4 * 1024 * 1024; // 4MB
constexpr int kAnnounceIntervalSec = 3;
constexpr int kStaleTimeoutSec = 15;
constexpr int kTagLen = 16;

namespace fs = std::filesystem;

std::string getDefaultLocalIp() {
  int fd = socket(AF_INET, SOCK_DGRAM, 0);
  if (fd < 0) return "127.0.0.1";
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(53);
  inet_pton(AF_INET, "8.8.8.8", &addr.sin_addr);
  if (connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0) {
    sockaddr_in local{};
    socklen_t len = sizeof(local);
    if (getsockname(fd, reinterpret_cast<sockaddr*>(&local), &len) == 0) {
      char buf[INET_ADDRSTRLEN];
      inet_ntop(AF_INET, &local.sin_addr, buf, sizeof(buf));
      close(fd);
      return buf;
    }
  }
  close(fd);
  return "127.0.0.1";
}

std::string generateDeviceId() {
  std::ifstream in("/proc/sys/kernel/random/uuid");
  std::string id;
  if (in >> id) return id;
  std::string seed = std::to_string(nowEpochSeconds()) + std::to_string(getpid());
  return Crypto::md5(reinterpret_cast<const uint8_t*>(seed.data()), seed.size());
}

// Load the persisted device id, or generate and persist a new one. Keeps the
// identity stable across restarts so the trusted-device whitelist survives.
std::string loadOrCreateDeviceId(const std::string& dataDir) {
  std::error_code ec;
  fs::create_directories(dataDir, ec);
  std::string path = dataDir + "/device_id";
  std::ifstream in(path);
  std::string id;
  if (in) {
    std::getline(in, id);
    while (!id.empty() && (id.back() == '\r' || id.back() == '\n' || id.back() == ' ' || id.back() == '\t')) {
      id.pop_back();
    }
  }
  if (!id.empty()) return id;
  id = generateDeviceId();
  std::ofstream out(path, std::ios::trunc);
  if (out) out << id << "\n";
  return id;
}

json::Value fileInfoToJson(const FileInfo& f) {
  json::Value v = json::Value::object();
  v.set("fileId", f.fileId);
  v.set("fileName", f.fileName);
  v.set("fileType", fileTypeToString(f.fileType));
  v.set("fileSize", f.fileSize);
  v.set("fileDuration", f.duration);
  v.set("createTime", f.createTime);
  v.set("cachePath", f.cachePath);
  return v;
}

FileInfo fileInfoFromJson(const json::Value& v) {
  FileInfo f;
  f.fileId = v["fileId"].asString();
  f.fileName = v["fileName"].asString();
  f.fileType = fileTypeFromString(v["fileType"].asString());
  f.fileSize = v["fileSize"].asInt(0);
  f.duration = v["fileDuration"].asInt(0);
  f.createTime = v["createTime"].asString();
  f.cachePath = v["cachePath"].asString();
  return f;
}

// Portable UTC struct -> epoch seconds (timegm is a GNU/BSD extension).
int64_t timegmPortable(std::tm* tm) {
  auto daysFromCivil = [](int y, unsigned m, unsigned d) -> int64_t {
    y -= m <= 2;
    const int64_t era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = static_cast<unsigned>(y - era * 400);            // [0, 399]
    const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1; // [0, 365]
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;          // [0, 146096]
    return era * 146097 + static_cast<int64_t>(doe) - 719468;
  };
  int64_t days = daysFromCivil(tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday);
  return days * 86400 + tm->tm_hour * 3600 + tm->tm_min * 60 + tm->tm_sec;
}

json::Value stripSig(const json::Value& v) {
  json::Value out = json::Value::object();
  for (const auto& kv : v.asObject()) {
    if (kv.first != "sig") out.set(kv.first, kv.second);
  }
  return out;
}

// Verify that `headerJson` carries a valid RSA signature over its own canonical
// body (the header minus the "sig" field) produced by the holder of `publicKeyPem`.
bool verifySignedHeader(const std::string& headerJson, const std::string& publicKeyPem) {
  if (publicKeyPem.empty()) return false;
  json::Value v = json::Value::parse(headerJson);
  std::string sigB64 = v["sig"].asString();
  if (sigB64.empty()) return false;
  std::vector<uint8_t> sig = Crypto::fromBase64(sigB64);
  if (sig.empty()) return false;
  std::string body = stripSig(v).dump();
  std::vector<uint8_t> data(body.begin(), body.end());
  return Crypto::rsaVerify(publicKeyPem, data, sig);
}

// Serialize `v`, sign it with our private key, and return JSON that includes "sig".
std::string signJson(const json::Value& v, const std::string& privateKeyPem) {
  std::string body = v.dump();
  std::vector<uint8_t> data(body.begin(), body.end());
  std::vector<uint8_t> sig;
  if (!Crypto::rsaSign(privateKeyPem, data, sig)) return body; // unsigned; will fail verify
  json::Value out = v;
  out.set("sig", Crypto::toBase64(sig));
  return out.dump();
}

std::string addHoursToUtcString(const std::string& utc, int hours) {
  std::tm tm{};
  int y = 0, mo = 0, d = 0, h = 0, mi = 0, s = 0;
  if (std::sscanf(utc.c_str(), "%d-%d-%d %d:%d:%d", &y, &mo, &d, &h, &mi, &s) != 6) return utc;
  tm.tm_year = y - 1900;
  tm.tm_mon = mo - 1;
  tm.tm_mday = d;
  tm.tm_hour = h;
  tm.tm_min = mi;
  tm.tm_sec = s;
  std::time_t t = timegmPortable(&tm);
  t += static_cast<std::time_t>(hours) * 3600;
  gmtime_r(&t, &tm);
  char buf[32];
  std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d", tm.tm_year + 1900,
                tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
  return std::string(buf);
}

json::Value ackJson(bool ok, const std::string& error = "") {
  json::Value v = json::Value::object();
  v.set("cmd", "ack");
  v.set("ok", ok);
  v.set("error", error);
  return v;
}

} // namespace

struct CoreImpl {
  CoreConfig cfg;
  std::string privateKeyPem;
  std::string publicKeyPem;
  std::string localIpCache;
  Settings settings;
  Callbacks callbacks;

  std::unique_ptr<Storage> storage;
  std::unique_ptr<Cache> cache;
  std::unique_ptr<Discovery> discovery;
  std::unique_ptr<Transport> transport;

  mutable std::mutex devMutex;
  std::map<std::string, Device> devices;
  std::map<std::string, int64_t> lastSeen;
  std::map<std::string, Device> pendingRequests;
  std::map<std::string, std::string> pendingRequestNonce; // incoming pair requests: deviceId -> nonce

  struct PendingOutgoing {
    std::string nonce;
    std::string publicKey; // peer key captured at request time (empty for manual IP)
  };
  std::map<std::string, PendingOutgoing> pendingOutgoing; // outgoing pair requests: deviceId -> state

  std::mutex cbMutex;
  std::atomic<bool> running{false};
  std::thread announcerThread;

  std::string selfId() const { return cfg.deviceId; }

  void emitDeviceFound(const Device& d);
  void emitDeviceOnline(const Device& d);
  void emitDeviceOffline(const Device& d);
  void emitPairRequest(const Device& d);
  void emitPairResult(const Device& d, bool accepted);
  void emitFileAdded(const FileInfo& f);
  void emitFileRemoved(const FileInfo& f);
  void emitFileReceiveStarted(const FileInfo& f);
  void emitTransferProgress(const TransferProgress& p);
  void emitTransferFinished(const std::string& fileId, const std::string& fileName, bool ok);
  void emitError(const std::string& message);

  Device deviceFromDiscovery(const DiscoveryMessage& m);
  void onDiscoveryMessage(const DiscoveryMessage& m);
  std::string onIncoming(const std::string& peerId, const std::string& headerJson,
                         size_t chunkTotal, const Transport::ChunkReader& readChunk);

  bool sendFileToPeer(const Device& peer, const FileInfo& fi);
  bool sendDeleteToPeer(const Device& peer, const FileInfo& fi);
  void syncWithPeer(const Device& peer);
  void syncWithAllTrusted();
  void announceLoop();
  void markOfflineStale();

  std::string encryptChunk(const std::string& keyB64, const std::vector<uint8_t>& plain,
                           std::vector<uint8_t>& out);
  bool decryptChunk(const std::string& keyB64, const std::vector<uint8_t>& frame,
                    std::vector<uint8_t>& plain);

  void applyCacheExpiry();
  void applyRateLimit();
};

// ---------- callback dispatch ----------

void CoreImpl::emitDeviceFound(const Device& d) {
  Callbacks cb;
  { std::lock_guard<std::mutex> lock(cbMutex); cb = callbacks; }
  if (cb.onDeviceFound) cb.onDeviceFound(d);
}
void CoreImpl::emitDeviceOnline(const Device& d) {
  Callbacks cb;
  { std::lock_guard<std::mutex> lock(cbMutex); cb = callbacks; }
  if (cb.onDeviceOnline) cb.onDeviceOnline(d);
}
void CoreImpl::emitDeviceOffline(const Device& d) {
  Callbacks cb;
  { std::lock_guard<std::mutex> lock(cbMutex); cb = callbacks; }
  if (cb.onDeviceOffline) cb.onDeviceOffline(d);
}
void CoreImpl::emitPairRequest(const Device& d) {
  Callbacks cb;
  { std::lock_guard<std::mutex> lock(cbMutex); cb = callbacks; }
  if (cb.onPairRequest) cb.onPairRequest(d);
}
void CoreImpl::emitPairResult(const Device& d, bool accepted) {
  Callbacks cb;
  { std::lock_guard<std::mutex> lock(cbMutex); cb = callbacks; }
  if (cb.onPairResult) cb.onPairResult(d, accepted);
}
void CoreImpl::emitFileAdded(const FileInfo& f) {
  Callbacks cb;
  { std::lock_guard<std::mutex> lock(cbMutex); cb = callbacks; }
  if (cb.onFileAdded) cb.onFileAdded(f);
}
void CoreImpl::emitFileRemoved(const FileInfo& f) {
  Callbacks cb;
  { std::lock_guard<std::mutex> lock(cbMutex); cb = callbacks; }
  if (cb.onFileRemoved) cb.onFileRemoved(f);
}
void CoreImpl::emitFileReceiveStarted(const FileInfo& f) {
  Callbacks cb;
  { std::lock_guard<std::mutex> lock(cbMutex); cb = callbacks; }
  if (cb.onFileReceiveStarted) cb.onFileReceiveStarted(f);
}
void CoreImpl::emitTransferProgress(const TransferProgress& p) {
  Callbacks cb;
  { std::lock_guard<std::mutex> lock(cbMutex); cb = callbacks; }
  if (cb.onTransferProgress) cb.onTransferProgress(p);
}
void CoreImpl::emitTransferFinished(const std::string& fileId, const std::string& fileName, bool ok) {
  Callbacks cb;
  { std::lock_guard<std::mutex> lock(cbMutex); cb = callbacks; }
  if (cb.onTransferFinished) cb.onTransferFinished(fileId, fileName, ok);
}
void CoreImpl::emitError(const std::string& message) {
  Callbacks cb;
  { std::lock_guard<std::mutex> lock(cbMutex); cb = callbacks; }
  if (cb.onError) cb.onError(message);
}

// ---------- helpers ----------

Device CoreImpl::deviceFromDiscovery(const DiscoveryMessage& m) {
  Device d;
  d.deviceId = m.deviceId;
  d.deviceName = m.deviceName;
  d.deviceType = m.deviceType;
  d.ip = m.ip;
  d.tcpPort = m.tcpPort;
  d.publicKey = m.publicKey;
  d.isTrusted = m.isTrusted;
  d.isOnline = true;
  return d;
}

std::string CoreImpl::encryptChunk(const std::string& keyB64, const std::vector<uint8_t>& plain,
                                   std::vector<uint8_t>& out) {
  auto key = Crypto::fromBase64(keyB64);
  if (key.empty()) return "no aes key";
  std::vector<uint8_t> iv, tag;
  Crypto::generateAesKey(iv, 12); // random 96-bit nonce per chunk -> no reuse
  if (!Crypto::aesGcmEncrypt(key, iv, plain.data(), plain.size(), out, tag)) return "encrypt failed";
  // Frame layout: IV(12) || ciphertext || tag(16).
  out.insert(out.begin(), iv.begin(), iv.end());
  out.insert(out.end(), tag.begin(), tag.end());
  return "";
}

bool CoreImpl::decryptChunk(const std::string& keyB64, const std::vector<uint8_t>& frame,
                            std::vector<uint8_t>& plain) {
  constexpr size_t kIvLen = 12;
  if (frame.size() < kIvLen + kTagLen) return false;
  auto key = Crypto::fromBase64(keyB64);
  if (key.empty()) return false;
  std::vector<uint8_t> iv(frame.begin(), frame.begin() + kIvLen);
  std::vector<uint8_t> tag(frame.end() - kTagLen, frame.end());
  return Crypto::aesGcmDecrypt(key, iv, frame.data() + kIvLen, frame.size() - kIvLen - kTagLen, tag, plain);
}

// ---------- discovery ----------

void CoreImpl::onDiscoveryMessage(const DiscoveryMessage& m) {
  bool trusted = storage && storage->getTrustedDevice(m.deviceId).has_value();
  bool wasOnline = false;
  Device d;
  {
    std::lock_guard<std::mutex> lock(devMutex);
    wasOnline = devices[m.deviceId].isOnline;
    d = deviceFromDiscovery(m);
    d.isTrusted = trusted;
    if (trusted) {
      if (auto td = storage->getTrustedDevice(m.deviceId)) {
        d.aesKey = td->aesKey;
        d.isTrusted = true;
      }
    }
    devices[m.deviceId] = d;
    lastSeen[m.deviceId] = nowEpochSeconds();
  }
  emitDeviceFound(d);
  if (trusted && !wasOnline) {
    Device snapshot;
    { std::lock_guard<std::mutex> lock(devMutex); snapshot = devices[m.deviceId]; }
    if (storage) storage->updateTrustedDevice(snapshot);
    emitDeviceOnline(snapshot);
    std::thread([this, snapshot]() { syncWithPeer(snapshot); }).detach();
  }
}

// ---------- transport incoming ----------

std::string CoreImpl::onIncoming(const std::string& peerId, const std::string& headerJson,
                                 size_t chunkTotal, const Transport::ChunkReader& readChunk) {
  json::Value h = json::Value::parse(headerJson);
  std::string cmd = h["cmd"].asString();

  // A peer is authenticated only if it is in the trusted list AND its control
  // message carries a valid RSA signature from that device's key. This prevents
  // spoofing a trusted deviceId on the unauthenticated LAN.
  auto authenticatedPeer = [&]() -> bool {
    if (!storage) return false;
    auto td = storage->getTrustedDevice(peerId);
    if (!td || td->publicKey.empty()) return false;
    return verifySignedHeader(headerJson, td->publicKey);
  };

  if (cmd == "fileProbe") {
    if (!authenticatedPeer()) return ackJson(false, "untrusted").dump();
    std::string fileId = h["fileId"].asString();
    bool exists = storage && storage->getFile(fileId).has_value();
    json::Value r = json::Value::object();
    r.set("cmd", "fileProbeAck");
    r.set("fileId", fileId);
    r.set("exists", exists);
    return r.dump();
  }

  if (cmd == "fileResume") {
    if (!authenticatedPeer()) return ackJson(false, "untrusted").dump();
    std::string fileId = h["fileId"].asString();
    int done = 0, total = 0;
    std::string peer;
    if (storage) storage->loadTransferState(fileId, done, total, peer);
    json::Value r = json::Value::object();
    r.set("cmd", "fileResumeAck");
    r.set("fileId", fileId);
    r.set("chunksDone", done);
    return r.dump();
  }

  if (cmd == "fileAdd") {
    FileInfo fi = fileInfoFromJson(h);
    int resumeFrom = static_cast<int>(h["resumeFrom"].asInt(0));
    int totalChunks = static_cast<int>(h["chunkTotal"].asInt(0));
    std::string enc = h["enc"].asString();

    if (!authenticatedPeer()) {
      return ackJson(false, "untrusted").dump();
    }

    if (totalChunks == 0) {
      if (auto existing = storage->getFile(fi.fileId)) fi.cachePath = existing->cachePath;
      if (storage) storage->upsertFile(fi);
      emitFileAdded(fi);
      return ackJson(true).dump();
    }

    Device peer;
    {
      std::lock_guard<std::mutex> lock(devMutex);
      auto it = devices.find(peerId);
      if (it == devices.end()) return ackJson(false, "unknown peer").dump();
      peer = it->second;
    }

    std::string finalPath = cache->buildCachePath(fi.fileId, fi.fileName);
    bool append = resumeFrom > 0 && fs::exists(finalPath);
    // Do NOT truncate when resuming: opening with trunc would destroy the
    // already-received prefix before appending the remaining chunks.
    std::ofstream out(finalPath, std::ios::binary | (append ? std::ios::app : std::ios::trunc));
    if (!out) return ackJson(false, "open failed").dump();

    emitFileReceiveStarted(fi);

    int chunksDone = resumeFrom;
    size_t expected = totalChunks > resumeFrom ? static_cast<size_t>(totalChunks - resumeFrom) : 0;
    bool fail = false;
    std::string failMsg;
    for (size_t i = 0; i < expected; ++i) {
      std::vector<uint8_t> frame;
      if (!readChunk(resumeFrom + i, frame)) {
        fail = true;
        failMsg = "transfer aborted";
        break;
      }
      std::vector<uint8_t> plain;
      if (enc == "aes128-gcm") {
        if (!decryptChunk(peer.aesKey, frame, plain)) {
          fail = true;
          failMsg = "decrypt failed";
          break;
        }
      } else {
        plain = std::move(frame);
      }
      out.write(reinterpret_cast<const char*>(plain.data()), static_cast<std::streamsize>(plain.size()));
      if (!out) {
        fail = true;
        failMsg = "write failed";
        break;
      }
      ++chunksDone;
      if (storage) storage->saveTransferState(fi.fileId, chunksDone, totalChunks, peerId);
      TransferProgress p;
      p.fileId = fi.fileId;
      p.fileName = fi.fileName;
      p.transferredBytes = static_cast<int64_t>(static_cast<size_t>(chunksDone) * kChunkSize);
      p.totalBytes = fi.fileSize;
      p.percent = fi.fileSize > 0 ? static_cast<double>(chunksDone * kChunkSize) / fi.fileSize : 0.0;
      if (p.percent > 1.0) p.percent = 1.0;
      emitTransferProgress(p);
    }
    out.close();

    if (fail) {
      return ackJson(false, failMsg).dump();
    }

    if (fs::file_size(finalPath) != static_cast<uintmax_t>(fi.fileSize)) {
      ::unlink(finalPath.c_str());
      if (storage) storage->removeTransferState(fi.fileId);
      return ackJson(false, "size mismatch").dump();
    }
    std::string gotMd5 = Crypto::md5File(finalPath);
    if (!gotMd5.empty() && gotMd5 != fi.fileId) {
      ::unlink(finalPath.c_str());
      if (storage) storage->removeTransferState(fi.fileId);
      return ackJson(false, "md5 mismatch").dump();
    }

    fi.cachePath = finalPath;
    fi.isLocal = true;
    if (fi.createTime.empty()) fi.createTime = nowUtcString();
    if (storage) {
      storage->removeTransferState(fi.fileId);
      storage->upsertFile(fi);
    }
    emitFileAdded(fi);
    emitTransferFinished(fi.fileId, fi.fileName, true);
    return ackJson(true).dump();
  }

  if (cmd == "fileDelete") {
    if (!authenticatedPeer()) return ackJson(false, "untrusted").dump();
    std::string fileId = h["fileId"].asString();
    if (auto existing = storage->getFile(fileId)) {
      cache->removeFile(existing->fileId, existing->fileName);
      storage->removeFile(fileId);
      emitFileRemoved(*existing);
    }
    return ackJson(true).dump();
  }

  if (cmd == "syncAllFiles") {
    if (!authenticatedPeer()) return ackJson(false, "untrusted").dump();
    json::Value missing = json::Value::array();
    for (const auto& item : h["fileList"].asArray()) {
      FileInfo f = fileInfoFromJson(item);
      if (!storage || !storage->getFile(f.fileId).has_value()) {
        missing.push(item);
      }
    }
    json::Value r = json::Value::object();
    r.set("cmd", "syncAllFilesAck");
    r.set("missing", missing);
    return r.dump();
  }

  if (cmd == "pairRequest") {
    PairRequest req;
    if (!Pairing::parseRequest(headerJson, req)) return ackJson(false, "bad request").dump();
    Device d;
    d.deviceId = req.deviceId;
    d.deviceName = req.deviceName;
    d.deviceType = req.deviceType;
    d.publicKey = req.publicKey;
    d.ip = req.ip;
    d.tcpPort = req.tcpPort;
    d.isTrusted = false;
    d.isOnline = true;
    {
      std::lock_guard<std::mutex> lock(devMutex);
      pendingRequests[d.deviceId] = d;
      pendingRequestNonce[d.deviceId] = req.nonce;
      devices[d.deviceId] = d;
      lastSeen[d.deviceId] = nowEpochSeconds();
    }
    emitPairRequest(d);
    return ackJson(true).dump();
  }

  if (cmd == "pairAccept") {
    std::string targetId, pubPem, encKey, name, type;
    uint16_t tcpPort = 0;
    if (!Pairing::parseAccept(headerJson, targetId, pubPem, encKey, name, type, tcpPort)) {
      return ackJson(false, "bad accept").dump();
    }
    if (targetId != selfId()) return ackJson(false, "target mismatch").dump();

    // Must answer one of our own outstanding pair requests, with a matching nonce,
    // and be signed by the key we originally addressed. Otherwise any LAN peer
    // could self-trust by sending an unsolicited pairAccept.
    PendingOutgoing pending;
    bool havePending = false;
    {
      std::lock_guard<std::mutex> lock(devMutex);
      auto it = pendingOutgoing.find(peerId);
      if (it != pendingOutgoing.end()) {
        pending = it->second;
        havePending = true;
      }
    }
    if (!havePending) return ackJson(false, "no pending request").dump();
    std::string nonce = h["nonce"].asString();
    if (nonce.empty() || nonce != pending.nonce) return ackJson(false, "nonce mismatch").dump();
    // For manual-IP peers we have no prior key -> trust-on-first-use from the
    // embedded key. For discovered peers, verify against the announced key.
    std::string verifyKey = pending.publicKey.empty() ? pubPem : pending.publicKey;
    if (!verifySignedHeader(headerJson, verifyKey)) return ackJson(false, "bad signature").dump();

    auto enc = Crypto::fromBase64(encKey);
    auto dec = std::vector<uint8_t>();
    if (!Crypto::rsaDecrypt(privateKeyPem, enc, dec)) return ackJson(false, "decrypt failed").dump();
    std::string aesKeyB64 = Crypto::toBase64(dec);

    Device d;
    {
      std::lock_guard<std::mutex> lock(devMutex);
      auto it = devices.find(peerId);
      if (it != devices.end()) d = it->second;
    }
    d.deviceId = peerId;
    d.deviceName = name;
    d.deviceType = deviceTypeFromString(type);
    d.publicKey = pubPem;
    d.aesKey = aesKeyB64;
    d.isTrusted = true;
    d.isOnline = true;
    if (tcpPort != 0) d.tcpPort = tcpPort;
    {
      std::lock_guard<std::mutex> lock(devMutex);
      devices[peerId] = d;
      lastSeen[peerId] = nowEpochSeconds();
      pendingRequests.erase(peerId);
      pendingRequestNonce.erase(peerId);
      pendingOutgoing.erase(peerId);
    }
    if (storage) {
      d.lastOnline = nowEpochSeconds();
      storage->addTrustedDevice(d);
    }
    emitPairResult(d, true);
    emitDeviceOnline(d);
    std::thread([this, d]() { syncWithPeer(d); }).detach();
    return ackJson(true).dump();
  }

  if (cmd == "pairReject") {
    Device d;
    {
      std::lock_guard<std::mutex> lock(devMutex);
      auto it = devices.find(peerId);
      if (it != devices.end()) d = it->second;
    }
    emitPairResult(d, false);
    return ackJson(true).dump();
  }

  return ackJson(false, "unknown cmd").dump();
}

// ---------- outgoing ----------

bool CoreImpl::sendFileToPeer(const Device& peer, const FileInfo& fi) {
  if (!running.load() || !transport) return false;

  json::Value probe = json::Value::object();
  probe.set("cmd", "fileProbe");
  probe.set("deviceId", selfId());
  probe.set("fileId", fi.fileId);
  probe.set("fileName", fi.fileName);
  probe.set("fileSize", fi.fileSize);
  std::string reply;
  if (!transport->sendJson(peer, signJson(probe, privateKeyPem), reply)) {
    emitError("连接失败: " + peer.deviceName);
    return false;
  }
  json::Value pv = json::Value::parse(reply);
  if (pv["exists"].asBool()) {
    emitTransferFinished(fi.fileId, fi.fileName, true); // MD5 秒传
    return true;
  }

  int resumeFrom = 0;
  json::Value resume = json::Value::object();
  resume.set("cmd", "fileResume");
  resume.set("deviceId", selfId());
  resume.set("fileId", fi.fileId);
  if (transport->sendJson(peer, signJson(resume, privateKeyPem), reply)) {
    json::Value rv = json::Value::parse(reply);
    resumeFrom = static_cast<int>(rv["chunksDone"].asInt(0));
  }

  uint64_t fileSize = static_cast<uint64_t>(fi.fileSize);
  size_t chunkTotal = (fileSize + kChunkSize - 1) / kChunkSize;
  if (chunkTotal == 0) chunkTotal = 1;
  if (resumeFrom >= static_cast<int>(chunkTotal)) {
    emitTransferFinished(fi.fileId, fi.fileName, true);
    return true;
  }

  json::Value h = json::Value::object();
  h.set("cmd", "fileAdd");
  h.set("deviceId", selfId());
  h.set("fileId", fi.fileId);
  h.set("fileName", fi.fileName);
  h.set("fileType", fileTypeToString(fi.fileType));
  h.set("fileSize", fi.fileSize);
  h.set("fileDuration", fi.duration);
  h.set("createTime", fi.createTime);
  h.set("chunkTotal", static_cast<int64_t>(chunkTotal));
  h.set("enc", "aes128-gcm");
  h.set("resumeFrom", static_cast<int64_t>(resumeFrom));

  std::string path = fi.cachePath;
  std::string keyB64 = peer.aesKey;

  std::ifstream in(path, std::ios::binary);
  if (!in) {
    emitError("本地文件不可读: " + fi.fileName);
    return false;
  }
  in.close();

  std::string ackReply;
  bool ok = transport->sendFile(
      peer, signJson(h, privateKeyPem), chunkTotal - static_cast<size_t>(resumeFrom),
      [this, path, keyB64, resumeFrom, fi, fileSize](size_t idx, std::vector<uint8_t>& out) -> bool {
        size_t globalIdx = static_cast<size_t>(resumeFrom) + idx;
        std::ifstream f(path, std::ios::binary);
        if (!f) return false;
        f.seekg(static_cast<std::streamoff>(globalIdx * kChunkSize));
        std::vector<uint8_t> plain(kChunkSize);
        f.read(reinterpret_cast<char*>(plain.data()), static_cast<std::streamsize>(plain.size()));
        std::streamsize got = f.gcount();
        plain.resize(static_cast<size_t>(got));
        if (encryptChunk(keyB64, plain, out) != "") return false;
        TransferProgress p;
        p.fileId = fi.fileId;
        p.fileName = fi.fileName;
        p.transferredBytes = static_cast<int64_t>(static_cast<size_t>(globalIdx + 1) * kChunkSize);
        p.totalBytes = static_cast<int64_t>(fileSize);
        p.percent = fileSize > 0 ? static_cast<double>(p.transferredBytes) / static_cast<double>(fileSize) : 1.0;
        if (p.percent > 1.0) p.percent = 1.0;
        emitTransferProgress(p);
        return true;
      },
      ackReply);

  json::Value av = json::Value::parse(ackReply);
  bool peerOk = av["ok"].asBool();
  emitTransferFinished(fi.fileId, fi.fileName, ok && peerOk);
  return ok && peerOk;
}

bool CoreImpl::sendDeleteToPeer(const Device& peer, const FileInfo& fi) {
  if (!running.load() || !transport) return false;
  json::Value v = json::Value::object();
  v.set("cmd", "fileDelete");
  v.set("deviceId", selfId());
  v.set("fileId", fi.fileId);
  std::string reply;
  return transport->sendJson(peer, signJson(v, privateKeyPem), reply);
}

void CoreImpl::syncWithPeer(const Device& peer) {
  if (!running.load() || !transport) return;
  json::Value list = json::Value::array();
  auto files = storage->listFiles();
  for (const auto& f : files) list.push(fileInfoToJson(f));
  json::Value v = json::Value::object();
  v.set("cmd", "syncAllFiles");
  v.set("deviceId", selfId());
  v.set("fileList", list);
  std::string reply;
  if (!transport->sendJson(peer, signJson(v, privateKeyPem), reply)) return;

  json::Value rv = json::Value::parse(reply);
  std::vector<std::string> missingIds;
  for (const auto& m : rv["missing"].asArray()) missingIds.push_back(m["fileId"].asString());
  if (missingIds.empty()) return;

  for (const auto& f : files) {
    bool needed = false;
    for (const auto& id : missingIds) {
      if (id == f.fileId) { needed = true; break; }
    }
    if (needed) sendFileToPeer(peer, f);
  }
}

void CoreImpl::syncWithAllTrusted() {
  std::vector<Device> targets;
  {
    std::lock_guard<std::mutex> lock(devMutex);
    for (const auto& kv : devices) {
      if (kv.second.isTrusted && kv.second.isOnline) targets.push_back(kv.second);
    }
  }
  for (const auto& d : targets) {
    std::thread([this, d]() { syncWithPeer(d); }).detach();
  }
}

void CoreImpl::markOfflineStale() {
  int64_t now = nowEpochSeconds();
  std::vector<Device> offline;
  {
    std::lock_guard<std::mutex> lock(devMutex);
    for (auto& kv : devices) {
      // Manually-added IP devices have no discovery heartbeat; keep them online.
      if (kv.first.rfind("manual_", 0) == 0) continue;
      auto it = lastSeen.find(kv.first);
      if (it != lastSeen.end() && it->second + kStaleTimeoutSec < now && kv.second.isOnline) {
        kv.second.isOnline = false;
        offline.push_back(kv.second);
      }
    }
  }
  for (const auto& d : offline) emitDeviceOffline(d);
}

void CoreImpl::announceLoop() {
  while (running.load()) {
    if (discovery) discovery->announce();
    markOfflineStale();
    std::this_thread::sleep_for(std::chrono::seconds(kAnnounceIntervalSec));
  }
}

// ---------- settings / cache ----------

void CoreImpl::applyRateLimit() {
  if (transport) transport->setMaxRateBps(settings.maxTransferRateBps);
}

void CoreImpl::applyCacheExpiry() {
  if (!storage) return;
  int hours = settings.cacheExpireHours;
  if (hours <= 0) return;
  auto files = storage->listFiles();
  for (auto& f : files) {
    if (f.expireTime.empty() && !f.createTime.empty()) {
      f.expireTime = addHoursToUtcString(f.createTime, hours);
      storage->upsertFile(f);
    }
  }
  std::string now = nowUtcString();
  for (auto& f : files) {
    if (!f.expireTime.empty() && f.expireTime <= now) {
      cache->removeFile(f.fileId, f.fileName);
      storage->removeFile(f.fileId);
      emitFileRemoved(f);
    }
  }
}

// ---------- Core facade ----------

Core::Core() : impl_(std::make_unique<CoreImpl>()) {}

Core::~Core() { stop(); }

Core& Core::instance() {
  static Core core;
  return core;
}

bool Core::configure(const CoreConfig& cfg) {
  impl_->cfg = cfg;
  if (impl_->cfg.dataDir.empty()) impl_->cfg.dataDir = "./localsend-data";
  if (impl_->cfg.deviceId.empty()) impl_->cfg.deviceId = loadOrCreateDeviceId(impl_->cfg.dataDir);
  if (impl_->cfg.deviceName.empty()) {
    char host[256] = {0};
    if (gethostname(host, sizeof(host)) == 0) impl_->cfg.deviceName = host;
    else impl_->cfg.deviceName = "localsend-device";
  }
  return true;
}

bool Core::start() {
  if (impl_->running.load()) return true;

  namespace fs = std::filesystem;
  std::error_code ec;
  fs::create_directories(impl_->cfg.dataDir + "/keys", ec);
  fs::create_directories(impl_->cfg.dataDir + "/cache", ec);

  std::string privPath = impl_->cfg.dataDir + "/keys/private.pem";
  std::string pubPath = impl_->cfg.dataDir + "/keys/public.pem";
  std::ifstream privIn(privPath);
  std::ifstream pubIn(pubPath);
  if (privIn && pubIn) {
    impl_->privateKeyPem.assign((std::istreambuf_iterator<char>(privIn)), std::istreambuf_iterator<char>());
    impl_->publicKeyPem.assign((std::istreambuf_iterator<char>(pubIn)), std::istreambuf_iterator<char>());
  } else {
    if (!Crypto::generateRsaKeyPair(impl_->publicKeyPem, impl_->privateKeyPem)) return false;
    std::ofstream privOut(privPath, std::ios::trunc);
    std::ofstream pubOut(pubPath, std::ios::trunc);
    privOut << impl_->privateKeyPem;
    pubOut << impl_->publicKeyPem;
    // Restrict the private key and its directory to the owner.
    ::chmod(privPath.c_str(), 0600);
    ::chmod((impl_->cfg.dataDir + "/keys").c_str(), 0700);
  }

  impl_->storage = std::make_unique<Storage>(impl_->cfg.dataDir + "/localsend.db");
  if (!impl_->storage->open()) return false;
  impl_->storage->loadSettings(impl_->settings);
  std::string cacheDir = impl_->settings.cacheDir.empty()
                             ? impl_->cfg.dataDir + "/cache"
                             : impl_->settings.cacheDir;
  fs::create_directories(cacheDir, ec);
  impl_->cache = std::make_unique<Cache>(cacheDir);
  impl_->cache->ensureDirs();

  impl_->transport = std::make_unique<Transport>();
  impl_->transport->setHandler([impl = impl_.get()](const std::string& peerId, const std::string& headerJson,
                                                    size_t chunkTotal, const Transport::ChunkReader& readChunk) {
    return impl->onIncoming(peerId, headerJson, chunkTotal, readChunk);
  });
  if (!impl_->transport->start(impl_->cfg.tcpPort)) {
    impl_->storage->close();
    return false;
  }
  impl_->applyRateLimit();

  for (const auto& td : impl_->storage->listTrustedDevices()) {
    std::lock_guard<std::mutex> lock(impl_->devMutex);
    Device d = td;
    d.isOnline = false;
    impl_->devices[td.deviceId] = d;
  }

  impl_->discovery = std::make_unique<Discovery>();
  Device self;
  self.deviceId = impl_->cfg.deviceId;
  self.deviceName = impl_->cfg.deviceName;
  self.deviceType = impl_->cfg.deviceType;
  self.publicKey = impl_->publicKeyPem;
  if (impl_->localIpCache.empty()) impl_->localIpCache = getDefaultLocalIp();
  self.ip = impl_->localIpCache;
  self.tcpPort = impl_->cfg.tcpPort;
  impl_->discovery->setTrustedChecker([impl = impl_.get()](const std::string& id) {
    return impl->storage && impl->storage->getTrustedDevice(id).has_value();
  });
  impl_->discovery->setCallback([impl = impl_.get()](const DiscoveryMessage& m) {
    impl->onDiscoveryMessage(m);
  });
  if (!impl_->discovery->start(self, impl_->cfg.multicastGroup, impl_->cfg.multicastPort)) {
    impl_->transport->stop();
    impl_->storage->close();
    return false;
  }

  impl_->running.store(true);
  impl_->discovery->announce();
  impl_->announcerThread = std::thread(&CoreImpl::announceLoop, impl_.get());
  impl_->applyCacheExpiry();
  return true;
}

void Core::stop() {
  if (!impl_->running.exchange(false)) return;
  if (impl_->announcerThread.joinable()) impl_->announcerThread.join();
  if (impl_->discovery) impl_->discovery->stop();
  if (impl_->transport) impl_->transport->stop();
  if (impl_->storage) impl_->storage->close();
}

void Core::setCallbacks(const Callbacks& cb) {
  std::lock_guard<std::mutex> lock(impl_->cbMutex);
  impl_->callbacks = cb;
}

void Core::setLocalIp(const std::string& ip) { impl_->localIpCache = ip; }

bool Core::addFile(const std::string& localPath, const std::string& displayName) {
  if (!impl_->running.load()) return false;
  std::string fileId = Crypto::md5File(localPath);
  if (fileId.empty()) return false;

  fs::path p(localPath);
  std::string name = displayName.empty() ? p.filename().string() : displayName;
  FileInfo fi;
  fi.fileId = fileId;
  fi.fileName = name;
  fi.fileType = detectFileType(name);
  fi.fileSize = static_cast<int64_t>(fs::file_size(p));
  fi.duration = 0;
  fi.createTime = nowUtcString();
  fi.isLocal = true;

  if (auto existing = impl_->storage->getFile(fileId)) {
    fi = *existing;
    fi.createTime = nowUtcString();
  } else {
    if (!impl_->cache->storeFile(fileId, name, localPath, fi.cachePath)) return false;
    impl_->storage->upsertFile(fi);
  }
  impl_->emitFileAdded(fi);

  std::vector<Device> targets;
  {
    std::lock_guard<std::mutex> lock(impl_->devMutex);
    for (const auto& kv : impl_->devices) {
      if (kv.second.isTrusted && kv.second.isOnline) targets.push_back(kv.second);
    }
  }
  for (const auto& d : targets) {
    std::thread([impl = impl_.get(), d, fi]() { impl->sendFileToPeer(d, fi); }).detach();
  }
  return true;
}

bool Core::removeFile(const std::string& fileId) {
  auto existing = impl_->storage->getFile(fileId);
  if (!existing) return false;
  impl_->cache->removeFile(existing->fileId, existing->fileName);
  impl_->storage->removeFile(fileId);
  impl_->emitFileRemoved(*existing);

  std::vector<Device> targets;
  {
    std::lock_guard<std::mutex> lock(impl_->devMutex);
    for (const auto& kv : impl_->devices) {
      if (kv.second.isTrusted && kv.second.isOnline) targets.push_back(kv.second);
    }
  }
  for (const auto& d : targets) {
    std::thread([impl = impl_.get(), d, fi = *existing]() { impl->sendDeleteToPeer(d, fi); }).detach();
  }
  return true;
}

std::vector<FileInfo> Core::listFiles() const {
  if (!impl_->storage) return {};
  return impl_->storage->listFiles();
}

std::vector<FileInfo> Core::listRemoteFiles() const {
  return {}; // file pool is mirrored; all files are local copies
}

std::vector<Device> Core::listDevices() const {
  std::vector<Device> out;
  std::lock_guard<std::mutex> lock(impl_->devMutex);
  for (const auto& kv : impl_->devices) out.push_back(kv.second);
  return out;
}

std::vector<Device> Core::listTrustedDevices() const {
  std::vector<Device> out;
  if (!impl_->storage) return out;
  for (const auto& d : impl_->storage->listTrustedDevices()) {
    std::lock_guard<std::mutex> lock(impl_->devMutex);
    auto it = impl_->devices.find(d.deviceId);
    if (it != impl_->devices.end()) {
      Device merged = d;
      merged.ip = it->second.ip;
      merged.tcpPort = it->second.tcpPort;
      merged.isOnline = it->second.isOnline;
      out.push_back(merged);
    } else {
      out.push_back(d);
    }
  }
  return out;
}

bool Core::requestPair(const std::string& deviceId) {
  Device target;
  {
    std::lock_guard<std::mutex> lock(impl_->devMutex);
    auto it = impl_->devices.find(deviceId);
    if (it != impl_->devices.end()) target = it->second;
  }
  if (target.deviceId.empty() || target.tcpPort == 0) return false;

  std::vector<uint8_t> nonceBytes;
  Crypto::generateAesKey(nonceBytes, 16);
  std::string nonce = Crypto::toBase64(nonceBytes);

  PairRequest req;
  req.deviceId = impl_->selfId();
  req.deviceName = impl_->cfg.deviceName;
  req.deviceType = impl_->cfg.deviceType;
  req.publicKey = impl_->publicKeyPem;
  req.ip = impl_->localIpCache.empty() ? getDefaultLocalIp() : impl_->localIpCache;
  req.tcpPort = impl_->cfg.tcpPort;
  req.nonce = nonce;

  {
    std::lock_guard<std::mutex> lock(impl_->devMutex);
    impl_->pendingOutgoing[deviceId] = {nonce, target.publicKey};
  }

  std::string reply;
  if (!impl_->transport->sendJson(target, Pairing::buildRequest(req), reply)) {
    {
      std::lock_guard<std::mutex> lock(impl_->devMutex);
      impl_->pendingOutgoing.erase(deviceId);
    }
    impl_->emitError("配对连接失败: " + target.deviceName);
    return false;
  }
  json::Value av = json::Value::parse(reply);
  return av["ok"].asBool();
}

bool Core::pairDevice(const std::string& deviceId, bool accept) {
  Device target;
  {
    std::lock_guard<std::mutex> lock(impl_->devMutex);
    auto it = impl_->devices.find(deviceId);
    if (it != impl_->devices.end()) target = it->second;
  }
  if (target.deviceId.empty() || target.tcpPort == 0) return false;

  if (!accept) {
    json::Value v = json::Value::parse(
        Pairing::buildReject(target.deviceId, "user rejected"));
    v.set("deviceId", impl_->selfId());
    std::string reply;
    impl_->transport->sendJson(target, v.dump(), reply);
    {
      std::lock_guard<std::mutex> lock(impl_->devMutex);
      impl_->pendingRequests.erase(target.deviceId);
      impl_->pendingRequestNonce.erase(target.deviceId);
    }
    impl_->emitPairResult(target, false);
    return true;
  }

  // We are the responder: generate AES key, encrypt to requester's public key.
  std::vector<uint8_t> aesKey;
  Crypto::generateAesKey(aesKey);
  std::vector<uint8_t> enc;
  if (!Crypto::rsaEncrypt(target.publicKey, aesKey, enc)) {
    impl_->emitError("配对加密失败: " + target.deviceName);
    return false;
  }
  std::string encB64 = Crypto::toBase64(enc);
  std::string nonce;
  {
    std::lock_guard<std::mutex> lock(impl_->devMutex);
    auto it = impl_->pendingRequestNonce.find(deviceId);
    if (it != impl_->pendingRequestNonce.end()) nonce = it->second;
  }
  json::Value v = json::Value::parse(Pairing::buildAccept(
      target.deviceId, impl_->publicKeyPem, encB64, impl_->cfg.deviceName,
      deviceTypeToString(impl_->cfg.deviceType), impl_->cfg.tcpPort, nonce));
  v.set("deviceId", impl_->selfId());

  Device self;
  self.deviceId = impl_->selfId();
  self.deviceName = impl_->cfg.deviceName;
  self.deviceType = impl_->cfg.deviceType;
  self.ip = impl_->localIpCache.empty() ? getDefaultLocalIp() : impl_->localIpCache;
  self.tcpPort = impl_->cfg.tcpPort;

  std::string reply;
  if (!impl_->transport->sendJson(target, signJson(v, impl_->privateKeyPem), reply)) {
    impl_->emitError("配对连接失败: " + target.deviceName);
    return false;
  }
  json::Value av = json::Value::parse(reply);
  bool peerOk = av["ok"].asBool();
  if (!peerOk) {
    impl_->emitError("对方拒绝配对: " + target.deviceName);
    return false;
  }

  target.aesKey = Crypto::toBase64(aesKey);
  target.isTrusted = true;
  target.isOnline = true;
  target.lastOnline = nowEpochSeconds();
  {
    std::lock_guard<std::mutex> lock(impl_->devMutex);
    impl_->devices[target.deviceId] = target;
    impl_->lastSeen[target.deviceId] = nowEpochSeconds();
    impl_->pendingRequests.erase(target.deviceId);
    impl_->pendingRequestNonce.erase(target.deviceId);
  }
  impl_->storage->addTrustedDevice(target);
  impl_->emitPairResult(target, true);
  impl_->emitDeviceOnline(target);
  std::thread([impl = impl_.get(), target]() { impl->syncWithPeer(target); }).detach();
  return true;
}

bool Core::removeDevice(const std::string& deviceId) {
  {
    std::lock_guard<std::mutex> lock(impl_->devMutex);
    impl_->devices.erase(deviceId);
    impl_->lastSeen.erase(deviceId);
  }
  if (impl_->storage) impl_->storage->removeTrustedDevice(deviceId);
  return true;
}

bool Core::connectDeviceByIp(const std::string& ip, uint16_t port) {
  Device d;
  d.deviceId = "manual_" + ip + "_" + std::to_string(port);
  d.deviceName = "手动设备 " + ip;
  d.deviceType = DeviceType::Unknown;
  d.ip = ip;
  d.tcpPort = port;
  d.isOnline = true;
  d.isTrusted = false;
  {
    std::lock_guard<std::mutex> lock(impl_->devMutex);
    impl_->devices[d.deviceId] = d;
    impl_->lastSeen[d.deviceId] = nowEpochSeconds();
  }
  impl_->emitDeviceFound(d);
  return true;
}

bool Core::rescan() {
  if (impl_->discovery) impl_->discovery->announce();
  return true;
}

Settings& Core::settings() { return impl_->settings; }
const Settings& Core::settings() const { return impl_->settings; }

void Core::saveSettings() {
  if (impl_->storage) impl_->storage->saveSettings(impl_->settings);
  impl_->applyRateLimit();
  if (impl_->settings.cacheExpireHours > 0) impl_->applyCacheExpiry();
}

std::string Core::cacheDir() const {
  return impl_->cache ? impl_->cache->cacheDir() : std::string();
}

bool Core::setCacheDir(const std::string& dir) {
  if (dir.empty() || !impl_->running.load() || !impl_->cache || !impl_->storage) return false;
  if (dir == impl_->cache->cacheDir()) return true;

  std::error_code ec;
  fs::create_directories(dir, ec);
  if (ec) return false;

  // Move existing cached files into the new staging directory and update paths.
  for (auto& f : impl_->storage->listFiles()) {
    if (f.cachePath.empty() || !fs::exists(f.cachePath)) continue;
    std::string newPath = dir + "/" + f.fileId + "_" + sanitizeFileName(f.fileName);
    fs::rename(f.cachePath, newPath, ec);
    if (!ec) {
      f.cachePath = newPath;
      impl_->storage->upsertFile(f);
    }
  }

  impl_->cache = std::make_unique<Cache>(dir);
  impl_->cache->ensureDirs();
  impl_->settings.cacheDir = dir;
  impl_->storage->saveSettings(impl_->settings);
  return true;
}

std::string Core::deviceId() const { return impl_->cfg.deviceId; }
std::string Core::deviceName() const { return impl_->cfg.deviceName; }
std::string Core::localIp() const { return impl_->localIpCache; }
std::string Core::localPublicKey() const { return impl_->publicKeyPem; }

bool Core::hasFile(const std::string& fileId) const {
  return impl_->storage && impl_->storage->getFile(fileId).has_value();
}

} // namespace localsend
