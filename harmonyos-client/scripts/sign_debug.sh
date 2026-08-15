#!/usr/bin/env bash
# =============================================================================
# HarmonyOS debug 签名 + 安装脚本
#
#   bash scripts/sign_debug.sh [--install]
#
# 用 SDK 自带的 hap-sign-tool.jar 生成一套 debug 签名（CA 链 + app/profile 证书），
# 对 entry-default-unsigned.hap 签名，可选直接 hdc install 到已授权真机。
# =============================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
SIGN_DIR="$PROJECT_DIR/sign"

source "$SCRIPT_DIR/env.sh" >/dev/null 2>&1 || true

TOOLCHAIN_DIR="$DEVECO_SDK_HOME/default/openharmony/toolchains"
SIGN_TOOL="$TOOLCHAIN_DIR/lib/hap-sign-tool.jar"
HDC="$TOOLCHAIN_DIR/hdc"

UNSIGNED_HAP="$PROJECT_DIR/entry/build/default/outputs/default/entry-default-unsigned.hap"
SIGNED_HAP="$PROJECT_DIR/entry/build/default/outputs/default/entry-default-signed.hap"

BUNDLE_NAME="com.localsend.transfer"
DEVICE_UDID="${DEVICE_UDID:-}"
COMPAT_VERSION="24"
KEYSTORE="$SIGN_DIR/debug.p12"
KS_PWD="123456"

# 证书 subject 命名（与 OpenHarmony 官方手册一致）
SUBJ_ROOT="C=CN,O=OpenHarmony,OU=OpenHarmony Community,CN=Root CA"
SUBJ_APP_CA="C=CN,O=OpenHarmony,OU=OpenHarmony Community,CN=Application Signature Service CA"
SUBJ_PROFILE_CA="C=CN,O=OpenHarmony,OU=OpenHarmony Community,CN=Profile Signature Service CA"
SUBJ_APP="C=CN,O=OpenHarmony,OU=OpenHarmony Community,CN=LocalSend Debug"
SUBJ_PROFILE="C=CN,O=OpenHarmony,OU=OpenHarmony Community,CN=Provision Profile Debug"

JAVA="java -jar $SIGN_TOOL"

mkdir -p "$SIGN_DIR"
cd "$SIGN_DIR"

echo "==> 使用签名工具: $SIGN_TOOL"

# ---------- 1. 生成 CA 链 ----------
echo "==> [1/7] 生成根 CA"
$JAVA generate-ca \
  -keyAlias "oh-root-ca-key-v1" -keyPwd "$KS_PWD" -keyAlg ECC -keySize NIST-P-256 \
  -subject "$SUBJ_ROOT" -validity 3650 -signAlg SHA384withECDSA \
  -keystoreFile "$KEYSTORE" -keystorePwd "$KS_PWD" \
  -outFile "$SIGN_DIR/root-ca.cer"

echo "==> [2/7] 生成应用签名子 CA"
$JAVA generate-ca \
  -keyAlias "oh-app-sign-srv-ca-key-v1" -keyPwd "$KS_PWD" -keyAlg ECC -keySize NIST-P-256 \
  -issuer "$SUBJ_ROOT" -issuerKeyAlias "oh-root-ca-key-v1" -issuerKeyPwd "$KS_PWD" \
  -subject "$SUBJ_APP_CA" -validity 3650 -signAlg SHA384withECDSA \
  -keystoreFile "$KEYSTORE" -keystorePwd "$KS_PWD" \
  -outFile "$SIGN_DIR/sub-app-ca.cer"

echo "==> [3/7] 生成 profile 签名子 CA"
$JAVA generate-ca \
  -keyAlias "oh-profile-sign-srv-ca-key-v1" -keyPwd "$KS_PWD" -keyAlg ECC -keySize NIST-P-256 \
  -issuer "$SUBJ_ROOT" -issuerKeyAlias "oh-root-ca-key-v1" -issuerKeyPwd "$KS_PWD" \
  -subject "$SUBJ_PROFILE_CA" -validity 3650 -signAlg SHA384withECDSA \
  -keystoreFile "$KEYSTORE" -keystorePwd "$KS_PWD" \
  -outFile "$SIGN_DIR/sub-profile-ca.cer"

# ---------- 2. 生成应用/profile 密钥对 ----------
echo "==> [4/7] 生成应用密钥对 + profile 密钥对"
$JAVA generate-keypair \
  -keyAlias "oh-app1-key-v1" -keyPwd "$KS_PWD" -keyAlg ECC -keySize NIST-P-256 \
  -keystoreFile "$KEYSTORE" -keystorePwd "$KS_PWD"

$JAVA generate-keypair \
  -keyAlias "oh-profile1-key-v1" -keyPwd "$KS_PWD" -keyAlg ECC -keySize NIST-P-256 \
  -keystoreFile "$KEYSTORE" -keystorePwd "$KS_PWD"

