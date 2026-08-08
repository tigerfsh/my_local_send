# 架构说明

## 1. 总体分层

```
┌─────────────────────────────────────────────────────────────┐
│                    UI 交互层（两端独立）                      │
│   HarmonyOS: ArkTS + ArkUI（悬浮窗/列表/预览/设置/设备管理）   │
│   Ubuntu:    Qt6 QML + C++（悬浮窗/列表/预览/设置/设备管理）   │
└──────────────────────────┬──────────────────────────────────┘
                           │
┌──────────────────────────▼──────────────────────────────────┐
│                终端适配桥接层（两端独立封装）                  │
│   HarmonyOS: NAPI C++ 桥接（bridge/），线程安全事件通道       │
│   Ubuntu:    AppController + QAbstractListModel（controller/）│
└──────────────────────────┬──────────────────────────────────┘
                           │
┌──────────────────────────▼──────────────────────────────────┐
│           跨端通用底层 C++ SDK（sdk/，静态库，一套复用）        │
│  core      门面/装配/协议编排/配对/同步                       │
│  discovery UDP 组播设备发现（224.0.0.1:53317）               │
│  transport TCP 帧收发、分片流、断点续传（53318）              │
│  crypto    RSA-OAEP / AES-128-GCM / MD5 / Base64（OpenSSL）  │
│  storage   SQLite：file_info / trusted_device / transfer_state│
│  cache     缓存目录、过期清理                                 │
│  pairing   配对报文构建/解析                                  │
│  json      内置轻量 JSON 解析/序列化（无第三方依赖）           │
└─────────────────────────────────────────────────────────────┘
```

## 2. 目录结构

```
my_local_send/
├── CMakeLists.txt          # 顶层构建（SDK + 测试 + Ubuntu 端）
├── docs/
│   ├── protocol.md         # 通信协议细化
│   └── architecture.md     # 本文档
├── sdk/                    # ★ 共享 C++ SDK（静态库 liblocalsend-core.a）
│   ├── include/localsend/  # 公共头文件
│   ├── src/                # 实现
│   └── tests/              # 单元/集成测试（ctest）
├── ubuntu-client/          # ★ Ubuntu Qt6 客户端
│   ├── src/controller/     # AppController / FileListModel / DeviceListModel
│   └── qml/                # 悬浮窗 UI
└── harmonyos-client/       # ★ HarmonyOS 工程（DevEco Studio 构建）
    ├── entry/src/main/ets/ # ArkTS 页面/组件/视图模型/桥接封装
    └── entry/src/main/cpp/ # NAPI 桥接 + 内嵌 SDK 源码（localsend-sdk/）
```

## 3. SDK 核心设计

### 3.1 单例门面 `localsend::Core`
- 全局单例 `Core::instance()`，`configure(cfg)` + `start()` / `stop()`。
- 内部持有：`Storage` / `Cache` / `Discovery` / `Transport`。
- 启动时自动生成/加载 RSA 密钥对、设备 UUID、SQLite 库与缓存目录。

### 3.2 事件驱动
`Core::setCallbacks(Callbacks)` 注入回调，SDK 线程安全地分发：
`onFileAdded / onFileRemoved / onDeviceFound / onDeviceOnline / onDeviceOffline / onPairRequest / onPairResult / onTransferProgress / onTransferFinished / onError`。

### 3.3 线程模型
- Discovery：独立接收线程（1s 接收超时，保证可优雅停止）+ Core 后台 announce 线程（3s 周期）。
- Transport：accept 线程（poll + 200ms 超时）+ 每连接 worker 线程；停止时关闭活跃 fd 使 worker 立即退出。
- 文件发送：`addFile`/`syncWithPeer` 在独立线程执行，不阻塞 UI。

