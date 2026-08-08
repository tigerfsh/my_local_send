#pragma once

#include <string>
#include <vector>

#include "types.h"

namespace localsend {

class LOCALSEND_API Cache {
public:
  explicit Cache(std::string cacheDir);

  bool ensureDirs();
  std::string cacheDir() const { return cacheDir_; }

  // Move/copy a received temp file into cache with a deterministic name: <fileId>_<sanitized name>
  std::string buildCachePath(const std::string& fileId, const std::string& fileName) const;
  bool storeFile(const std::string& fileId, const std::string& fileName, const std::string& srcPath,
                 std::string& finalPath);
  bool storeBytes(const std::string& fileId, const std::string& fileName, const std::vector<uint8_t>& data,
                  std::string& finalPath);
  bool removeFile(const std::string& fileId, const std::string& fileName);

  // Remove files whose expire_time is in the past and non-empty.
  int cleanupExpired(const std::vector<FileInfo>& files);
  void clearAll();

private:
  std::string cacheDir_;
};

} // namespace localsend
