#include "bridge_common.h"

#include "localsend/localsend.h"

namespace lsbridge {

napi_value Start(napi_env env, napi_callback_info info) {
  size_t argc = 3;
  napi_value args[3];
  napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
  if (argc < 3) return MakeBool(env, false);

  std::string dataDir = GetUtf8(env, args[0]);
  int tcpPort = GetInt(env, args[1]);
  std::string deviceName = GetUtf8(env, args[2]);

  localsend::CoreConfig cfg;
  cfg.deviceName = deviceName;
  cfg.dataDir = dataDir;
  cfg.tcpPort = static_cast<uint16_t>(tcpPort);
  cfg.deviceType = localsend::DeviceType::HarmonyOS;
  localsend::Core::instance().configure(cfg);
  bool ok = localsend::Core::instance().start();
  return MakeBool(env, ok);
}

napi_value Stop(napi_env env, napi_callback_info info) {
  (void)env;
  (void)info;
  localsend::Core::instance().stop();
  return MakeBool(env, true);
}

napi_value AddFile(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1];
  napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
  if (argc < 1) return MakeBool(env, false);
  std::string path = GetUtf8(env, args[0]);
  bool ok = localsend::Core::instance().addFile(path);
  return MakeBool(env, ok);
}

napi_value RemoveFile(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1];
  napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
  if (argc < 1) return MakeBool(env, false);
  std::string fileId = GetUtf8(env, args[0]);
  bool ok = localsend::Core::instance().removeFile(fileId);
  return MakeBool(env, ok);
}

napi_value RequestPair(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1];
  napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
  if (argc < 1) return MakeBool(env, false);
  std::string deviceId = GetUtf8(env, args[0]);
  bool ok = localsend::Core::instance().requestPair(deviceId);
  return MakeBool(env, ok);
}

napi_value PairDevice(napi_env env, napi_callback_info info) {
  size_t argc = 2;
  napi_value args[2];
  napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
  if (argc < 2) return MakeBool(env, false);
  std::string deviceId = GetUtf8(env, args[0]);
  bool accept = GetBool(env, args[1]);
  bool ok = localsend::Core::instance().pairDevice(deviceId, accept);
  return MakeBool(env, ok);
}

napi_value RemoveDevice(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1];
  napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
  if (argc < 1) return MakeBool(env, false);
  std::string deviceId = GetUtf8(env, args[0]);
  bool ok = localsend::Core::instance().removeDevice(deviceId);
  return MakeBool(env, ok);
}

napi_value ConnectByIp(napi_env env, napi_callback_info info) {
  size_t argc = 2;
  napi_value args[2];
  napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
  if (argc < 2) return MakeBool(env, false);
  std::string ip = GetUtf8(env, args[0]);
  int port = GetInt(env, args[1]);
  bool ok = localsend::Core::instance().connectDeviceByIp(ip, static_cast<uint16_t>(port));
  return MakeBool(env, ok);
}

napi_value OnEvent(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1];
  napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
  if (argc < 1) return MakeBool(env, false);

  if (!InitEventChannel(env, args[0])) return MakeBool(env, false);

  localsend::Callbacks cb;
  cb.onFileAdded = [](const localsend::FileInfo& f) {
    PostEvent("{\"event\":\"fileAdded\",\"fileId\":\"" + f.fileId + "\",\"fileName\":\"" + f.fileName + "\"}");
  };
  cb.onFileRemoved = [](const localsend::FileInfo& f) {
    PostEvent("{\"event\":\"fileRemoved\",\"fileId\":\"" + f.fileId + "\"}");
  };
  cb.onPairRequest = [](const localsend::Device& d) {
    PostEvent("{\"event\":\"pairRequest\",\"deviceId\":\"" + d.deviceId +
              "\",\"deviceName\":\"" + d.deviceName + "\"}");
  };
  cb.onPairResult = [](const localsend::Device& d, bool accepted) {
    PostEvent("{\"event\":\"pairResult\",\"deviceId\":\"" + d.deviceId +
              "\",\"accepted\":" + std::string(accepted ? "true" : "false") + "}");
  };
  cb.onDeviceFound = [](const localsend::Device&) {
    PostEvent("{\"event\":\"devicesChanged\"}");
  };
  cb.onDeviceOnline = [](const localsend::Device&) {
    PostEvent("{\"event\":\"devicesChanged\"}");
  };
  cb.onDeviceOffline = [](const localsend::Device&) {
    PostEvent("{\"event\":\"devicesChanged\"}");
  };
  cb.onTransferFinished = [](const std::string& fileId, const std::string& fileName, bool ok) {
    PostEvent("{\"event\":\"transferFinished\",\"fileId\":\"" + fileId +
              "\",\"fileName\":\"" + fileName + "\",\"ok\":" + std::string(ok ? "true" : "false") + "}");
  };
  cb.onError = [](const std::string& message) {
    PostEvent("{\"event\":\"error\",\"message\":\"" + message + "\"}");
  };
  localsend::Core::instance().setCallbacks(cb);
  return MakeBool(env, true);
}

} // namespace lsbridge
