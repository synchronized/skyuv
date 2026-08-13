# 004：Socket 语义兼容

## 状态

就绪。

## 背景

阶段 2 已完成 libuv TCP 最小闭环，Windows、macOS 和 Linux 均可构建便携 Skynet，Windows Lua TCP echo 已通过，Linux 原版与 libuv 版关键 TCP 事件序列也已自动对照一致。

当前 `socket_server.h` 仍有部分接口采用降级实现：`pause`、`nodelay` 是空操作，`shutdown` 等同完整关闭，高低优先级发送未区分，`socket_info` 返回空，UDP 与外部 fd `bind` 明确失败。阶段 3 需要补齐常用语义，同时保持 libuv handle 只由所属 loop 线程操作。

## 目标

- 补齐常用 `skynet.socket` TCP 控制和观测能力；
- 实现 Skynet UDP 创建、收发、默认目标和来源地址语义；
- 保持 `socket_server.h`、`skynet_socket.c` 和上层 Lua API 不变；
- 为无法跨平台等价实现的外部 fd 接管定义清晰限制；
- 每项能力都建立 Linux 原版与 libuv 版对照测试。

## 不在范围内

- TLS；
- DNS 异步解析和 Happy Eyeballs；
- Unix domain socket；
- Windows Service、daemon 和信号生命周期；
- `lua-clientsocket`、控制台输入及其他非核心 Lua 模块；
- 为兼容外部 fd 而暴露 libuv 或平台原生 handle 到普通公共 API。

## 兼容原则

### 线程和所有权

- worker 只提交命令，不直接操作 `uv_tcp_t`、`uv_udp_t` 或写队列；
- handle、请求和 socket 状态只在网络 loop 线程创建、变更和关闭；
- send 成功入队后由 runtime 持有缓冲区，失败时所有权留在或按现有接口约定归还调用方；
- pause、shutdown、nodelay 和 UDP connect 都进入同一 FIFO 命令队列，保持相对顺序。

### 可观察行为

- 事件类型、socket ID、opaque、数据和地址字段优先与原版一致；
- libuv 与 epoll 的内部批次、系统错误码和错误文字允许不同，但 Lua 可见错误必须非空且事件因果顺序一致；
- 不用额外 `SOCKET_OPEN`、`SOCKET_CLOSE` 或 `SOCKET_ERR` 掩盖状态机错误；
- 暂不支持的能力必须同步失败或产生明确错误，不允许静默成功。

## 实施顺序

### 1. TCP 控制语义

实现：

- `pause`：在 loop 线程调用 `uv_read_stop`，保留连接和已接收事件；
- `start`：暂停连接恢复读取，listener 的 start/resume 保持阶段 2 已验证行为；
- `nodelay`：映射 `uv_tcp_nodelay`，无效 ID 不影响其他连接；
- `shutdown`：映射 `uv_shutdown` 完成写方向 half-close，读取方向继续工作，完成后按原版语义报告关闭或错误；
- 重复 pause/start/shutdown 的幂等边界。

验证：

- pause 后对端发送数据不产生 DATA，start 后按顺序收到；
- shutdown 前已入队数据发送完成，对端收到 EOF，本端仍可处理对端数据；
- nodelay 对连接、listener 和无效 ID 的行为与原版对照；
- 重复控制命令、控制命令与 close/exit 竞态不泄漏或挂起。

### 2. 高低优先级写队列与 warning

实现：

- 每个连接维护高、低两个 FIFO；
- 高优先级队列优先，但已经提交给 libuv 的 write 不抢占；
- 同一优先级保持提交顺序；
- 记录待写字节数，在跨越原版阈值时产生 `SOCKET_WARNING`，下降后允许再次报告；
- close、shutdown 和 exit 明确处理两级队列及其缓冲区所有权。

验证：

- 高低优先级混合发送的顺序；
- 多 worker 并发发送及每个生产者顺序；
- warning 字段、触发和恢复行为；
- 写失败、连接中关闭和退出时缓冲区只释放一次。

### 3. Socket info

实现：

- 在 loop 线程维护类型、读写字节数、最近读写时间、待写字节数、读写状态和地址文本；
- `socket_server_info` 返回调用方拥有的快照链表，不泄漏 runtime 内部对象；
- 快照期间允许连接变化，单个条目必须自洽，已复用 ID 不得混入旧代际统计。

验证：

