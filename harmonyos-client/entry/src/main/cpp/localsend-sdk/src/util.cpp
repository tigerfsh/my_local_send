#include "localsend/types.h"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdio>
#include <ctime>
#include <string>

namespace localsend {

std::string deviceTypeToString(DeviceType t) {
  switch (t) {
    case DeviceType::HarmonyOS: return "harmonyos";
    case DeviceType::Ubuntu: return "ubuntu";
    default: return "unknown";
  }
}

DeviceType deviceTypeFromString(const std::string& s) {
  if (s == "harmonyos") return DeviceType::HarmonyOS;
  if (s == "ubuntu") return DeviceType::Ubuntu;
  return DeviceType::Unknown;
}

std::string fileTypeToString(FileType t) {
  switch (t) {
    case FileType::Image: return "image";
    case FileType::Video: return "video";
    case FileType::Doc: return "doc";
    case FileType::Other: return "other";
    default: return "unknown";
  }
}

FileType fileTypeFromString(const std::string& s) {
  if (s == "image") return FileType::Image;
  if (s == "video") return FileType::Video;
  if (s == "doc") return FileType::Doc;
  if (s == "other") return FileType::Other;
  return FileType::Unknown;
}

FileType detectFileType(const std::string& fileName) {
  std::string lower = fileName;
  std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  static const char* images[] = {"jpg", "jpeg", "png", "gif", "bmp", "webp", "svg", "heic"};
  static const char* videos[] = {"mp4", "mkv", "mov", "avi", "webm", "flv", "wmv", "ts"};
  auto dot = lower.find_last_of('.');
  std::string ext = (dot == std::string::npos) ? "" : lower.substr(dot + 1);
  if (!ext.empty()) {
    for (const char* e : images) if (ext == e) return FileType::Image;
    for (const char* e : videos) if (ext == e) return FileType::Video;
  }
  return FileType::Doc;
}

std::string nowUtcString() {
  std::time_t t = std::time(nullptr);
  std::tm tm{};
  gmtime_r(&t, &tm);
  char buf[32];
  std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d", tm.tm_year + 1900,
                tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
  return std::string(buf);
}

int64_t nowEpochSeconds() {
  return static_cast<int64_t>(std::chrono::system_clock::now().time_since_epoch().count() /
                              std::chrono::system_clock::period::den);
}

std::string formatFileSize(int64_t bytes) {
  char buf[64];
  if (bytes < 1024) {
    std::snprintf(buf, sizeof(buf), "%lld B", static_cast<long long>(bytes));
  } else if (bytes < 1024 * 1024) {
    std::snprintf(buf, sizeof(buf), "%.1f KB", bytes / 1024.0);
  } else if (bytes < 1024LL * 1024 * 1024) {
    std::snprintf(buf, sizeof(buf), "%.1f MB", bytes / (1024.0 * 1024.0));
  } else {
    std::snprintf(buf, sizeof(buf), "%.2f GB", bytes / (1024.0 * 1024.0 * 1024.0));
  }
  return std::string(buf);
}

std::string sanitizeFileName(const std::string& name) {
  std::string out = name;
  for (char& c : out) {
    if (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' ||
        c == '<' || c == '>' || c == '|' || c == '\0') {
      c = '_';
    }
  }
  if (out.empty()) out = "file";
  if (out.size() > 200) out = out.substr(out.size() - 200);
  return out;
}

} // namespace localsend