### 3.4 数据持久化（SQLite，两端一致）
- `file_info`：file_id(PK)/file_name/file_type/file_size/duration/cache_path/create_time/expire_time/is_local
- `trusted_device`：device_id(PK)/device_name/device_type/public_key/aes_key/last_online
- `transfer_state`：file_id/chunks_done/chunk_total/peer_id（断点续传）
- `settings`：键值对（悬浮窗锁定/自动收起/缓存过期/限速）

## 4. 两端适配要点

### Ubuntu（Qt6）
- 悬浮窗：`Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint | Qt.Tool` 无边框置顶窗口，`Main.qml` 三态（收缩/自适应/拉满）切换 + 5s 自动回收定时器。
- 数据绑定：`FileListModel`/`DeviceListModel`（QAbstractListModel）暴露给 QML；`AppController` 桥接 SDK 回调（跨线程经 `QMetaObject::invokeMethod` 派发到 GUI 线程）。
- 媒体预览：图片 `Image`；视频 `MediaPlayer` + `VideoOutput`（QtMultimedia）。
- 文件存入：QML `FileDialog` / `DropArea` 拖放；删除/多选/批量删除经 SDK 同步到可信设备。
- 命令行参数：`--data-dir --tcp-port --device-name --local-ip`（支持同机双实例联调）。

### HarmonyOS（ArkTS + NAPI）
- 悬浮窗：`window.createWindow(WINDOW_TYPE_FLOAT)` 子窗口，`FloatingWindowMgr` 管理页面切换（收缩条/展开面板/预览/设置/设备管理）；需要 `SYSTEM_FLOAT_WINDOW`（ACL）权限。
- NAPI 桥接：`cpp/` 编译内嵌 SDK 源码（`localsend-sdk/`），导出原始类型接口；对象以 JSON 字符串传递；事件经 `napi_threadsafe_function` 投递到 ArkTS 线程。
- 文件选择：`DocumentViewPicker` + `fileIo` 拷贝到应用沙箱后调用 `bridge.addFile`。
- 视频预览：ArkUI `Video` 组件；图片预览：`Image` 全屏点击关闭。
- 构建要求：DevEco Studio + HarmonyOS SDK 5.0.0(12)+；OpenSSL 使用 OpenHarmony 系统库（`libcrypto.z.so`/`libssl.z.so`）。**OpenHarmony NDK 不提供可链接的 sqlite3 系统库**，需在 `entry/src/main/cpp/sqlite/` 放置 SQLite 合并版源码（sqlite3.c + sqlite3.h，来自 https://sqlite.org/amalgamation.html），CMake 会自动编译；否则构建会以清晰错误提示终止。**注意**：SDK 的 `core.cpp`/`cache.cpp` 使用了 `std::filesystem`，若目标 OpenHarmony SDK 的 libc++ 不支持（部分 4.x/5.0 版本缺失实现符号），需将相关调用替换为 POSIX 文件 API（`mkdir/rename/unlink/stat` 等）。

## 5. 构建与测试

```bash
# 顶层一键构建（SDK + Ubuntu 客户端 + 测试）
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)

# SDK 单测（crypto/storage/discovery/core 端到端）
ctest --test-dir build --output-on-failure

# 运行 Ubuntu 客户端
./build/ubuntu-client/localsend-ubuntu

# 同机双实例联调（错开端口）
./build/ubuntu-client/localsend-ubuntu --data-dir /tmp/a --tcp-port 53320 --device-name A --local-ip 127.0.0.1 &
./build/ubuntu-client/localsend-ubuntu --data-dir /tmp/b --tcp-port 53321 --device-name B --local-ip 127.0.0.1 &
```

## 6. 安全设计
- 设备发现明文（仅设备元数据 + 公钥）；配对密钥 RSA-OAEP(SHA256) 传输。
- 文件分片 AES-128-GCM 加密，每片独立 IV（fileId+序号），抗重放与篡改。
- 接收文件 MD5 校验，失败即删除。
- 明文不入公网：全程局域网 Socket，无 HTTP/云端依赖。