- listener、连接、暂停、写队列和 closing 状态；
- 读写计数及时间单调更新；
- 并发创建/关闭/查询和 `socket_info_release`；
- Lua `socket.info` 与原版关键字段对照。

### 4. UDP

实现：

- 使用 `uv_udp_t` 支持 IPv4/IPv6 bind、dial 和 listen；
- UDP socket 创建后立即接收，不要求 start；
- `udp_connect` 保存默认目标，普通 send 使用该目标；
- `udp_send` 支持事件携带的显式目标地址；
- `SOCKET_UDP` 数据缓冲区包含与原版兼容的来源地址编码；
- 无默认目标、地址非法、发送失败、close 和 exit 的明确行为。

验证：

- IPv4/IPv6 回环收发；
- 已连接与未连接 UDP；
- 回复来源地址、零长度数据报、连续数据报和大数据报；
- 非法地址、端口占用、无目标发送及关闭竞态；
- Linux 原版与 libuv 版 Lua UDP 服务对照。

### 5. 外部 fd 接管

先完成平台能力审计，再确定接口行为：

- Unix 可评估 `uv_tcp_open` 接管 socket fd；
- Windows 必须区分 CRT fd 与 Winsock `SOCKET`，现有 `int fd` 接口不能安全表达 64 位原生句柄；
- 若无法在不改变上层 ABI 的前提下可靠接管，Windows 的 `socket_server_bind` 保持明确失败，并在平台扩展中另行设计原生句柄入口；
- 接管时必须明确句柄所有权、重复接管、close 和进程标准输入等非 socket fd 行为。

验证：

- 可支持平台上的 TCP socket 接管和收发；
- 非 socket fd、无效 fd、重复接管和接管后关闭；
- 不支持平台返回稳定失败，不伪造成功事件。

## 分步提交

1. 增加阶段 3 原版行为基线，覆盖 pause/start、shutdown、nodelay、优先级、warning 和 info；
2. 实现 pause/start 与回归测试；
3. 实现 nodelay 和 shutdown/half-close；
4. 实现双优先级写队列；
5. 实现写缓冲 warning；
6. 实现 socket info 快照；
7. 增加原版 UDP 事件和 Lua API 基线；
8. 实现 UDP 生命周期和地址编码；
9. 实现 UDP 收发与默认目标；
10. 审计并实现外部 fd 的平台能力或限制；
11. 在三个平台运行功能、并发、压力和原版对照测试；
12. 记录差异并关闭阶段 3。

每一步独立提交。修改源码、构建、测试或工作流后，推送并手动运行相关 Linux/macOS CI；Windows 使用本机 MSVC 完整验证。

## 验收标准

- pause/start、shutdown、nodelay、优先级、warning、info 和 UDP 均有直接测试；
- 三个平台的常用 `skynet.socket` TCP/UDP 服务无需修改 Lua 代码即可运行；
- Linux 原版与 libuv 版的事件类型、关键字段和因果顺序无未解释差异；
- 多 worker 并发发送、反复暂停恢复、UDP 连续收发和高频关闭稳定通过；
- 没有已知泄漏、双重释放、悬空回调、ID 代际串扰或退出挂起；
- 外部 fd 在支持的平台行为明确，在不支持的平台稳定失败并有文档说明；
- 第三方源码保持干净，补丁可重复应用，skyuv 自有目标保持严格警告和警告即错误。

## 风险与决策

- `uv_shutdown` 只适用于流 handle，half-close 与原版 close 事件时机需用对照测试确定；
- libuv write 一旦提交不能被后来的高优先级数据抢占，兼容目标限定为未提交队列的优先级；
- UDP 地址编码属于 Lua 可见 ABI，必须先从原版测试固化字节布局再实现；
- socket info 是跨线程快照，不返回 runtime 内部链表；
- Windows 外部 fd 的类型宽度是 ABI 限制，不通过截断或强制转换规避。

## 未决问题

开始步骤 1 前没有会改变总体架构的未决问题。以下细节由原版基线确定：

- shutdown 完成、对端 EOF 与本端 CLOSE/ERROR 的精确顺序；
- warning 阈值、重复报告和恢复条件；
- 高低优先级在连续小包下的可观察合并边界；
- UDP 来源地址编码及 IPv4/IPv6 长度；
- `socket_info` 中 closing、reading、writing 和地址文本的精确表示。

