#pragma once

#include <atomic>
#include <functional>
#include <mutex>
#include <string>
#include <thread>

#include "types.h"

namespace localsend {

struct DiscoveryMessage {
  std::string msgType;
  std::string deviceId;
  std::string deviceName;
  DeviceType deviceType = DeviceType::Unknown;
  std::string publicKey;
  std::string ip;
  uint16_t tcpPort = 0;
  bool isTrusted = false;
};

using DiscoveryCallback = std::function<void(const DiscoveryMessage&)>;

class LOCALSEND_API Discovery {
public:
  Discovery();
  ~Discovery();
  Discovery(const Discovery&) = delete;
  Discovery& operator=(const Discovery&) = delete;

  bool start(const Device& self, const std::string& group, uint16_t port);
  void stop();
  void announce();
  void setCallback(DiscoveryCallback cb);
  void setTrustedChecker(std::function<bool(const std::string&)> checker);

private:
  void receiveLoop();
  void handlePacket(const std::string& data, const std::string& srcIp);
  bool sendMulticast(const std::string& payload);
  bool sendUnicast(const std::string& payload, const std::string& addr, uint16_t port);

  int sock_ = -1;
  int sendSock_ = -1;
  std::string group_;
  uint16_t port_ = 53317;
  Device self_;
  std::thread thread_;
  std::atomic<bool> running_{false};
  std::mutex cbMutex_;
  DiscoveryCallback cb_;
  std::function<bool(const std::string&)> trustedChecker_;
};

} // namespace localsend
