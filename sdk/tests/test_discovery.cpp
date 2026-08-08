#include <atomic>
#include <chrono>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "localsend/discovery.h"
#include "test_util.h"

using namespace localsend;

namespace {

struct Collector {
  std::mutex m;
  std::vector<DiscoveryMessage> messages;
  void add(const DiscoveryMessage& msg) {
    std::lock_guard<std::mutex> lock(m);
    messages.push_back(msg);
  }
  size_t count(const std::string& deviceId, const std::string& msgType) {
    std::lock_guard<std::mutex> lock(m);
    size_t n = 0;
    for (const auto& msg : messages) {
      if (msg.deviceId == deviceId && msg.msgType == msgType) ++n;
    }
    return n;
  }
};

} // namespace

static void run() {
  const std::string group = "224.0.0.1";
  const uint16_t port = 53317;

  Device selfA;
  selfA.deviceId = "test-device-a";
  selfA.deviceName = "A";
  selfA.deviceType = DeviceType::Ubuntu;
  selfA.ip = "127.0.0.1";
  selfA.tcpPort = 53320;

  Device selfB;
  selfB.deviceId = "test-device-b";
  selfB.deviceName = "B";
  selfB.deviceType = DeviceType::HarmonyOS;
  selfB.ip = "127.0.0.1";
  selfB.tcpPort = 53321;

  Discovery a;
  Discovery b;
  Collector colA;
  Collector colB;

  a.setCallback([&colA](const DiscoveryMessage& m) { colA.add(m); });
  b.setCallback([&colB](const DiscoveryMessage& m) { colB.add(m); });
  a.setTrustedChecker([](const std::string&) { return false; });
  b.setTrustedChecker([](const std::string&) { return false; });

  CHECK(a.start(selfA, group, port));
  CHECK(b.start(selfB, group, port));

  // Let both announce repeatedly for a couple seconds.
  auto t0 = std::chrono::steady_clock::now();
  while (std::chrono::steady_clock::now() - t0 < std::chrono::seconds(2)) {
    a.announce();
    b.announce();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  // A should have received B's announce and B's ack (acks now use multicast).
  CHECK(colA.count("test-device-b", "deviceAnnounce") > 0);
  CHECK(colA.count("test-device-b", "deviceAck") > 0);
  CHECK(colB.count("test-device-a", "deviceAnnounce") > 0);
  CHECK(colB.count("test-device-a", "deviceAck") > 0);

  a.stop();
  b.stop();
}

TEST_MAIN(run)
