#pragma once

#include <optional>
#include <string>
#include <vector>

#include "types.h"

struct sqlite3;

namespace localsend {

class LOCALSEND_API Storage {
public:
  explicit Storage(std::string dbPath);
  ~Storage();
  Storage(const Storage&) = delete;
  Storage& operator=(const Storage&) = delete;

  bool open();
  void close();
  bool isOpen() const { return db_ != nullptr; }

  bool upsertFile(const FileInfo& fi);
  bool removeFile(const std::string& fileId);
  std::vector<FileInfo> listFiles() const;
  std::optional<FileInfo> getFile(const std::string& fileId) const;

  bool addTrustedDevice(const Device& d);
  bool updateTrustedDevice(const Device& d);
  bool removeTrustedDevice(const std::string& deviceId);
  std::vector<Device> listTrustedDevices() const;
  std::optional<Device> getTrustedDevice(const std::string& deviceId) const;

  bool saveTransferState(const std::string& fileId, int chunksDone, int chunkTotal, const std::string& peerId);
  bool loadTransferState(const std::string& fileId, int& chunksDone, int& chunkTotal, std::string& peerId) const;
  bool removeTransferState(const std::string& fileId);

  bool saveSettings(const Settings& s);
  bool loadSettings(Settings& s) const;

private:
  std::string dbPath_;
  sqlite3* db_ = nullptr;
};

} // namespace localsend
