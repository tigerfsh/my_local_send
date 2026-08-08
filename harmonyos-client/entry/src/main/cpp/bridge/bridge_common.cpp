#include "bridge_common.h"

namespace lsbridge {

napi_value MakeUtf8(napi_env env, const std::string& s) {
  napi_value out;
  napi_create_string_utf8(env, s.c_str(), s.size(), &out);
  return out;
}

std::string GetUtf8(napi_env env, napi_value value) {
  size_t len = 0;
  napi_get_value_string_utf8(env, value, nullptr, 0, &len);
  if (len == 0) return std::string();
  std::string out(len, '\0');
  size_t written = 0;
  napi_get_value_string_utf8(env, value, &out[0], len + 1, &written);
  return out;
}

napi_value MakeBool(napi_env env, bool b) {
  napi_value out;
  napi_get_boolean(env, b, &out);
  return out;
}

bool GetBool(napi_env env, napi_value value) {
  bool out = false;
  napi_get_value_bool(env, value, &out);
  return out;
}

napi_value MakeInt(napi_env env, int v) {
  napi_value out;
  napi_create_int32(env, v, &out);
  return out;
}

int GetInt(napi_env env, napi_value value) {
  int32_t out = 0;
  napi_get_value_int32(env, value, &out);
  return static_cast<int>(out);
}

namespace {
napi_threadsafe_function g_tsfn = nullptr;
}

static void CallJs(napi_env env, napi_value jsCb, void*, void* data) {
  auto* payload = static_cast<std::string*>(data);
  napi_value argv[1];
  napi_create_string_utf8(env, payload->c_str(), payload->size(), &argv[0]);
  napi_value recv;
  napi_get_undefined(env, &recv);
  napi_value result;
  napi_call_function(env, recv, jsCb, 1, argv, &result);
  delete payload;
}

bool InitEventChannel(napi_env env, napi_value jsCallback) {
  if (g_tsfn != nullptr) {
    napi_release_threadsafe_function(g_tsfn, napi_tsfn_abort);
    g_tsfn = nullptr;
  }
  napi_value resourceName;
  napi_create_string_utf8(env, "localsend-events", NAPI_AUTO_LENGTH, &resourceName);
  napi_status status = napi_create_threadsafe_function(
      env, jsCallback, nullptr, resourceName, 0, 1, nullptr, nullptr, nullptr, CallJs, &g_tsfn);
  return status == napi_ok;
}

void PostEvent(const std::string& json) {
  if (g_tsfn == nullptr) return;
  auto* payload = new std::string(json);
  napi_call_threadsafe_function(g_tsfn, payload, napi_tsfn_blocking);
}

void ReleaseEventChannel() {
  if (g_tsfn != nullptr) {
    napi_release_threadsafe_function(g_tsfn, napi_tsfn_abort);
    g_tsfn = nullptr;
  }
}

} // namespace lsbridge
