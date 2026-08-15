#!/usr/bin/env bash
# =============================================================================
# HarmonyOS 命令行构建 —— 一次性准备脚本
#
#   bash scripts/prepare.sh
#
# 完成三件事：
#   1. 下载 SQLite 合并版源码（NAPI C++ 层需要，工程里缺失）
#   2. 生成 local.properties（记录 SDK 路径）
#   3. 提示 hvigor 版本与签名配置（需人工确认）
# =============================================================================
set -euo pipefail

# 脚本所在目录（harmonyos-client/scripts），工程根 = 上一级
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
SQLITE_DIR="$PROJECT_DIR/entry/src/main/cpp/sqlite"

# --- SQLite amalgamation 版本（可改）----------------------------------------
# 版本号写成无分隔形式，例如 3.45.1 -> 3450100
SQLITE_VERSION="3450100"
SQLITE_YEAR="2024"
SQLITE_ZIP="sqlite-amalgamation-${SQLITE_VERSION}.zip"
SQLITE_URL="https://sqlite.org/${SQLITE_YEAR}/${SQLITE_ZIP}"

echo "==> [1/3] 准备 SQLite 合并源码到 entry/src/main/cpp/sqlite/"
mkdir -p "$SQLITE_DIR"
if [[ -f "$SQLITE_DIR/sqlite3.c" && -f "$SQLITE_DIR/sqlite3.h" ]]; then
  echo "    sqlite3.c / sqlite3.h 已存在，跳过下载。"
else
  echo "    下载 $SQLITE_URL ..."
  TMPDIR="$(mktemp -d)"
  curl -fL -o "$TMPDIR/$SQLITE_ZIP" "$SQLITE_URL"
  unzip -qo "$TMPDIR/$SQLITE_ZIP" -d "$TMPDIR"
  cp "$TMPDIR/sqlite-amalgamation-${SQLITE_VERSION}/sqlite3.c" "$SQLITE_DIR/"
  cp "$TMPDIR/sqlite-amalgamation-${SQLITE_VERSION}/sqlite3.h" "$SQLITE_DIR/"
  rm -rf "$TMPDIR"
  echo "    已放置 sqlite3.c / sqlite3.h。"
fi

# --- local.properties（SDK 路径）--------------------------------------------
echo "==> [2/3] 生成 local.properties"
source "$SCRIPT_DIR/env.sh" || true
LOCAL_PROPS="$PROJECT_DIR/local.properties"
cat > "$LOCAL_PROPS" <<EOF
sdk.dir=${DEVECO_SDK_HOME}
EOF
echo "    写入 $LOCAL_PROPS："
cat "$LOCAL_PROPS"

# --- hvigor 版本 & 签名 提示 -------------------------------------------------
echo "==> [3/3] 请人工确认以下两项"
echo "  1) hvigor 版本：工程 hvigor/hvigor-config.json5 目前缺少 hvigorVersion 字段。"
echo "     请按你安装的命令行工具版本补上，例如："
echo '       { "modelVersion": "5.0.0", "hvigorVersion": "5.0.0", ... }'
echo "     （若用 6.1.1 工具，请对照其自带的 hvigor 版本填写，或直接让 DevEco Studio 打开一次工程自动补全。）"
echo ""
echo "  2) 签名：build-profile.json5 里 signingConfigs 为空。"
echo "     纯编译 debug 可先不签名；要装真机/发 release，请配置签名后"
echo "     用 build.sh 的 --signing-config 参数，或把 signingConfigs 填进 build-profile.json5。"
echo ""
echo "完成。"
