#include "bridge_common.h"

#include "localsend/localsend.h"
#include "localsend-sdk/src/json.hpp"

namespace lsbridge {

napi_value GetFileListJson(napi_env env, napi_callback_info info) {
  (void)info;
  auto files = localsend::Core::instance().listFiles();
  localsend::json::Value arr = localsend::json::Value::array();
  for (const auto& f : files) {
    localsend::json::Value v = localsend::json::Value::object();
    v.set("fileId", f.fileId);
    v.set("fileName", f.fileName);
    v.set("fileType", localsend::fileTypeToString(f.fileType));
    v.set("fileSize", f.fileSize);
    v.set("fileDuration", f.duration);
    v.set("cachePath", f.cachePath);
    v.set("createTime", f.createTime);
    arr.push(v);
  }
  return MakeUtf8(env, arr.dump());
}

napi_value GetDevicesJson(napi_env env, napi_callback_info info) {
  (void)info;
  auto devices = localsend::Core::instance().listDevices();
  localsend::json::Value arr = localsend::json::Value::array();
  for (const auto& d : devices) {
    localsend::json::Value v = localsend::json::Value::object();
    v.set("deviceId", d.deviceId);
    v.set("deviceName", d.deviceName);
    v.set("deviceType", localsend::deviceTypeToString(d.deviceType));
    v.set("ip", d.ip);
    v.set("tcpPort", static_cast<int64_t>(d.tcpPort));
    v.set("isTrusted", d.isTrusted);
    v.set("isOnline", d.isOnline);
    arr.push(v);
  }
  return MakeUtf8(env, arr.dump());
}

napi_value GetSettingsJson(napi_env env, napi_callback_info info) {
  (void)info;
  const localsend::Settings& s = localsend::Core::instance().settings();
  localsend::json::Value v = localsend::json::Value::object();
  v.set("floatWindowLocked", s.floatWindowLocked);
  v.set("autoCollapseMs", s.autoCollapseMs);
  v.set("cacheExpireHours", s.cacheExpireHours);
  v.set("maxTransferRateBps", s.maxTransferRateBps);
  return MakeUtf8(env, v.dump());
}

napi_value SaveSettingsJson(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1];
  napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
  if (argc < 1) return MakeBool(env, false);
  std::string jsonText = GetUtf8(env, args[0]);
  localsend::json::Value v = localsend::json::Value::parse(jsonText);
  localsend::Settings& s = localsend::Core::instance().settings();
  if (v.has("floatWindowLocked")) s.floatWindowLocked = v["floatWindowLocked"].asBool();
  if (v.has("autoCollapseMs")) s.autoCollapseMs = static_cast<int>(v["autoCollapseMs"].asInt());
  if (v.has("cacheExpireHours")) s.cacheExpireHours = static_cast<int>(v["cacheExpireHours"].asInt());
  if (v.has("maxTransferRateBps")) s.maxTransferRateBps = v["maxTransferRateBps"].asInt();
  localsend::Core::instance().saveSettings();
  return MakeBool(env, true);
}

} // namespace lsbridge
