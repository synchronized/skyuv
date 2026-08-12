# 003：libuv TCP 最小闭环

## 状态

就绪。

## 背景

阶段 1 已完成线程、同步、TLS、时间、动态加载和原子操作的平台抽象。阶段 2 开始替换 Skynet 的 Linux/POSIX 网络实现，但继续保留 `socket_server.h`、`skynet_socket.c`、Actor 调度和消息投递语义。

上游 `socket_server.c` 同时承担 socket ID 分配、跨线程命令、网络轮询、TCP/UDP 状态机、写队列和统计。直接一次性重写风险过高，因此本计划只建立 TCP 最小闭环，并按“生命周期骨架、监听、连接、收发、关闭”的顺序推进。

## 目标

- 使用一个专用网络线程和一个 `uv_loop_t`；
- 使用线程安全命令队列与 `uv_async_t` 替换 pipe 控制通道；
- 保持 `socket_server.h` 对上层可见的类型、函数签名和整数 socket ID；
- 支持 TCP listen、accept、connect、start、read、send、close、error 和 exit；
- 将 libuv 回调转换为 `SOCKET_OPEN`、`SOCKET_ACCEPT`、`SOCKET_DATA`、`SOCKET_CLOSE`、`SOCKET_ERR` 和 `SOCKET_EXIT`；
- 在 Windows、macOS 和 Linux 上通过独立测试，并在 Linux 上与原版事件序列对照。

## 不在范围内

- UDP；
- `bind` 外部文件描述符；
- pause、shutdown、half-close 和 nodelay 的完整语义；
- 高低优先级写队列；
- direct write 优化；
- `SOCKET_WARNING`、socket info 和流量统计；
- 主机名 DNS 解析和 Happy Eyeballs；首期地址只接受 IPv4/IPv6 字面量及监听通配地址；
- 修改 Actor 调度器或 Lua socket API。

暂不支持的接口必须返回明确失败或产生可诊断错误，不得静默伪装成功。其完整实现属于计划 004。

## 设计决策

### 线程与 loop 所有权

- Skynet 现有 socket 线程调用 `socket_server_poll`，该线程同时作为 libuv loop 所属线程；不额外创建第二个网络线程。
- `uv_loop_t`、TCP handle、connect/write 请求和关闭状态只由 socket 线程创建、操作和销毁。
- worker 线程只分配 socket ID、封装命令、转移必要数据所有权、入队并调用 `uv_async_send`。
- `uv_async_send` 只表示“可能有命令”，命令数据始终保存在队列中。

### 命令队列与唤醒

- 首版使用 mutex 保护的 FIFO 多生产者单消费者队列，以正确性和可审查性优先。
- 入队和唤醒遵循“持锁入队、解锁、`uv_async_send`”；网络线程每次回调必须持续取出命令直到队列为空。
- 不依赖一次命令对应一次 async 回调，允许 libuv 合并多次唤醒。
- exit 命令进入同一 FIFO，确保此前已经成功入队的命令先被观察；进入停止状态后拒绝新命令。

### socket ID 与代际

- 对外 ID 继续为 `int`，内部保留槽位索引和 generation/tag，迟到回调必须同时匹配完整 ID 和当前对象。
- ID 经原子状态从 `INVALID` 转为 `RESERVE` 后即可返回给 worker；网络线程创建 handle 成功后进入具体状态。
- `uv_close` 开始后槽位进入 `CLOSING`，只有 close callback 完成并清理全部请求后才能回到 `INVALID`。
- ID 复用不得让旧 connect、read、write 或 close 回调作用于新连接。

### 对象与缓冲区所有权

- socket 槽位拥有 TCP handle；handle 内存一直保留到 close callback。
- connect 请求由对应 socket 拥有，connect callback 后释放。
- read 回调为每条 `SOCKET_DATA` 分配独立缓冲区；成功投递后所有权转移给 `skynet_socket.c`，错误和关闭路径自行释放。
- `socket_server_send` 接收成功后立即取得 `SOCKET_BUFFER_MEMORY/OBJECT` 的所有权；无论排队、写成功、写失败或关闭都必须且只能释放一次。
- `SOCKET_BUFFER_RAWPOINTER` 不转移原始指针所有权；进入异步队列前必须复制内容，避免调用方生命周期结束。
- 每个 `uv_write_t` 拥有自己的数据描述和释放函数，write callback 是正常完成路径的唯一释放点；未提交给 libuv 的待发送命令由关闭或退出流程释放。

### 事件队列与 poll

- libuv 回调不得直接调用 Actor 层，只生成内部事件并压入网络线程独占的事件 FIFO。
- `socket_server_poll` 优先返回已经排队的事件；无事件时驱动 `uv_run`，直到出现事件或完成退出。
- `more` 表示当前事件队列或 loop 中存在可立即处理的工作，不承诺与 epoll 返回批次数量一致。
- 错误文本由事件对象拥有，直到 `socket_server_poll` 返回；`SOCKET_DATA` 的数据所有权按现有接口转移。

### TCP 首期语义

- listen 命令成功入队后返回预留 ID；由于 libuv handle 只能由 loop 线程操作，实际 bind/listen 失败通过 `SOCKET_ERR` 异步报告。这项与原版同步返回 `-1` 的差异必须记录，并在计划 004 决定是否增加同步确认机制。
- accept 产生 `SOCKET_ACCEPT`，其中监听 ID 放在 `id`，新连接 ID 放在 `ud`；新连接在收到 `start` 前不读取数据。
- connect 成功产生一次 `SOCKET_OPEN`，失败产生一次 `SOCKET_ERR` 并进入异步关闭；首期只连接 IP 字面量。
- EOF 产生一次 `SOCKET_CLOSE`；非 EOF 读取错误产生一次 `SOCKET_ERR`。
- 主动 close 最终产生一次 `SOCKET_CLOSE`，重复 close 不得重复释放或重复产生终止事件。
- 首版所有 send 都经过命令队列和 `uv_write`，不实现 worker 线程 direct write。

