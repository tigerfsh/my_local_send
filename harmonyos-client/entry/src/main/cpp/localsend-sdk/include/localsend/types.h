#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "export.h"

namespace localsend {

enum class DeviceType : int { Unknown = 0, HarmonyOS = 1, Ubuntu = 2 };
enum class FileType : int { Unknown = 0, Image = 1, Video = 2, Doc = 3, Other = 4 };

struct Device {
  std::string deviceId;
  std::string deviceName;
  DeviceType deviceType = DeviceType::Unknown;
  std::string ip;
  uint16_t tcpPort = 0;
  std::string publicKey;
  std::string aesKey;
  bool isTrusted = false;
  bool isOnline = false;
  int64_t lastOnline = 0;
  bool isLocal = true;
};

struct FileInfo {
  std::string fileId;
  std::string fileName;
  FileType fileType = FileType::Unknown;
  int64_t fileSize = 0;
  int64_t duration = 0;
  std::string cachePath;
  std::string createTime;
  std::string expireTime;
  bool isLocal = true;
};

struct TransferProgress {
  std::string fileId;
  std::string fileName;
  int64_t transferredBytes = 0;
  int64_t totalBytes = 0;
  double percent = 0.0;
};

struct Settings {
  bool floatWindowLocked = false;
  int autoCollapseMs = 5000;
  int cacheExpireHours = 0;
  int64_t maxTransferRateBps = 0;
};

LOCALSEND_API std::string deviceTypeToString(DeviceType t);
LOCALSEND_API DeviceType deviceTypeFromString(const std::string& s);
LOCALSEND_API std::string fileTypeToString(FileType t);
LOCALSEND_API FileType fileTypeFromString(const std::string& s);
LOCALSEND_API FileType detectFileType(const std::string& fileName);

LOCALSEND_API std::string nowUtcString();
LOCALSEND_API int64_t nowEpochSeconds();
LOCALSEND_API std::string formatFileSize(int64_t bytes);
LOCALSEND_API std::string sanitizeFileName(const std::string& name);

} // namespace localsend
