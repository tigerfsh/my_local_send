#include "localsend/transport.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <chrono>
#include <cstring>
#include <thread>

#include "json.hpp"

namespace localsend {

namespace {
constexpr uint32_t kMaxFrameSize = 64 * 1024 * 1024; // 64MB safety cap
constexpr int kTimeoutSec = 60;

void setTimeouts(int fd, int sec) {
  timeval tv{};
  tv.tv_sec = sec;
  setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
  setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
}

uint32_t be32(const uint8_t* p) {
  return (static_cast<uint32_t>(p[0]) << 24) | (static_cast<uint32_t>(p[1]) << 16) |
         (static_cast<uint32_t>(p[2]) << 8) | static_cast<uint32_t>(p[3]);
}

void putBe32(uint8_t* p, uint32_t v) {
  p[0] = static_cast<uint8_t>(v >> 24);
  p[1] = static_cast<uint8_t>(v >> 16);
  p[2] = static_cast<uint8_t>(v >> 8);
  p[3] = static_cast<uint8_t>(v);
}

} // namespace

Transport::Transport() = default;
Transport::~Transport() { stop(); }

bool Transport::start(uint16_t port) {
  if (running_.load()) return false;
  listenFd_ = socket(AF_INET, SOCK_STREAM, 0);
  if (listenFd_ < 0) return false;
  int reuse = 1;
  setsockopt(listenFd_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  addr.sin_port = htons(port);
  if (bind(listenFd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0 ||
      listen(listenFd_, 32) < 0) {
    close(listenFd_);
    listenFd_ = -1;
    return false;
  }
  port_ = port;
  running_.store(true);
  acceptThread_ = std::thread(&Transport::acceptLoop, this);
  return true;
}

void Transport::stop() {
  if (!running_.exchange(false)) return;
  if (listenFd_ >= 0) {
    close(listenFd_);
    listenFd_ = -1;
  }
  {
    std::lock_guard<std::mutex> lock(fdsMutex_);
    for (int fd : activeFds_) close(fd);
    activeFds_.clear();
  }
  if (acceptThread_.joinable()) acceptThread_.join();
  for (auto& t : workers_) {
    if (t.joinable()) t.join();
  }
  workers_.clear();
}
void Transport::setHandler(Handler h) { handler_ = std::move(h); }

void Transport::trackFd(int fd) {
  std::lock_guard<std::mutex> lock(fdsMutex_);
  activeFds_.insert(fd);
}

void Transport::untrackFd(int fd) {
  std::lock_guard<std::mutex> lock(fdsMutex_);
  activeFds_.erase(fd);
}

void Transport::acceptLoop() {
  pollfd pfd{};
  pfd.fd = listenFd_;
  pfd.events = POLLIN;
  while (running_.load()) {
    // Poll with a timeout instead of blocking accept(): closing a listening
    // socket does not reliably wake a blocked accept() on Linux.
    int rc = poll(&pfd, 1, 200);
    if (rc < 0) {
      if (!running_.load()) break;
      continue;
    }
    if (rc == 0) continue;
    if (!(pfd.revents & POLLIN)) continue;

    sockaddr_in client{};
    socklen_t len = sizeof(client);
    int fd = accept(listenFd_, reinterpret_cast<sockaddr*>(&client), &len);
    if (fd < 0) {
      if (!running_.load()) break;
      continue;
    }
    setTimeouts(fd, kTimeoutSec);
    char ipStr[INET_ADDRSTRLEN] = {0};
    inet_ntop(AF_INET, &client.sin_addr, ipStr, sizeof(ipStr));
    trackFd(fd);
    std::lock_guard<std::mutex> lock(workersMutex_);
    workers_.emplace_back(&Transport::handleConnection, this, fd, std::string(ipStr),
                          static_cast<uint16_t>(ntohs(client.sin_port)));
  }
}

bool Transport::readExact(int fd, uint8_t* buf, size_t n) {
  size_t got = 0;
  while (got < n) {
    ssize_t r = recv(fd, buf + got, n - got, 0);
    if (r <= 0) return false;
    got += static_cast<size_t>(r);
  }
  return true;
}

bool Transport::writeAll(int fd, const uint8_t* data, size_t n) {
  size_t sent = 0;
  while (sent < n) {
    ssize_t r = send(fd, data + sent, n - sent, MSG_NOSIGNAL);
    if (r <= 0) return false;
    sent += static_cast<size_t>(r);
  }
  return true;
}

bool Transport::readFrame(int fd, std::vector<uint8_t>& out) {
  uint8_t lenBuf[4];
  if (!readExact(fd, lenBuf, 4)) return false;
  uint32_t len = be32(lenBuf);
  if (len == 0 || len > kMaxFrameSize) return false;
  out.resize(len);
  return readExact(fd, out.data(), len);
}

bool Transport::writeFrame(int fd, const std::vector<uint8_t>& data) {
  if (data.empty()) return false;
  uint8_t lenBuf[4];
  putBe32(lenBuf, static_cast<uint32_t>(data.size()));
  return writeAll(fd, lenBuf, 4) && writeAll(fd, data.data(), data.size());
}

int Transport::connectTo(const Device& peer) {
  int fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) return -1;
  setTimeouts(fd, kTimeoutSec);
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(peer.tcpPort);
  if (inet_pton(AF_INET, peer.ip.c_str(), &addr.sin_addr) != 1) {
    close(fd);
    return -1;
  }
  if (connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
    close(fd);
    return -1;
  }
  return fd;
}

void Transport::throttle(size_t bytes) {
  int64_t rate = maxRateBps_.load();
  if (rate <= 0 || bytes == 0) return;
  // Aim for 1 chunk interval so sending is paced to `rate` bytes/second.
  int64_t targetMs = static_cast<int64_t>(bytes) * 1000 / rate;
  std::this_thread::sleep_for(std::chrono::milliseconds(targetMs));
}

void Transport::handleConnection(int fd, const std::string& ip, uint16_t port) {
  std::vector<uint8_t> header;
  bool ok = readFrame(fd, header);
  std::string reply;
  if (ok) {
    std::string headerJson(reinterpret_cast<const char*>(header.data()), header.size());
    json::Value hv = json::Value::parse(headerJson);
    std::string peerId = hv["deviceId"].asString();
    size_t chunkTotal = static_cast<size_t>(hv["chunkTotal"].asInt(0));

    auto readChunk = [this, fd](size_t index, std::vector<uint8_t>& data) -> bool {
      (void)index;
      return readFrame(fd, data);
    };

    Handler h = handler_;
    if (h) {
      reply = h(peerId, headerJson, chunkTotal, readChunk);
    }
    if (!reply.empty()) {
      std::vector<uint8_t> replyBytes(reply.begin(), reply.end());
      writeFrame(fd, replyBytes);
    }
  }
  (void)ip;
  (void)port;
  close(fd);
  untrackFd(fd);
}

bool Transport::sendJson(const Device& peer, const std::string& headerJson, std::string& replyJson) {
  int fd = connectTo(peer);
  if (fd < 0) return false;
  bool ok = false;
  std::vector<uint8_t> header(headerJson.begin(), headerJson.end());
  if (writeFrame(fd, header)) {
    std::vector<uint8_t> reply;
    if (readFrame(fd, reply)) {
      replyJson.assign(reinterpret_cast<const char*>(reply.data()), reply.size());
      ok = true;
    }
  }
  close(fd);
  return ok;
}

bool Transport::sendFile(const Device& peer, const std::string& headerJson, size_t chunkTotal,
                         const std::function<bool(size_t, std::vector<uint8_t>&)>& getChunk,
                         std::string& replyJson) {
  int fd = connectTo(peer);
  if (fd < 0) return false;
  bool ok = false;
  std::vector<uint8_t> header(headerJson.begin(), headerJson.end());
  if (writeFrame(fd, header)) {
    bool allSent = true;
    for (size_t i = 0; i < chunkTotal; ++i) {
      std::vector<uint8_t> chunk;
      if (!getChunk(i, chunk) || !writeFrame(fd, chunk)) {
        allSent = false;
        break;
      }
      throttle(chunk.size());
    }
    if (allSent) {
      std::vector<uint8_t> reply;
      if (readFrame(fd, reply)) {
        replyJson.assign(reinterpret_cast<const char*>(reply.data()), reply.size());
        ok = true;
      }
    }
  }
  close(fd);
  return ok;
}

} // namespace localsend
