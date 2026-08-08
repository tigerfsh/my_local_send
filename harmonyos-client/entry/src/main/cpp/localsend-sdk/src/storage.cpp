#include "localsend/storage.h"

#include <sqlite3.h>

#include <cstdio>
#include <cstdlib>

namespace localsend {

namespace {

FileInfo rowToFileInfo(sqlite3_stmt* stmt) {
  FileInfo fi;
  if (const unsigned char* t = sqlite3_column_text(stmt, 0)) fi.fileId = reinterpret_cast<const char*>(t);
  if (const unsigned char* t = sqlite3_column_text(stmt, 1)) fi.fileName = reinterpret_cast<const char*>(t);
  fi.fileType = fileTypeFromString(sqlite3_column_text(stmt, 2)
                                       ? reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2))
                                       : "unknown");
  fi.fileSize = sqlite3_column_int64(stmt, 3);
  fi.duration = sqlite3_column_int64(stmt, 4);
  if (const unsigned char* t = sqlite3_column_text(stmt, 5)) fi.cachePath = reinterpret_cast<const char*>(t);
  if (const unsigned char* t = sqlite3_column_text(stmt, 6)) fi.createTime = reinterpret_cast<const char*>(t);
  if (const unsigned char* t = sqlite3_column_text(stmt, 7)) fi.expireTime = reinterpret_cast<const char*>(t);
  fi.isLocal = sqlite3_column_int(stmt, 8) != 0;
  return fi;
}

Device rowToDevice(sqlite3_stmt* stmt) {
  Device d;
  if (const unsigned char* t = sqlite3_column_text(stmt, 0)) d.deviceId = reinterpret_cast<const char*>(t);
  if (const unsigned char* t = sqlite3_column_text(stmt, 1)) d.deviceName = reinterpret_cast<const char*>(t);
  d.deviceType = deviceTypeFromString(sqlite3_column_text(stmt, 2)
                                          ? reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2))
                                          : "unknown");
  if (const unsigned char* t = sqlite3_column_text(stmt, 3)) d.publicKey = reinterpret_cast<const char*>(t);
  if (const unsigned char* t = sqlite3_column_text(stmt, 4)) d.aesKey = reinterpret_cast<const char*>(t);
  d.lastOnline = sqlite3_column_int64(stmt, 5);
  d.isTrusted = true;
  return d;
}

} // namespace

Storage::Storage(std::string dbPath) : dbPath_(std::move(dbPath)) {}

Storage::~Storage() { close(); }

bool Storage::open() {
  if (db_) return true;
  if (sqlite3_open(dbPath_.c_str(), &db_) != SQLITE_OK) {
    if (db_) sqlite3_close(db_);
    db_ = nullptr;
    return false;
  }
  sqlite3_busy_timeout(db_, 3000);

  const char* schema =
      "CREATE TABLE IF NOT EXISTS file_info ("
      "  file_id TEXT PRIMARY KEY,"
      "  file_name TEXT NOT NULL,"
      "  file_type TEXT NOT NULL,"
      "  file_size INTEGER NOT NULL,"
      "  duration INTEGER DEFAULT 0,"
      "  cache_path TEXT NOT NULL,"
      "  create_time TEXT NOT NULL,"
      "  expire_time TEXT,"
      "  is_local INTEGER DEFAULT 1"
      ");"
      "CREATE TABLE IF NOT EXISTS trusted_device ("
      "  device_id TEXT PRIMARY KEY,"
      "  device_name TEXT NOT NULL,"
      "  device_type TEXT NOT NULL,"
      "  public_key TEXT NOT NULL,"
      "  aes_key TEXT,"
      "  last_online INTEGER DEFAULT 0"
      ");"
      "CREATE TABLE IF NOT EXISTS transfer_state ("
      "  file_id TEXT PRIMARY KEY,"
      "  chunks_done INTEGER DEFAULT 0,"
      "  chunk_total INTEGER DEFAULT 0,"
      "  peer_id TEXT"
      ");"
      "CREATE TABLE IF NOT EXISTS settings ("
      "  key TEXT PRIMARY KEY,"
      "  value TEXT"
      ");";
  if (sqlite3_exec(db_, schema, nullptr, nullptr, nullptr) != SQLITE_OK) {
    sqlite3_close(db_);
    db_ = nullptr;
    return false;
  }
  return true;
}

void Storage::close() {
  if (db_) {
    sqlite3_close(db_);
    db_ = nullptr;
  }
}

