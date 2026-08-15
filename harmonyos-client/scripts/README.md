# HarmonyOS 命令行构建脚本

用「命令行工具（Command Line Tools）」在 Linux/Ubuntu 上编译 `harmonyos-client`，不依赖 DevEco Studio 图形界面。

## 目录

| 脚本 | 作用 |
|---|---|
| `env.sh` | 环境变量（`source` 使用） |
| `prepare.sh` | 一次性准备：SQLite 源码、`local.properties`、版本/签名提示 |
| `build.sh` | 编译（ohpm 装依赖 + hvigorw 构建） |
| `install.sh` | 用 `hdc` 装到真机 |

---

## 0. 版本说明（重要）

本工程已升级到 **HarmonyOS 6.1.1（API 24）**：

- `build-profile.json5`：`compatibleSdkVersion` = `6.1.1(24)`
- `hvigor/hvigor-config.json5`：`modelVersion` = `6.0.0`、`hvigorVersion` = `6.24.1`

请使用 **DevEco Studio 6.1.1 / Command Line Tools 6.1.1** 且装齐对应 SDK 组件（hvigor / ets / toolchains / ohos-sdk 等）。

> `hvigorVersion` 已按你本机 `hvigorw --version` 输出的 `6.24.1` 填写。`modelVersion` 若与实际不匹配，构建时会报明确的版本范围错误，按提示改即可；或直接用 DevEco Studio 打开工程让它自动对齐。

> 命令行工具只负责**构建**；模拟器/预览器在 Linux 上不可用，调试请用真机（hdc）。

---

## 1. 安装命令行工具 + Node

```bash
# 安装 Node 18+/20+
sudo apt install -y curl unzip
# 下载并解压 HarmonyOS Command Line Tools（从华为开发者官网获取压缩包）
# 解压后假设位于 /opt/command-line-tools
```

用 `ohsdkmgr` 装所需 SDK 组件（`hvigor`、`ets`、`toolchains`、`ohos-sdk` 等）：
```bash
ohsdkmgr install <组件> ...
# 或按官网指引用 ohsdkmgr list / install
```

---

## 2. 改环境变量

编辑 `scripts/env.sh`，把 `DEVECO_SDK_HOME`、`NODE_HOME` 改成你的实际路径。

---

## 3. 一次性准备

```bash
cd harmonyos-client
bash scripts/prepare.sh
```

它会：
- 下载 SQLite 合并版 `sqlite3.c` / `sqlite3.h` 到 `entry/src/main/cpp/sqlite/`（工程 C++ 层需要，缺失会构建报错）；
- 生成 `local.properties`（SDK 路径）。

`prepare.sh` 末尾会提示两件**必须人工确认**的事：

1. **hvigor 版本**：`hvigor/hvigor-config.json5` 缺 `hvigorVersion`，按你安装的工具版本补上，例如：
   ```json5
   { "modelVersion": "5.0.0", "hvigorVersion": "5.0.0", "dependencies": {} }
   ```
2. **签名**：`build-profile.json5` 的 `signingConfigs` 为空。纯编译 debug 可先不签；装真机/发 release 需配签名。

---

## 4. 编译

```bash
cd harmonyos-client
bash scripts/build.sh debug        # 编译 debug HAP
bash scripts/build.sh release      # 编译 release（需签名）
bash scripts/build.sh debug --clean
bash scripts/build.sh release --signing-config mySigning
```

产物：
- `entry/build/default/outputs/default/entry-default-unsigned.hap`（未签名）
- `entry/build/default/outputs/default/entry-default-signed.hap`（已签名）

---

## 5. 装到真机

```bash
bash scripts/install.sh
# 或显式指定：
bash scripts/install.sh entry/build/default/outputs/default/entry-default-signed.hap
```

前置：真机开「开发者模式 + USB 调试」，`hdc list targets` 能列出设备。

---

## 常见问题

- **报 `DEVECO_SDK_HOME` 找不到**：没 `source scripts/env.sh`，或 SDK 路径写错（见 [HarmonyOS 论坛](https://bbs.itying.com/topic/678eddf94b218c005fa28226)）。
- **报 SDK component missing**：`ohsdkmgr` 没装齐组件（见 [DevEco Studio FAQ](https://developer.huawei.com/consumer/cn/doc/harmonyos-faqs/faqs-development-environment-32)）。
- **CI 里 hvigorw BUILD FAILED 常见根因**：Node 版本、SDK 组件缺失、签名缺失、依赖未 `ohpm install`（见 [华为开发者论坛](https://developer.huawei.com.cn/consumer/cn/forum/topic/0204221010438867421?fid=0109140870620153026)）。
- **脱离 DevEco Studio 用 hvigorw**：参考 [hvigorw 命令](https://developer.huawei.com/consumer/en/doc/harmonyos-guides-V14/ide-hvigor-commandline-V14) 与 [搭建流水线](https://developer.huawei.com/consumer/cn/doc/doccenter-deveco-studio/ide-command-line-building-app)。

---

## 备注

- 本工程 C++ 层（NAPI + 复用 `localsend-sdk/`）在 `entry/src/main/cpp/CMakeLists.txt` 里交叉编译，SQLite 用的是工程内合并源码，不依赖系统 sqlite3。
- 如果目标 OpenHarmony NDK 的 libc++ 不支持 `std::filesystem`，按 `docs/architecture.md` 说明替换为 POSIX 文件 API。
