#ifndef BRIDGE_COMMON_H
#define BRIDGE_COMMON_H

#include <napi/native_api.h>

#include <string>

namespace lsbridge {

napi_value MakeUtf8(napi_env env, const std::string& s);
std::string GetUtf8(napi_env env, napi_value value);
napi_value MakeBool(napi_env env, bool b);
bool GetBool(napi_env env, napi_value value);
napi_value MakeInt(napi_env env, int v);
int GetInt(napi_env env, napi_value value);

// Event delivery to the ArkTS thread via a thread-safe function.
bool InitEventChannel(napi_env env, napi_value jsCallback);
void PostEvent(const std::string& json);
void ReleaseEventChannel();

} // namespace lsbridge

#endif // BRIDGE_COMMON_H
