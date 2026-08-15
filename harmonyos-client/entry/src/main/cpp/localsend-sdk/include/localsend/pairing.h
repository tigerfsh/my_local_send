#pragma once

#include <string>

#include "types.h"

namespace localsend {

struct PairRequest {
  std::string deviceId;
  std::string deviceName;
  DeviceType deviceType = DeviceType::Unknown;
  std::string publicKey;
  std::string ip;
  uint16_t tcpPort = 0;
  std::string nonce;
};

// Builds and parses pairing protocol messages.
class LOCALSEND_API Pairing {
public:
  static std::string buildRequest(const PairRequest& req);
  static bool parseRequest(const std::string& json, PairRequest& out);
  static std::string buildAccept(const std::string& targetDeviceId, const std::string& publicKeyPem,
                                 const std::string& aesKeyEncryptedBase64, const std::string& deviceName,
                                 const std::string& deviceType, uint16_t tcpPort, const std::string& nonce);
  static std::string buildReject(const std::string& targetDeviceId, const std::string& reason = "");
  static bool parseAccept(const std::string& json, std::string& targetDeviceId, std::string& publicKeyPem,
                          std::string& aesKeyEncryptedBase64, std::string& deviceName, std::string& deviceType,
                          uint16_t& tcpPort);
};

} // namespace localsend
