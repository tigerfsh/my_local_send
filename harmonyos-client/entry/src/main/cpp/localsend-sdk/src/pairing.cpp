#include "localsend/pairing.h"

#include "json.hpp"

namespace localsend {

std::string Pairing::buildRequest(const PairRequest& req) {
  json::Value v = json::Value::object();
  v.set("cmd", "pairRequest");
  v.set("deviceId", req.deviceId);
  v.set("deviceName", req.deviceName);
  v.set("deviceType", deviceTypeToString(req.deviceType));
  v.set("publicKey", req.publicKey);
  v.set("ip", req.ip);
  v.set("tcpPort", static_cast<int64_t>(req.tcpPort));
  v.set("nonce", req.nonce);
  return v.dump();
}

bool Pairing::parseRequest(const std::string& jsonText, PairRequest& out) {
  json::Value v = json::Value::parse(jsonText);
  if (v["cmd"].asString() != "pairRequest") return false;
  out.deviceId = v["deviceId"].asString();
  out.deviceName = v["deviceName"].asString();
  out.deviceType = deviceTypeFromString(v["deviceType"].asString());
  out.publicKey = v["publicKey"].asString();
  out.ip = v["ip"].asString();
  out.tcpPort = static_cast<uint16_t>(v["tcpPort"].asInt(0));
  out.nonce = v["nonce"].asString();
  return !out.deviceId.empty();
}

std::string Pairing::buildAccept(const std::string& targetDeviceId, const std::string& publicKeyPem,
                                 const std::string& aesKeyEncryptedBase64, const std::string& deviceName,
                                 const std::string& deviceType, uint16_t tcpPort, const std::string& nonce) {
  json::Value v = json::Value::object();
  v.set("cmd", "pairAccept");
  v.set("targetDeviceId", targetDeviceId);
  v.set("publicKey", publicKeyPem);
  v.set("aesKeyEnc", aesKeyEncryptedBase64);
  v.set("deviceName", deviceName);
  v.set("deviceType", deviceType);
  v.set("tcpPort", static_cast<int64_t>(tcpPort));
  v.set("nonce", nonce);
  return v.dump();
}

std::string Pairing::buildReject(const std::string& targetDeviceId, const std::string& reason) {
  json::Value v = json::Value::object();
  v.set("cmd", "pairReject");
  v.set("targetDeviceId", targetDeviceId);
  v.set("reason", reason);
  return v.dump();
}

bool Pairing::parseAccept(const std::string& jsonText, std::string& targetDeviceId, std::string& publicKeyPem,
                          std::string& aesKeyEncryptedBase64, std::string& deviceName, std::string& deviceType,
                          uint16_t& tcpPort) {
  json::Value v = json::Value::parse(jsonText);
  if (v["cmd"].asString() != "pairAccept") return false;
  targetDeviceId = v["targetDeviceId"].asString();
  publicKeyPem = v["publicKey"].asString();
  aesKeyEncryptedBase64 = v["aesKeyEnc"].asString();
  deviceName = v["deviceName"].asString();
  deviceType = v["deviceType"].asString();
  tcpPort = static_cast<uint16_t>(v["tcpPort"].asInt(0));
  return !targetDeviceId.empty();
}

} // namespace localsend