bool Storage::upsertFile(const FileInfo& fi) {
  if (!db_) return false;
  static const char* sql =
      "INSERT OR REPLACE INTO file_info (file_id, file_name, file_type, file_size, duration, cache_path, create_time, expire_time, is_local) "
      "VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9);";
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
  sqlite3_bind_text(stmt, 1, fi.fileId.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 2, fi.fileName.c_str(), -1, SQLITE_TRANSIENT);
  std::string ft = fileTypeToString(fi.fileType);
  sqlite3_bind_text(stmt, 3, ft.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int64(stmt, 4, fi.fileSize);
  sqlite3_bind_int64(stmt, 5, fi.duration);
  sqlite3_bind_text(stmt, 6, fi.cachePath.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 7, fi.createTime.c_str(), -1, SQLITE_TRANSIENT);
  if (fi.expireTime.empty()) {
    sqlite3_bind_null(stmt, 8);
  } else {
    sqlite3_bind_text(stmt, 8, fi.expireTime.c_str(), -1, SQLITE_TRANSIENT);
  }
  sqlite3_bind_int(stmt, 9, fi.isLocal ? 1 : 0);
  bool ok = sqlite3_step(stmt) == SQLITE_DONE;
  sqlite3_finalize(stmt);
  return ok;
}

bool Storage::removeFile(const std::string& fileId) {
  if (!db_) return false;
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db_, "DELETE FROM file_info WHERE file_id = ?1;", -1, &stmt, nullptr) != SQLITE_OK) return false;
  sqlite3_bind_text(stmt, 1, fileId.c_str(), -1, SQLITE_TRANSIENT);
  bool ok = sqlite3_step(stmt) == SQLITE_DONE;
  sqlite3_finalize(stmt);
  if (ok) removeTransferState(fileId);
  return ok;
}

std::vector<FileInfo> Storage::listFiles() const {
  std::vector<FileInfo> out;
  if (!db_) return out;
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db_, "SELECT file_id, file_name, file_type, file_size, duration, cache_path, create_time, expire_time, is_local FROM file_info;",
                         -1, &stmt, nullptr) != SQLITE_OK) return out;
  while (sqlite3_step(stmt) == SQLITE_ROW) out.push_back(rowToFileInfo(stmt));
  sqlite3_finalize(stmt);
  return out;
}

std::optional<FileInfo> Storage::getFile(const std::string& fileId) const {
  if (!db_) return std::nullopt;
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db_, "SELECT file_id, file_name, file_type, file_size, duration, cache_path, create_time, expire_time, is_local FROM file_info WHERE file_id = ?1;",
                         -1, &stmt, nullptr) != SQLITE_OK) return std::nullopt;
  sqlite3_bind_text(stmt, 1, fileId.c_str(), -1, SQLITE_TRANSIENT);
  std::optional<FileInfo> out;
  if (sqlite3_step(stmt) == SQLITE_ROW) out = rowToFileInfo(stmt);
  sqlite3_finalize(stmt);
  return out;
}

bool Storage::addTrustedDevice(const Device& d) {
  if (!db_) return false;
  static const char* sql =
      "INSERT OR REPLACE INTO trusted_device (device_id, device_name, device_type, public_key, aes_key, last_online) "
      "VALUES (?1, ?2, ?3, ?4, ?5, ?6);";
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
  sqlite3_bind_text(stmt, 1, d.deviceId.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 2, d.deviceName.c_str(), -1, SQLITE_TRANSIENT);
  std::string dt = deviceTypeToString(d.deviceType);
  sqlite3_bind_text(stmt, 3, dt.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 4, d.publicKey.c_str(), -1, SQLITE_TRANSIENT);
  if (d.aesKey.empty()) {
    sqlite3_bind_null(stmt, 5);
  } else {
    sqlite3_bind_text(stmt, 5, d.aesKey.c_str(), -1, SQLITE_TRANSIENT);
  }
  sqlite3_bind_int64(stmt, 6, d.lastOnline);
  bool ok = sqlite3_step(stmt) == SQLITE_DONE;
  sqlite3_finalize(stmt);
  return ok;
}

bool Storage::updateTrustedDevice(const Device& d) { return addTrustedDevice(d); }

bool Storage::removeTrustedDevice(const std::string& deviceId) {
  if (!db_) return false;
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db_, "DELETE FROM trusted_device WHERE device_id = ?1;", -1, &stmt, nullptr) != SQLITE_OK) return false;
  sqlite3_bind_text(stmt, 1, deviceId.c_str(), -1, SQLITE_TRANSIENT);
  bool ok = sqlite3_step(stmt) == SQLITE_DONE;
  sqlite3_finalize(stmt);
  return ok;
}

std::vector<Device> Storage::listTrustedDevices() const {
  std::vector<Device> out;
  if (!db_) return out;
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db_, "SELECT device_id, device_name, device_type, public_key, aes_key, last_online FROM trusted_device;",
                         -1, &stmt, nullptr) != SQLITE_OK) return out;
  while (sqlite3_step(stmt) == SQLITE_ROW) out.push_back(rowToDevice(stmt));
  sqlite3_finalize(stmt);
  return out;
}

