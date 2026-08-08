#include <cstdio>
#include <cstdlib>
#include <string>
#include <unistd.h>

#include "localsend/storage.h"
#include "test_util.h"

using namespace localsend;

static void run() {
  std::string dbPath = std::string("/tmp/lstest_storage_") + std::to_string(getpid()) + ".db";
  Storage s(dbPath);
  CHECK(s.open());

  FileInfo f;
  f.fileId = "abc123md5";
  f.fileName = "demo.png";
  f.fileType = FileType::Image;
  f.fileSize = 4096;
  f.duration = 0;
  f.cachePath = "/tmp/lstest_demo.png";
  f.createTime = "2026-08-08 00:00:00";
  CHECK(s.upsertFile(f));

  auto got = s.getFile("abc123md5");
  CHECK(got.has_value());
  CHECK_EQ(got->fileName, std::string("demo.png"));
  CHECK_EQ(got->fileType, FileType::Image);
  CHECK_EQ(got->fileSize, static_cast<int64_t>(4096));

  CHECK_EQ(s.listFiles().size(), static_cast<size_t>(1));

  Device d;
  d.deviceId = "dev-1";
  d.deviceName = "Phone";
  d.deviceType = DeviceType::HarmonyOS;
  d.publicKey = "PEM...";
  d.aesKey = "b64aes";
  d.lastOnline = 12345;
  CHECK(s.addTrustedDevice(d));
  auto td = s.getTrustedDevice("dev-1");
  CHECK(td.has_value());
  CHECK_EQ(td->deviceName, std::string("Phone"));
  CHECK_EQ(td->deviceType, DeviceType::HarmonyOS);
  CHECK_EQ(td->aesKey, std::string("b64aes"));

  d.lastOnline = 99999;
  CHECK(s.updateTrustedDevice(d));
  CHECK_EQ(s.getTrustedDevice("dev-1")->lastOnline, static_cast<int64_t>(99999));

  CHECK(s.saveTransferState("file1", 2, 5, "dev-1"));
  int done = 0, total = 0;
  std::string peer;
  CHECK(s.loadTransferState("file1", done, total, peer));
  CHECK_EQ(done, 2);
  CHECK_EQ(total, 5);
  CHECK_EQ(peer, std::string("dev-1"));

  Settings st;
  st.floatWindowLocked = true;
  st.autoCollapseMs = 8000;
  st.maxTransferRateBps = 123456;
  CHECK(s.saveSettings(st));
  Settings loaded;
  CHECK(s.loadSettings(loaded));
  CHECK(loaded.floatWindowLocked);
  CHECK_EQ(loaded.autoCollapseMs, 8000);
  CHECK_EQ(loaded.maxTransferRateBps, static_cast<int64_t>(123456));

  CHECK(s.removeTransferState("file1"));
  CHECK(!s.loadTransferState("file1", done, total, peer));

  CHECK(s.removeFile("abc123md5"));
  CHECK(!s.getFile("abc123md5").has_value());

  CHECK(s.removeTrustedDevice("dev-1"));
  CHECK(!s.getTrustedDevice("dev-1").has_value());

  s.close();
  std::remove(dbPath.c_str());
}

TEST_MAIN(run)
