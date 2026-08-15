#!/usr/bin/env bash
# =============================================================================
# HarmonyOS 命令行构建 —— 安装到真机（hdc）
#
#   bash scripts/install.sh [hap 文件路径]
#
# 不带参数时，自动查找 debug 签名 HAP。
# 前提：真机已开启「开发者模式 + USB 调试」，hdc 能识别设备。
# =============================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

source "$SCRIPT_DIR/env.sh"

HAP="${1:-}"
if [[ -z "$HAP" ]]; then
  HAP="$PROJECT_DIR/entry/build/default/outputs/default/entry-default-signed.hap"
  if [[ ! -f "$HAP" ]]; then
    HAP="$PROJECT_DIR/entry/build/default/outputs/default/entry-default-unsigned.hap"
  fi
fi

if [[ ! -f "$HAP" ]]; then
  echo "[install] 找不到 HAP：$HAP" >&2
  echo "          请先执行 bash scripts/build.sh，或显式传入 HAP 路径。" >&2
  exit 1
fi

echo "==> 检测设备"
hdc list targets

echo "==> 安装 $HAP"
hdc install "$HAP"

echo "==> 完成。可用 hdc shell 或真机启动应用。"
