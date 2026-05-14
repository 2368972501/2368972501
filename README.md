# OPC UA Multi-Server Client (open62541 1.4.14)

本工程提供一个基于 `open62541`（已安装头文件+静态库）的 Linux C++ OPC UA Client 框架，面向生产部署场景，重点能力：

- 多 Server 并发连接（每个 Server 独立会话线程）
- 批量读（`UA_Client_Service_read`）
- 批量写（`UA_Client_Service_write`）
- 自动掉线重连（指数退避）
- 单点故障隔离（某个 Server 掉线不影响其他会话）
- 支持加密连接（安全模式 / 安全策略 / 证书）
- 支持用户认证（Anonymous、Username/Password）

## 架构设计

### 1. 分层

1. **Session 层（`OpcUaSession`）**
   - 一条会话绑定一个 `UA_Client`
   - 内部独立线程循环执行 `UA_Client_run_iterate`
   - 连接失败自动重试，并通过回调上报连接状态
   - 提供线程安全的批量读写 API

2. **Manager 层（`OpcUaMultiClientManager`）**
   - 管理多个 `OpcUaSession`
   - 对外按 `serverId` 路由请求
   - 支持动态增删 Server、统一启停

3. **类型层（`OpcUaTypes.hpp`）**
   - 定义读写请求/结果、Server 配置对象

### 2. 关键生产特性说明

- **隔离性**：每个 Server 有自己的线程、连接状态和重连策略，互不阻塞。
- **可恢复性**：掉线时持续重连（`reconnectIntervalMs` 到 `maxReconnectIntervalMs` 指数退避），恢复后自动继续服务。
- **线程安全**：每个会话对 `UA_Client` 调用使用互斥锁保护；Manager 对会话表也加锁。
- **可观测性**：连接状态变化通过回调统一输出，便于接入日志系统。

### 3. 构建

```bash
cmake -S . -B build
cmake --build build -j
```

如果你的 open62541 安装在非系统默认路径，可通过 `CMAKE_PREFIX_PATH` 或修改 `OPEN62541_INCLUDE_DIR/OPEN62541_LIBRARY` 搜索路径。

### 4. 示例运行

```bash
./build/opcua_client_example
```

默认示例连接：
- `opc.tcp://127.0.0.1:4840`（line-a，SignAndEncrypt + Basic256Sha256 + 用户名密码）
- `opc.tcp://127.0.0.1:4850`（line-b）

请按你的真实 Server 地址与节点配置修改 `src/main.cpp`。

## 5. 安全连接与认证配置

`ServerConfig` 新增以下字段：

- `securityMode`：`None` / `Sign` / `SignAndEncrypt`
- `securityPolicyUri`：如
  - `http://opcfoundation.org/UA/SecurityPolicy#None`
  - `http://opcfoundation.org/UA/SecurityPolicy#Basic256Sha256`
- `clientCertificatePath`：客户端证书（DER）
- `clientPrivateKeyPath`：客户端私钥（DER）
- `trustListPaths`：服务端证书或 CA 证书列表
- `useUsernamePassword`：是否使用用户名密码认证
- `username` / `password`：用户名密码

注意：
- 当 `securityMode != None` 或 `securityPolicyUri != #None` 时，客户端将按加密模式初始化。
- 若启用 `useUsernamePassword=true`，连接时会使用 `UA_Client_connectUsername`。