std::optional<Device> Storage::getTrustedDevice(const std::string& deviceId) const {
  if (!db_) return std::nullopt;
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db_, "SELECT device_id, device_name, device_type, public_key, aes_key, last_online FROM trusted_device WHERE device_id = ?1;",
                         -1, &stmt, nullptr) != SQLITE_OK) return std::nullopt;
  sqlite3_bind_text(stmt, 1, deviceId.c_str(), -1, SQLITE_TRANSIENT);
  std::optional<Device> out;
  if (sqlite3_step(stmt) == SQLITE_ROW) out = rowToDevice(stmt);
  sqlite3_finalize(stmt);
  return out;
}

bool Storage::saveTransferState(const std::string& fileId, int chunksDone, int chunkTotal, const std::string& peerId) {
  if (!db_) return false;
  static const char* sql =
      "INSERT OR REPLACE INTO transfer_state (file_id, chunks_done, chunk_total, peer_id) VALUES (?1, ?2, ?3, ?4);";
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
  sqlite3_bind_text(stmt, 1, fileId.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(stmt, 2, chunksDone);
  sqlite3_bind_int(stmt, 3, chunkTotal);
  sqlite3_bind_text(stmt, 4, peerId.c_str(), -1, SQLITE_TRANSIENT);
  bool ok = sqlite3_step(stmt) == SQLITE_DONE;
  sqlite3_finalize(stmt);
  return ok;
}

bool Storage::loadTransferState(const std::string& fileId, int& chunksDone, int& chunkTotal, std::string& peerId) const {
  chunksDone = 0;
  chunkTotal = 0;
  peerId.clear();
  if (!db_) return false;
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db_, "SELECT chunks_done, chunk_total, peer_id FROM transfer_state WHERE file_id = ?1;",
                         -1, &stmt, nullptr) != SQLITE_OK) return false;
  sqlite3_bind_text(stmt, 1, fileId.c_str(), -1, SQLITE_TRANSIENT);
  bool found = false;
  if (sqlite3_step(stmt) == SQLITE_ROW) {
    found = true;
    chunksDone = sqlite3_column_int(stmt, 0);
    chunkTotal = sqlite3_column_int(stmt, 1);
    if (const unsigned char* t = sqlite3_column_text(stmt, 2)) peerId = reinterpret_cast<const char*>(t);
  }
  sqlite3_finalize(stmt);
  return found;
}

bool Storage::removeTransferState(const std::string& fileId) {
  if (!db_) return false;
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db_, "DELETE FROM transfer_state WHERE file_id = ?1;", -1, &stmt, nullptr) != SQLITE_OK) return false;
  sqlite3_bind_text(stmt, 1, fileId.c_str(), -1, SQLITE_TRANSIENT);
  bool ok = sqlite3_step(stmt) == SQLITE_DONE;
  sqlite3_finalize(stmt);
  return ok;
}

bool Storage::saveSettings(const Settings& s) {
  if (!db_) return false;
  static const char* sql = "INSERT OR REPLACE INTO settings (key, value) VALUES (?1, ?2);";
  auto put = [&](const std::string& k, const std::string& v) -> bool {
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_text(stmt, 1, k.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, v.c_str(), -1, SQLITE_TRANSIENT);
    bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return ok;
  };
  char buf[32];
  std::snprintf(buf, sizeof(buf), "%d", s.floatWindowLocked ? 1 : 0);
  bool ok = put("floatWindowLocked", buf);
  std::snprintf(buf, sizeof(buf), "%d", s.autoCollapseMs);
  ok = ok && put("autoCollapseMs", buf);
  std::snprintf(buf, sizeof(buf), "%d", s.cacheExpireHours);
  ok = ok && put("cacheExpireHours", buf);
  std::snprintf(buf, sizeof(buf), "%lld", static_cast<long long>(s.maxTransferRateBps));
  ok = ok && put("maxTransferRateBps", buf);
  return ok;
}

bool Storage::loadSettings(Settings& s) const {
  if (!db_) return false;
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db_, "SELECT key, value FROM settings;", -1, &stmt, nullptr) != SQLITE_OK) return false;
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    std::string key = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
    std::string val = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
    if (key == "floatWindowLocked") s.floatWindowLocked = val == "1";
    else if (key == "autoCollapseMs") s.autoCollapseMs = std::atoi(val.c_str());
    else if (key == "cacheExpireHours") s.cacheExpireHours = std::atoi(val.c_str());
    else if (key == "maxTransferRateBps") s.maxTransferRateBps = std::atoll(val.c_str());
  }
  sqlite3_finalize(stmt);
  return true;
}

} // namespace localsend
