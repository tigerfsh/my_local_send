#include "bridge_common.h"

#include "localsend/localsend.h"

namespace lsbridge {

// 传输相关桥接：由 Core 统一驱动，这里仅暴露重扫描等控制能力。
napi_value Rescan(napi_env env, napi_callback_info info) {
  (void)info;
  localsend::Core::instance().rescan();
  return MakeBool(env, true);
}

} // namespace lsbridge
