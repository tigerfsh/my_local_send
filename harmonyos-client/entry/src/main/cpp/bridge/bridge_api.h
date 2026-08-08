#ifndef BRIDGE_API_H
#define BRIDGE_API_H

#include <napi/native_api.h>

namespace lsbridge {

// bridge_core.cpp
napi_value Start(napi_env env, napi_callback_info info);
napi_value Stop(napi_env env, napi_callback_info info);
napi_value AddFile(napi_env env, napi_callback_info info);
napi_value RemoveFile(napi_env env, napi_callback_info info);
napi_value RequestPair(napi_env env, napi_callback_info info);
napi_value PairDevice(napi_env env, napi_callback_info info);
napi_value RemoveDevice(napi_env env, napi_callback_info info);
napi_value ConnectByIp(napi_env env, napi_callback_info info);
napi_value OnEvent(napi_env env, napi_callback_info info);

// bridge_storage.cpp
napi_value GetFileListJson(napi_env env, napi_callback_info info);
napi_value GetDevicesJson(napi_env env, napi_callback_info info);
napi_value GetSettingsJson(napi_env env, napi_callback_info info);
napi_value SaveSettingsJson(napi_env env, napi_callback_info info);

// bridge_transport.cpp
napi_value Rescan(napi_env env, napi_callback_info info);

} // namespace lsbridge

#endif // BRIDGE_API_H
