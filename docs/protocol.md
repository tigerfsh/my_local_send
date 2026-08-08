# 通信协议（局域网跨端协议，兼容 HarmonyOS + Ubuntu）

基于 PRD 第三章细化，由共享 C++ SDK（`sdk/`）实现。报文统一 JSON（UTF-8），传输二进制采用分片流。

## 1. 端口与基础规范

| 项 | 规格 |
|---|---|
| 设备发现 | IPv4 UDP 组播 `224.0.0.1:53317` |
| 文件传输 | TCP 点对点 `53318`（可配置，双实例联调时错开端口） |
| 数据格式 | JSON 报文；文件二进制分片流（每分片 4MB） |
| 加密 | 配对阶段 RSA-OAEP(SHA256) 交换 AES 密钥；文件流 **AES-128-GCM** 分片加密（每片独立 IV，可断点续传） |
| 编码 | 全部 UTF-8 |

> 说明：PRD 指定 AES-128，实现选用 **AES-128-GCM**（认证加密），可在 `crypto.cpp` 中切换到 AES-256-GCM。

## 2. 帧封装（TCP）

每条 TCP 连接只承载**一个逻辑消息**（简化接收方解析），帧格式：

```
[4 字节大端长度][负载]
```

- 首帧：JSON 报文（`cmd` 字段区分指令）
- `fileAdd`：首帧后紧跟 N 个分片帧（长度前缀），分片负载 = `AES-GCM(明文分片) || 16字节tag`

## 3. UDP 设备发现

### deviceAnnounce（组播广播）
```json
{
  "msgType": "deviceAnnounce",
  "deviceId": "设备唯一UUID",
  "deviceType": "harmonyos/ubuntu",
  "deviceName": "办公手机/开发PC",
  "version": "1.0.0",
  "publicKey": "设备RSA公钥PEM",
  "ip": "本机局域网IP",
  "tcpPort": 53318
}
```

### deviceAck（回包，采用组播发送保证同机多实例可靠）
```json
{
  "msgType": "deviceAck",
  "targetDeviceId": "广播设备ID",
  "isTrusted": true,
  "deviceId": "本机ID",
  "deviceName": "本机名称",
  "deviceType": "ubuntu",
  "ip": "本机IP",
  "tcpPort": 53318
}
```

设备每 3s 组播一次 announce；15s 未收到则标记离线。

## 4. TCP 指令

所有指令首帧都包含 `deviceId`（发送方 ID），便于接收方做信任校验。

### 4.1 fileProbe（MD5 秒传探测）
请求：
```json
{ "cmd": "fileProbe", "deviceId": "本机ID", "fileId": "文件MD5", "fileName": "a.mp4", "fileSize": 1048576 }
```
响应：
```json
{ "cmd": "fileProbeAck", "fileId": "文件MD5", "exists": false }
```
`exists=true` 时接收方本地已有该文件，跳过传输。

### 4.2 fileResume（断点续传查询）
请求：
```json
{ "cmd": "fileResume", "deviceId": "本机ID", "fileId": "文件MD5" }
```
响应：
```json
{ "cmd": "fileResumeAck", "fileId": "文件MD5", "chunksDone": 7 }
```

### 4.3 fileAdd（文件新增同步）
```json
{
  "cmd": "fileAdd",
  "deviceId": "本机ID",
  "fileId": "文件MD5",
  "fileName": "demo.mp4",
  "fileType": "image/video/doc",
  "fileSize": 1048576,
  "fileDuration": 33,
  "createTime": "2026-08-08 11:53:00",
  "chunkTotal": 4,
  "enc": "aes128-gcm",
  "resumeFrom": 0
}
```
- 首帧后跟 `chunkTotal - resumeFrom` 个分片帧（加密）。
- `chunkTotal == 0`：仅同步元数据（MD5 秒传命中），无二进制流。
- 接收方：按 `fileId` 派生每片 IV（`fileId` 前 4 字节 + 8 字节分片序号 LE），解密后追加写入 `cache/<fileId>_<name>`，完成后校验 `fileSize` 与 `fileId(MD5)`。
- 响应：`{ "cmd": "ack", "ok": true, "error": "" }`

### 4.4 fileDelete（文件删除同步）
```json
{ "cmd": "fileDelete", "deviceId": "本机ID", "fileId": "文件MD5" }
```
响应：`{ "cmd": "ack", "ok": true }`

### 4.5 syncAllFiles（全量文件池同步，重连时触发）
```json
{
  "cmd": "syncAllFiles",
  "deviceId": "本机ID",
  "fileList": [
    { "fileId": "xxx", "fileName": "a.jpg", "fileSize": 1024, "fileType": "image", "fileDuration": 0, "createTime": "...", "cachePath": "..." }
  ]
}
```
响应：
```json
{
  "cmd": "syncAllFilesAck",
  "missing": [ { "fileId": "xxx", "fileName": "a.jpg", ... } ]
}
```
请求方随后对 `missing` 中的文件逐一执行 fileProbe → fileAdd。

### 4.6 配对协议

pairRequest（发起方 → 响应方）：
```json
{
  "cmd": "pairRequest",
  "deviceId": "本机ID",
  "deviceName": "办公手机",
  "deviceType": "harmonyos",
  "publicKey": "本机RSA公钥PEM",
  "ip": "本机IP",
  "tcpPort": 53318
}
```
响应方即时回 `{ "cmd": "ack", "ok": true }`（仅确认收到）。用户决定后：

接受 → pairAccept（响应方 → 发起方，**由响应方生成 AES 密钥**）：
```json
{
  "cmd": "pairAccept",
  "deviceId": "响应方ID",
  "targetDeviceId": "发起方ID",
  "publicKey": "响应方RSA公钥PEM",
  "aesKeyEnc": "RSA-OAEP加密后的AES-128密钥Base64",
  "deviceName": "响应方名称",
  "deviceType": "ubuntu",
  "tcpPort": 53318
}
```
发起方用私钥解密 `aesKeyEnc` 得到会话密钥，双方各自持久化到 `trusted_device.aes_key`。

拒绝 → pairReject：
```json
{ "cmd": "pairReject", "deviceId": "响应方ID", "targetDeviceId": "发起方ID", "reason": "user rejected" }
```

## 5. 分片加密 / 续传细节

- 分片大小固定 4MB（`kChunkSize`）。
- 每片 IV（12 字节）= `fileId` 前 4 字节 + 分片序号（8 字节小端）。
- 加密后分片帧 = `密文 || 16字节GCM-tag`。
- 接收方每收一片写入 `transfer_state`（chunks_done），连接中断后重发侧通过 `fileResume` 获取进度，从 `resumeFrom = chunksDone` 续传。
- 传输完成后接收方 `MD5` 校验：不符则删除文件并报错。

## 6. 限速

`Settings.maxTransferRateBps > 0` 时，发送方在每分片后按目标速率 sleep 节流。