## 影响范围

- 新增 skyuv 自有 socket server 实现，建议位于 `src/socket/`；
- 公共或内部接口放在 `include/skyuv/` 或 `src/socket/` 私有头中，不把 `uv_*` 类型暴露给 Skynet；
- CMake 新增独立网络目标和测试目标；
- 跨平台 Skynet 目标改用新实现，Linux 原版基线继续编译上游 `socket_server.c`；
- `3rd/skynet` 不直接修改，必要接入变化继续使用补丁或构建目标替换。

## 实施步骤

1. 固定原版 TCP 事件序列基线，覆盖监听、连接、收发、连接失败、对端关闭、主动关闭和退出。
2. 定义内部 socket 状态、ID generation、命令类型、事件类型及所有权注释。
3. 实现 create、命令 FIFO、async 唤醒、poll、exit 和 release，不创建 TCP 连接；增加多生产者与重复启停测试。
4. 实现 ID 预留、listen、accept 和 start，验证新连接在 start 前不读数据。
5. 实现 connect 成功、拒绝连接、无效地址及取消中的关闭。
6. 实现 read 和 `SOCKET_DATA` 所有权转移，覆盖 EOF、读取错误和消息投递失败清理。
7. 实现单一优先级 send 与 `uv_write` 生命周期，覆盖连续写、并发 worker 写和写中关闭。
8. 实现 close、迟到回调防护、退出时遍历关闭 handle，并确保 `uv_loop_close` 成功。
9. 将新 socket server 接入跨平台 Skynet 目标，运行 Windows/macOS/Linux TCP echo。
10. 与 Linux 原版逐项对照事件顺序，记录首期有意保留到计划 004 的差异。

每一步应独立提交并通过相应单元测试，不在同一提交中同时引入状态机、TCP 收发和上层接入。

## 测试方案

### 基线与功能

- 监听成功、端口占用和非法地址；
- accept 后 start 前不读，start 后正常收包；
- connect 成功、连接拒绝和地址解析失败；
- 单包、连续小包和大于单次 read buffer 的数据；
- worker 并发 send，接收内容完整且每个缓冲区只释放一次；
- 对端正常关闭、主动关闭、重复关闭和连接中关闭；
- exit 时分别存在 listener、已连接 socket、待 connect 和待 write。

### 并发与生命周期

- 多生产者同时提交命令，验证无丢命令和 FIFO 边界；
- 高频分配和复用 socket ID，注入迟到回调验证 generation 防护；
- 重复创建、运行、退出和 release 至少 100 次；
- 大量连接建立/断开和写中关闭压力测试；
- 记录分配/释放计数，验证所有失败路径归零。

### 平台验证

- Windows MSVC 本机完整构建与测试；
- Linux GCC/Clang Debug/Release、AddressSanitizer 和适用的 ThreadSanitizer；
- macOS Apple Clang Debug/Release；
- Linux 原版与 libuv 版 TCP 事件序列及 echo 行为对照。

## 验收标准

- 三个平台均可运行基于新网络层的 TCP echo；
- `socket_server.h` 首期支持接口无需修改上层调用代码；
- 连接、读取、写入、关闭和退出没有已知泄漏、双重释放、悬空回调或挂起；
- socket ID 快速复用时迟到回调不会命中新对象；
- 多 worker 并发提交命令和发送数据稳定通过压力测试；
- Linux 与原版在首期事件类型、关键字段和顺序上无未解释差异；
- 暂不支持的接口均具有明确且已测试的失败行为；
- 第三方子模块保持干净，skyuv 自有目标保持严格警告和警告即错误。

## 风险与回退

- libuv 异步关闭可能让错误事件和 close callback 次序与 epoll 实现不同；以原版基线约束外部事件，内部释放等待 close callback。
- async 唤醒可能合并；队列消费以“直到为空”为准，不以回调次数计数。
- send 所有权最容易出现双重释放；先建立释放计数测试，再接入 Skynet 对象缓冲区。
- 若完整 `socket_server.h` 兼容层阻碍早期测试，先通过私有内部接口验证状态机，但不得让临时接口进入上层 Lua。
- 每个实施步骤保持独立提交；出现回归时回退最近能力，不回退阶段 1 平台基础层。

## 未决问题

当前没有阻塞骨架实现的问题。以下行为将在步骤 1 的原版基线测试中确定，而不是凭文档假设：

- connect 失败、对端复位和写失败时 `SOCKET_ERR`/`SOCKET_CLOSE` 的精确顺序；
- 主动 close 时是否需要等待已入队写完成后再报告关闭；
- `more` 在连续事件下的精确兼容要求；
- 地址字符串和错误文本的外部可见格式。

若基线结果改变对外状态机，先更新本计划再实现对应逻辑。

## 完成记录

已开始：

- 新增直接注册 `PTYPE_SOCKET` 的原始 TCP 事件基线服务，不经过高层 `skynet.socket` 状态封装；
- 基线覆盖 listener open、accept、accepted start、双向 data、主动 close、对端 close 和 connect error；
- 在服务内部断言关键字段、负载内容和每条连接的因果顺序；
- Linux 完整矩阵同时运行 CMake 版和原版 Skynet 基线。
- 首次运行确认 listener 在建立和 start/resume 时可能收到重复 `SOCKET_OPEN`；基线记录该语义，但只启动一个测试客户端。
