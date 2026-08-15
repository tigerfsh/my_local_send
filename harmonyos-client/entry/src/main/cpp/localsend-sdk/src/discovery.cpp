#include "localsend/discovery.h"

#include <arpa/inet.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include <cstring>

#include "json.hpp"

namespace localsend {

Discovery::Discovery() = default;
Discovery::~Discovery() { stop(); }

bool Discovery::start(const Device& self, const std::string& group, uint16_t port) {
  if (running_.load()) return false;
  self_ = self;
  group_ = group;
  port_ = port;

  sock_ = socket(AF_INET, SOCK_DGRAM, 0);
  if (sock_ < 0) return false;
  int reuse = 1;
  setsockopt(sock_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
#ifdef SO_REUSEPORT
  setsockopt(sock_, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(reuse));
#endif
  // Wake the receive loop periodically so stop() can join promptly.
  timeval tv{};
  tv.tv_sec = 1;
  tv.tv_usec = 0;
  setsockopt(sock_, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port_);
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  if (bind(sock_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
    close(sock_);
    sock_ = -1;
    return false;
  }

  ip_mreq mreq{};
  inet_pton(AF_INET, group_.c_str(), &mreq.imr_multiaddr);
  mreq.imr_interface.s_addr = htonl(INADDR_ANY);
  if (setsockopt(sock_, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) {
    close(sock_);
    sock_ = -1;
    return false;
  }

  sendSock_ = socket(AF_INET, SOCK_DGRAM, 0);
  if (sendSock_ < 0) {
    close(sock_);
    sock_ = -1;
    return false;
  }
  unsigned char ttl = 4;
  setsockopt(sendSock_, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl));
  int loop = 1;
  setsockopt(sendSock_, IPPROTO_IP, IP_MULTICAST_LOOP, &loop, sizeof(loop));

  running_.store(true);
  thread_ = std::thread(&Discovery::receiveLoop, this);
  return true;
}

void Discovery::stop() {
  if (!running_.exchange(false)) return;
  if (sock_ >= 0) {
    shutdown(sock_, SHUT_RDWR);
    close(sock_);
    sock_ = -1;
  }
  if (sendSock_ >= 0) {
    close(sendSock_);
    sendSock_ = -1;
  }
  if (thread_.joinable()) thread_.join();
}

void Discovery::setCallback(DiscoveryCallback cb) {
  std::lock_guard<std::mutex> lock(cbMutex_);
  cb_ = std::move(cb);
}

void Discovery::setTrustedChecker(std::function<bool(const std::string&)> checker) {
  std::lock_guard<std::mutex> lock(cbMutex_);
  trustedChecker_ = std::move(checker);
}

bool Discovery::sendMulticast(const std::string& payload) {
  if (sendSock_ < 0) return false;
  sockaddr_in dst{};
  dst.sin_family = AF_INET;
  dst.sin_port = htons(port_);
  inet_pton(AF_INET, group_.c_str(), &dst.sin_addr);
  ssize_t n = sendto(sendSock_, payload.data(), payload.size(), 0,
                     reinterpret_cast<sockaddr*>(&dst), sizeof(dst));
  return n == static_cast<ssize_t>(payload.size());
}

bool Discovery::sendUnicast(const std::string& payload, const std::string& addr, uint16_t port) {
  if (sendSock_ < 0) return false;
  sockaddr_in dst{};
  dst.sin_family = AF_INET;
  dst.sin_port = htons(port);
  inet_pton(AF_INET, addr.c_str(), &dst.sin_addr);
  ssize_t n = sendto(sendSock_, payload.data(), payload.size(), 0,
                     reinterpret_cast<sockaddr*>(&dst), sizeof(dst));
  return n == static_cast<ssize_t>(payload.size());
}

void Discovery::announce() {
  json::Value v = json::Value::object();
  v.set("msgType", "deviceAnnounce");
  v.set("deviceId", self_.deviceId);
  v.set("deviceType", deviceTypeToString(self_.deviceType));
  v.set("deviceName", self_.deviceName);
  v.set("version", LOCALSEND_VERSION);
  v.set("publicKey", self_.publicKey);
  v.set("ip", self_.ip);
  v.set("tcpPort", static_cast<int64_t>(self_.tcpPort));
  sendMulticast(v.dump());
}

void Discovery::receiveLoop() {
  char buf[65536];
  while (running_.load()) {
    sockaddr_in src{};
    socklen_t srcLen = sizeof(src);
    ssize_t n = recvfrom(sock_, buf, sizeof(buf), 0, reinterpret_cast<sockaddr*>(&src), &srcLen);
    if (n <= 0) {
      if (!running_.load()) break;
      continue;
    }
    std::string data(buf, static_cast<size_t>(n));
    char ipStr[INET_ADDRSTRLEN] = {0};
    inet_ntop(AF_INET, &src.sin_addr, ipStr, sizeof(ipStr));
    handlePacket(data, ipStr);
  }
}

void Discovery::handlePacket(const std::string& data, const std::string& srcIp) {
  json::Value v = json::Value::parse(data);
  std::string msgType = v["msgType"].asString();
  if (msgType.empty()) return;
  std::string deviceId = v["deviceId"].asString();
  if (deviceId.empty() || deviceId == self_.deviceId) return;

  DiscoveryMessage msg;
  msg.msgType = msgType;
  msg.deviceId = deviceId;
  msg.deviceName = v["deviceName"].asString();
  msg.deviceType = deviceTypeFromString(v["deviceType"].asString());
  msg.publicKey = v["publicKey"].asString();
  msg.ip = v["ip"].asString();
  if (msg.ip.empty()) msg.ip = srcIp;
  msg.tcpPort = static_cast<uint16_t>(v["tcpPort"].asInt(0));

  std::function<bool(const std::string&)> checker;
  {
    std::lock_guard<std::mutex> lock(cbMutex_);
    checker = trustedChecker_;
  }
  msg.isTrusted = checker ? checker(deviceId) : false;

  if (msgType == "deviceAnnounce") {
    json::Value ack = json::Value::object();
    ack.set("msgType", "deviceAck");
    ack.set("targetDeviceId", deviceId);
    ack.set("isTrusted", msg.isTrusted);
    ack.set("deviceId", self_.deviceId);
    ack.set("deviceName", self_.deviceName);
    ack.set("deviceType", deviceTypeToString(self_.deviceType));
    ack.set("ip", self_.ip);
    ack.set("tcpPort", static_cast<int64_t>(self_.tcpPort));
    // Ack is sent via multicast so that all group members receive it; this also
    // stays reliable when multiple instances run on the same host (SO_REUSEPORT
    // load-balancing can otherwise drop unicast acks between local sockets).
    sendMulticast(ack.dump());
  }

  DiscoveryCallback cb;
  {
    std::lock_guard<std::mutex> lock(cbMutex_);
    cb = cb_;
  }
  if (cb) cb(msg);
}

} // namespace localsend