# ---------- 3. 生成应用/profile 证书（证书链） ----------
echo "==> [5/7] 生成应用证书链"
$JAVA generate-app-cert \
  -keyAlias "oh-app1-key-v1" -keyPwd "$KS_PWD" \
  -issuer "$SUBJ_APP_CA" -issuerKeyAlias "oh-app-sign-srv-ca-key-v1" -issuerKeyPwd "$KS_PWD" \
  -subject "$SUBJ_APP" -validity 365 -signAlg SHA256withECDSA \
  -rootCaCertFile "$SIGN_DIR/root-ca.cer" -subCaCertFile "$SIGN_DIR/sub-app-ca.cer" \
  -keystoreFile "$KEYSTORE" -keystorePwd "$KS_PWD" \
  -outForm certChain -outFile "$SIGN_DIR/app-cert-chain.cer"

echo "==> [6/7] 生成 profile 证书链"
$JAVA generate-profile-cert \
  -keyAlias "oh-profile1-key-v1" -keyPwd "$KS_PWD" \
  -issuer "$SUBJ_PROFILE_CA" -issuerKeyAlias "oh-profile-sign-srv-ca-key-v1" -issuerKeyPwd "$KS_PWD" \
  -subject "$SUBJ_PROFILE" -validity 365 -signAlg SHA256withECDSA \
  -rootCaCertFile "$SIGN_DIR/root-ca.cer" -subCaCertFile "$SIGN_DIR/sub-profile-ca.cer" \
  -keystoreFile "$KEYSTORE" -keystorePwd "$KS_PWD" \
  -outForm certChain -outFile "$SIGN_DIR/profile-cert-chain.cer"

# ---------- 4. 生成并签名 profile ----------
echo "==> [7/8] 生成 debug profile 并签名"
python3 - "$SIGN_DIR/app-cert-chain.cer" "$BUNDLE_NAME" "$DEVICE_UDID" "$SIGN_DIR/app1-profile-debug.json" <<'PYEOF'
import json, sys, time, uuid

cert_chain_path, bundle_name, udid, out_path = sys.argv[1:5]

with open(cert_chain_path) as f:
    chain = f.read()

# 取证书链中的第一张证书（叶子 = 应用证书）
blocks = []
for m in chain.split('-----BEGIN CERTIFICATE-----'):
    if '-----END CERTIFICATE-----' in m:
        blocks.append('-----BEGIN CERTIFICATE-----' + m)

if not blocks:
    print("ERROR: no cert found in chain", file=sys.stderr)
    sys.exit(1)
# 注意：证书必须以 "-----END CERTIFICATE-----\n" 结尾（含换行），
# 否则 hap-sign-tool 会把它当裸 base64 去 URL 解码，报 "Illegal base64 character"。
leaf = blocks[0].rstrip('\r\n') + '\n'

now = int(time.time())
profile = {
    "version-name": "2.0.0",
    "version-code": 2,
    "uuid": str(uuid.uuid4()),
    "validity": {
        "not-before": now - 86400,
        "not-after": now + 365 * 86400
    },
    "type": "debug",
    "bundle-info": {
        "developer-id": "OpenHarmony",
        "development-certificate": leaf,
        "bundle-name": bundle_name,
        "apl": "normal",
        "app-feature": "hos_normal_app"
    },
    "acls": {"allowed-acls": [""]},
    "permissions": {"restricted-permissions": [""]},
    "debug-info": {
        "device-ids": [udid],
        "device-id-type": "udid"
    },
    "issuer": "pki_internal"
}

with open(out_path, 'w') as f:
    json.dump(profile, f, indent=4)
print("profile written to", out_path)
print("leaf cert subject snippet: ", leaf[:60])
PYEOF

$JAVA sign-profile \
  -mode localSign \
  -keyAlias "oh-profile1-key-v1" -keyPwd "$KS_PWD" \
  -profileCertFile "$SIGN_DIR/profile-cert-chain.cer" \
  -inFile "$SIGN_DIR/app1-profile-debug.json" \
  -signAlg SHA256withECDSA \
  -keystoreFile "$KEYSTORE" -keystorePwd "$KS_PWD" \
  -outFile "$SIGN_DIR/signed-profile.p7b"

# ---------- 5. 签名 app ----------
echo "==> [8/8] 签名 HAP"
$JAVA sign-app \
  -mode localSign \
  -keyAlias "oh-app1-key-v1" -keyPwd "$KS_PWD" \
  -appCertFile "$SIGN_DIR/app-cert-chain.cer" \
  -profileFile "$SIGN_DIR/signed-profile.p7b" \
  -inFile "$UNSIGNED_HAP" \
  -signAlg SHA256withECDSA \
  -keystoreFile "$KEYSTORE" -keystorePwd "$KS_PWD" \
  -outFile "$SIGNED_HAP" \
  -compatibleVersion "$COMPAT_VERSION"

echo ""
echo "==> 完成。已签名 HAP："
echo "    $SIGNED_HAP"

if [[ "${1:-}" == "--install" ]]; then
  echo "==> 安装到设备"
  "$HDC" install -r "$SIGNED_HAP"
fi
