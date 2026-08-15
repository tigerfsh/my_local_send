#!/usr/bin/env bash
# =============================================================================
# HarmonyOS 命令行构建 —— 编译脚本
#
#   bash scripts/build.sh [debug|release] [--signing-config <name>] [--clean]
#
# 示例：
#   bash scripts/build.sh debug                 # 编译 debug HAP（未签名）
#   bash scripts/build.sh release               # 编译 release（需签名）
#   bash scripts/build.sh release --clean       # 先 clean 再编译
# =============================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

# 默认参数
BUILD_MODE="debug"
SIGNING_CONFIG=""
DO_CLEAN=0
EXTRA_ARGS=()

# 解析参数
while [[ $# -gt 0 ]]; do
  case "$1" in
    debug|release) BUILD_MODE="$1"; shift ;;
    --signing-config) SIGNING_CONFIG="$2"; shift 2 ;;
    --clean) DO_CLEAN=1; shift ;;
    *) EXTRA_ARGS+=("$1"); shift ;;
  esac
done

echo "==> 加载环境变量"
# shellcheck disable=SC1091
source "$SCRIPT_DIR/env.sh"

cd "$PROJECT_DIR"

echo "==> 安装依赖 (ohpm install --all)"
ohpm install --all

if [[ "$DO_CLEAN" -eq 1 ]]; then
  echo "==> 清理 (hvigorw clean)"
  hvigorw clean --no-daemon
fi

# 构造 hvigor 参数
HVIGOR_ARGS=(assembleHap --mode module -p "product=default" -p "buildMode=$BUILD_MODE")
if [[ -n "$SIGNING_CONFIG" ]]; then
  HVIGOR_ARGS+=(-p "signingConfig=$SIGNING_CONFIG")
fi
HVIGOR_ARGS+=(--no-daemon)
HVIGOR_ARGS+=("${EXTRA_ARGS[@]:-}")

echo "==> 构建 ($BUILD_MODE)"
echo "    hvigorw ${HVIGOR_ARGS[*]}"
hvigorw "${HVIGOR_ARGS[@]}"

echo ""
echo "==> 完成。产物位置（常见路径）："
echo "    entry/build/default/outputs/default/entry-default-unsigned.hap  (debug 未签名)"
echo "    entry/build/default/outputs/default/entry-default-signed.hap    (已签名)"
