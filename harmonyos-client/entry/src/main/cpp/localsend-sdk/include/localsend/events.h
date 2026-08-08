#pragma once

#include <functional>
#include <string>

#include "types.h"

namespace localsend {

struct Callbacks {
  std::function<void(const Device&)> onDeviceFound;
  std::function<void(const Device&)> onDeviceOnline;
  std::function<void(const Device&)> onDeviceOffline;
  std::function<void(const Device&)> onPairRequest;
  std::function<void(const Device&, bool)> onPairResult;
  std::function<void(const FileInfo&)> onFileAdded;
  std::function<void(const FileInfo&)> onFileRemoved;
  std::function<void(const FileInfo&)> onFileReceiveStarted;
  std::function<void(const TransferProgress&)> onTransferProgress;
  std::function<void(const std::string& fileId, const std::string& fileName, bool success)> onTransferFinished;
  std::function<void(const std::string& message)> onError;
};

} // namespace localsend
