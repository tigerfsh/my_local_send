#include "localsend/cache.h"

#include <sys/stat.h>
#include <unistd.h>

#include <cerrno>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>

#include "localsend/crypto.h"

namespace localsend {

namespace fs = std::filesystem;

Cache::Cache(std::string cacheDir) : cacheDir_(std::move(cacheDir)) {}

bool Cache::ensureDirs() {
  std::error_code ec;
  fs::create_directories(cacheDir_, ec);
  return !ec;
}

std::string Cache::buildCachePath(const std::string& fileId, const std::string& fileName) const {
  return cacheDir_ + "/" + fileId + "_" + sanitizeFileName(fileName);
}

bool Cache::storeFile(const std::string& fileId, const std::string& fileName, const std::string& srcPath,
                      std::string& finalPath) {
  if (!ensureDirs()) return false;
  finalPath = buildCachePath(fileId, fileName);
  std::error_code ec;
  fs::copy_file(srcPath, finalPath, fs::copy_options::overwrite_existing, ec);
  return !ec;
}

bool Cache::storeBytes(const std::string& fileId, const std::string& fileName,
                       const std::vector<uint8_t>& data, std::string& finalPath) {
  if (!ensureDirs()) return false;
  finalPath = buildCachePath(fileId, fileName);
  std::ofstream out(finalPath, std::ios::binary | std::ios::trunc);
  if (!out) return false;
  out.write(reinterpret_cast<const char*>(data.data()),
            static_cast<std::streamsize>(data.size()));
  return out.good();
}

bool Cache::removeFile(const std::string& fileId, const std::string& fileName) {
  std::string path = buildCachePath(fileId, fileName);
  if (::unlink(path.c_str()) != 0 && errno != ENOENT) return false;
  return true;
}

int Cache::cleanupExpired(const std::vector<FileInfo>& files) {
  std::string now = nowUtcString();
  int removed = 0;
  for (const auto& f : files) {
    if (f.expireTime.empty() || f.expireTime > now) continue;
    std::string path = buildCachePath(f.fileId, f.fileName);
    if (::unlink(path.c_str()) == 0 || errno == ENOENT) ++removed;
  }
  return removed;
}

void Cache::clearAll() {
  std::error_code ec;
  for (const auto& entry : fs::directory_iterator(cacheDir_, ec)) {
    if (entry.is_regular_file(ec)) fs::remove(entry.path(), ec);
  }
}

} // namespace localsend
