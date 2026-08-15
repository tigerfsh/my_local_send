#include "bridge_common.h"

#include "json.hpp"
#include "localsend/localsend.h"

namespace lsbridge {

namespace {

std::string makeEvent(const std::string& name) {
  localsend::json::Value v = localsend::json::Value::object();
  v.set("event", name);
  return v.dump();
}

std::string makeEventWith(const std::string& name, const std::string& key, const std::string& value) {
  localsend::json::Value v = localsend::json::Value::object();
  v.set("event", name);
  v.set(key, value);
  return v.dump();
}

} // namespace

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
    localsend::json::Value v = localsend::json::Value::object();
    v.set("event", "fileAdded");
    v.set("fileId", f.fileId);
    v.set("fileName", f.fileName);
    PostEvent(v.dump());
  };
  cb.onFileRemoved = [](const localsend::FileInfo& f) {
    PostEvent(makeEventWith("fileRemoved", "fileId", f.fileId));
  };
  cb.onPairRequest = [](const localsend::Device& d) {
    localsend::json::Value v = localsend::json::Value::object();
    v.set("event", "pairRequest");
    v.set("deviceId", d.deviceId);
    v.set("deviceName", d.deviceName);
    PostEvent(v.dump());
  };
  cb.onPairResult = [](const localsend::Device& d, bool accepted) {
    localsend::json::Value v = localsend::json::Value::object();
    v.set("event", "pairResult");
    v.set("deviceId", d.deviceId);
    v.set("accepted", accepted);
    PostEvent(v.dump());
  };
  cb.onDeviceFound = [](const localsend::Device&) {
    PostEvent(makeEvent("devicesChanged"));
  };
  cb.onDeviceOnline = [](const localsend::Device&) {
    PostEvent(makeEvent("devicesChanged"));
  };
  cb.onDeviceOffline = [](const localsend::Device&) {
    PostEvent(makeEvent("devicesChanged"));
  };
  cb.onTransferFinished = [](const std::string& fileId, const std::string& fileName, bool ok) {
    localsend::json::Value v = localsend::json::Value::object();
    v.set("event", "transferFinished");
    v.set("fileId", fileId);
    v.set("fileName", fileName);
    v.set("ok", ok);
    PostEvent(v.dump());
  };
  cb.onError = [](const std::string& message) {
    PostEvent(makeEventWith("error", "message", message));
  };
  localsend::Core::instance().setCallbacks(cb);
  return MakeBool(env, true);
}

} // namespace lsbridge
