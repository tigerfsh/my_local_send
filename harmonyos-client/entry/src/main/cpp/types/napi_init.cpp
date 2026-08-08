#include <napi/native_api.h>

#include "bridge/bridge_api.h"

static napi_value Init(napi_env env, napi_value exports) {
  napi_property_descriptor desc[] = {
      {"start", nullptr, lsbridge::Start, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"stop", nullptr, lsbridge::Stop, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"addFile", nullptr, lsbridge::AddFile, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"removeFile", nullptr, lsbridge::RemoveFile, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"requestPair", nullptr, lsbridge::RequestPair, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"pairDevice", nullptr, lsbridge::PairDevice, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"removeDevice", nullptr, lsbridge::RemoveDevice, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"connectByIp", nullptr, lsbridge::ConnectByIp, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"onEvent", nullptr, lsbridge::OnEvent, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"getFileListJson", nullptr, lsbridge::GetFileListJson, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"getDevicesJson", nullptr, lsbridge::GetDevicesJson, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"getSettingsJson", nullptr, lsbridge::GetSettingsJson, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"saveSettingsJson", nullptr, lsbridge::SaveSettingsJson, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"rescan", nullptr, lsbridge::Rescan, nullptr, nullptr, nullptr, napi_default, nullptr},
  };
  napi_define_properties(env, exports, sizeof(desc) / sizeof(desc[0]), desc);
  return exports;
}

static napi_module demoModule = {
    .nm_version = 1,
    .nm_flags = 0,
    .nm_filename = nullptr,
    .nm_register_func = Init,
    .nm_modname = "entry",
    .nm_priv = ((void*)0),
    .reserved = {0},
};

extern "C" __attribute__((constructor)) void RegisterEntryModule(void) {
  napi_module_register(&demoModule);
}
