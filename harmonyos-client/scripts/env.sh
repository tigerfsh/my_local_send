#!/usr/bin/env bash
# =============================================================================
# HarmonyOS 命令行构建 —— 环境变量（用 source 加载）
#
#   source scripts/env.sh
#
# 请按你的实际安装路径修改下面变量（当前已按本机实际路径填写）。
# =============================================================================
set -euo pipefail

# --- 1. 命令行工具根目录（含 bin/ 与 sdk/）--------------------------------
export DEVECO_HOME="${DEVECO_HOME:-/home/fsh/My_Island/ohos_CLI/command-line-tools}"

# --- 2. SDK 根目录 ----------------------------------------------------------
export DEVECO_SDK_HOME="${DEVECO_SDK_HOME:-$DEVECO_HOME/sdk}"

# --- 3. Node.js 根目录 ------------------------------------------------------
# hvigor 依赖 Node.js（推荐 18.x / 20.x）。若系统已通过 nvm 等提供 node，
# 则自动探测其真实目录，无需手动填写。
if [[ -z "${NODE_HOME:-}" ]]; then
  if command -v node >/dev/null 2>&1; then
    NODE_BIN="$(command -v node)"
    # 解析符号链接，得到真实 node 可执行文件路径
    NODE_REAL="$(readlink -f "$NODE_BIN")"
    NODE_HOME="$(dirname "$(dirname "$NODE_REAL")")"
  else
    NODE_HOME=""
  fi
fi
export NODE_HOME

# --- 3.5 hvigor 用户缓存目录 -------------------------------------------------
# hvigor 默认把插件/依赖缓存写到 ~/.hvigor。为保持工程自包含、并避免权限/沙箱
# 限制，这里默认放到工程内的 .hvigor 目录（可用 HVIGOR_USER_HOME 覆盖）。
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
export HVIGOR_USER_HOME="${HVIGOR_USER_HOME:-$PROJECT_DIR/.hvigor}"
mkdir -p "$HVIGOR_USER_HOME"

# --- 3.6 npm / pnpm 缓存目录（自包含，避免污染 ~/.npm / ~/.cache）-------------
export NPM_CONFIG_CACHE="$PROJECT_DIR/.npm-cache"
export XDG_CACHE_HOME="$PROJECT_DIR/.cache"
export PNPM_HOME="$PROJECT_DIR/.pnpm-home"
mkdir -p "$NPM_CONFIG_CACHE" "$XDG_CACHE_HOME" "$PNPM_HOME"

# --- 4. 加入 PATH -----------------------------------------------------------
# 命令行工具 bin：hvigorw / ohpm / codelinter / hstack
export PATH="$DEVECO_HOME/bin:$PATH"
# SDK 原生编译工具链：hdc / 交叉编译工具等
export PATH="$DEVECO_SDK_HOME/default/openharmony/toolchains:$PATH"
# Node（若探测到）
if [[ -n "$NODE_HOME" ]]; then
  export PATH="$NODE_HOME/bin:$PATH"
fi

echo "[env] DEVECO_HOME       = $DEVECO_HOME"
echo "[env] DEVECO_SDK_HOME   = $DEVECO_SDK_HOME"
echo "[env] NODE_HOME         = $NODE_HOME"

# 基本自检
command -v node    >/dev/null 2>&1 && echo "[env] node    OK: $(node --version 2>/dev/null)" || echo "[env][warn] 未找到 node"
command -v ohpm    >/dev/null 2>&1 && echo "[env] ohpm    OK" || echo "[env][warn] 未找到 ohpm"
command -v hvigorw >/dev/null 2>&1 && echo "[env] hvigorw OK: $(hvigorw --version 2>/dev/null | head -1)" || echo "[env][warn] 未找到 hvigorw"
