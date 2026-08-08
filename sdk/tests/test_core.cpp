#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include <unistd.h>

#include "localsend/core.h"
#include "localsend/crypto.h"
#include "test_util.h"

using namespace localsend;

namespace fs = std::filesystem;

namespace {

void writeRandomFile(const std::string& path, size_t size, uint8_t seed) {
  std::ofstream out(path, std::ios::binary);
  std::vector<uint8_t> buf(64 * 1024);
  uint8_t v = seed;
  size_t written = 0;
  while (written < size) {
    size_t n = std::min(buf.size(), size - written);
    for (size_t i = 0; i < n; ++i) buf[i] = static_cast<uint8_t>(v++);
    out.write(reinterpret_cast<const char*>(buf.data()), static_cast<std::streamsize>(n));
    written += n;
  }
}

bool waitFor(const std::function<bool()>& pred, int seconds) {
  auto t0 = std::chrono::steady_clock::now();
  while (std::chrono::steady_clock::now() - t0 < std::chrono::seconds(seconds)) {
    if (pred()) return true;
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
  return false;
}

struct State {
  std::mutex m;
  std::vector<std::string> pairRequests;
  std::vector<std::string> addedFiles;
  std::vector<std::string> removedFiles;
  std::atomic<bool> pairResultA{false};
};

} // namespace

static void run() {
  std::string dir = std::string("/tmp/lstest_core_") + std::to_string(getpid());
  fs::remove_all(dir);
  fs::create_directories(dir + "/a");
  fs::create_directories(dir + "/b");

  std::string srcFile = dir + "/payload.bin";
  writeRandomFile(srcFile, 5 * 1024 * 1024 + 12345, 77);
  std::string fileId = Crypto::md5File(srcFile);
  CHECK(!fileId.empty());

  Core a;
  Core b;

  CoreConfig ca;
  ca.deviceId = "test-a";
  ca.deviceName = "A";
  ca.deviceType = DeviceType::Ubuntu;
  ca.dataDir = dir + "/a";
  ca.tcpPort = 53320;

  CoreConfig cb;
  cb.deviceId = "test-b";
  cb.deviceName = "B";
  cb.deviceType = DeviceType::HarmonyOS;
  cb.dataDir = dir + "/b";
  cb.tcpPort = 53321;

  CHECK(a.configure(ca));
  CHECK(b.configure(cb));
  a.setLocalIp("127.0.0.1");
  b.setLocalIp("127.0.0.1");

  State st;
  Callbacks cba;
  cba.onPairResult = [&st](const Device&, bool ok) { st.pairResultA.store(ok); };
  Callbacks cbb;
  cbb.onPairRequest = [&b, &st](const Device& d) {
    std::lock_guard<std::mutex> lock(st.m);
    st.pairRequests.push_back(d.deviceId);
    b.pairDevice(d.deviceId, true);
  };
  cbb.onFileAdded = [&st](const FileInfo& f) {
    std::lock_guard<std::mutex> lock(st.m);
    st.addedFiles.push_back(f.fileId);
  };
  cbb.onFileRemoved = [&st](const FileInfo& f) {
    std::lock_guard<std::mutex> lock(st.m);
    st.removedFiles.push_back(f.fileId);
  };
  a.setCallbacks(cba);
  b.setCallbacks(cbb);

  CHECK(a.start());
  CHECK(b.start());

  // Wait for mutual discovery.
  CHECK(waitFor([&]() {
    auto da = a.listDevices();
    auto db = b.listDevices();
    bool aSeesB = false, bSeesA = false;
    for (const auto& d : da) if (d.deviceId == "test-b") aSeesB = true;
    for (const auto& d : db) if (d.deviceId == "test-a") bSeesA = true;
    return aSeesB && bSeesA;
  }, 8));

  // A requests pairing, B auto-accepts.
  CHECK(a.requestPair("test-b"));
  CHECK(waitFor([&]() {
    auto t = a.listTrustedDevices();
    for (const auto& d : t) if (d.deviceId == "test-b") return true;
    return false;
  }, 8));
  CHECK(st.pairResultA.load());

  // Add file on A -> should arrive at B with same MD5.
  CHECK(a.addFile(srcFile));
  CHECK(waitFor([&]() {
    std::lock_guard<std::mutex> lock(st.m);
    return std::find(st.addedFiles.begin(), st.addedFiles.end(), fileId) != st.addedFiles.end();
  }, 15));

  auto filesB = b.listFiles();
  bool found = false;
  std::string recvPath;
  for (const auto& f : filesB) {
    if (f.fileId == fileId) {
      found = true;
      recvPath = f.cachePath;
    }
  }
  CHECK(found);
  CHECK(!recvPath.empty());
  CHECK(fs::exists(recvPath));
  CHECK_EQ(Crypto::md5File(recvPath), fileId);

  // Delete on A -> should be removed on B.
  CHECK(a.removeFile(fileId));
  CHECK(waitFor([&]() {
    std::lock_guard<std::mutex> lock(st.m);
    return std::find(st.removedFiles.begin(), st.removedFiles.end(), fileId) != st.removedFiles.end();
  }, 10));
  CHECK(!b.hasFile(fileId));

  a.stop();
  b.stop();
  fs::remove_all(dir);
}

TEST_MAIN(run)
