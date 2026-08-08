#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "events.h"
#include "export.h"
#include "types.h"

namespace localsend {

struct CoreConfig {
  std::string appName = "localsend";
  std::string deviceId;
  std::string deviceName;
  DeviceType deviceType = DeviceType::Ubuntu;
  std::string dataDir;
  uint16_t tcpPort = 53318;
  std::string multicastGroup = "224.0.0.1";
  uint16_t multicastPort = 53317;
};

class CoreImpl;

class LOCALSEND_API Core {
public:
  static Core& instance();

  Core();
  ~Core();
  Core(const Core&) = delete;
  Core& operator=(const Core&) = delete;

  bool configure(const CoreConfig& cfg);
  bool start();
  void stop();

  void setCallbacks(const Callbacks& cb);
  void setLocalIp(const std::string& ip);

  bool addFile(const std::string& localPath, const std::string& displayName = "");
  bool removeFile(const std::string& fileId);
  std::vector<FileInfo> listFiles() const;
  std::vector<FileInfo> listRemoteFiles() const;

  std::vector<Device> listDevices() const;
  std::vector<Device> listTrustedDevices() const;
  bool requestPair(const std::string& deviceId);
  bool pairDevice(const std::string& deviceId, bool accept);
  bool removeDevice(const std::string& deviceId);
  bool connectDeviceByIp(const std::string& ip, uint16_t port);
  bool rescan();

  Settings& settings();
  const Settings& settings() const;
  void saveSettings();

  std::string deviceId() const;
  std::string deviceName() const;
  std::string localIp() const;
  std::string localPublicKey() const;
  bool hasFile(const std::string& fileId) const;

private:
  std::unique_ptr<CoreImpl> impl_;
};

} // namespace localsend
