#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include "types.h"

namespace localsend {

class Transport {
public:
  using ChunkReader = std::function<bool(size_t index, std::vector<uint8_t>& data)>;
  using Handler = std::function<std::string(const std::string& peerId, const std::string& headerJson,
                                            size_t chunkTotal, const ChunkReader& readChunk)>;

  Transport();
  ~Transport();
  Transport(const Transport&) = delete;
  Transport& operator=(const Transport&) = delete;

  bool start(uint16_t port);
  void stop();
  uint16_t port() const { return port_; }
  void setHandler(Handler h);

  // Send a JSON-only message, returns reply JSON (or empty on failure).
  bool sendJson(const Device& peer, const std::string& headerJson, std::string& replyJson);

  // Send a JSON header followed by chunk frames fetched lazily via getChunk.
  // getChunk(i, data) must fill data with the (already encrypted) i-th chunk.
  bool sendFile(const Device& peer, const std::string& headerJson, size_t chunkTotal,
                const std::function<bool(size_t, std::vector<uint8_t>&)>& getChunk, std::string& replyJson);

  void setMaxRateBps(int64_t bps) { maxRateBps_.store(bps); }
  int64_t maxRateBps() const { return maxRateBps_.load(); }

private:
  void acceptLoop();
  void handleConnection(int fd, const std::string& ip, uint16_t port);
  bool readExact(int fd, uint8_t* buf, size_t n);
  bool readFrame(int fd, std::vector<uint8_t>& out);
  bool writeFrame(int fd, const std::vector<uint8_t>& data);
  bool writeAll(int fd, const uint8_t* data, size_t n);
  int connectTo(const Device& peer);
  void throttle(size_t bytes);
  void trackFd(int fd);
  void untrackFd(int fd);

  int listenFd_ = -1;
  uint16_t port_ = 53318;
  std::atomic<bool> running_{false};
  std::thread acceptThread_;
  std::vector<std::thread> workers_;
  std::mutex workersMutex_;
  std::mutex fdsMutex_;
  std::set<int> activeFds_;
  Handler handler_;
  std::atomic<int64_t> maxRateBps_{0};
};

} // namespace localsend
