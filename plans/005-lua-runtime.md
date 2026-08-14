# 005：Lua 模块与运行环境

## 状态

进行中。

## 背景

阶段 3 已完成常用 Socket 语义，便携 Skynet 已能在 Windows、macOS 和 Linux 构建，Windows 可运行最小启动与 Lua TCP echo，Linux 可执行原版与 libuv 版 TCP/UDP 行为对照。

当前便携构建只纳入启动所需的 `skynet` Lua C 模块及 `snlua`、`logger`、`harbor` 服务。`lua-clientsocket` 仍直接依赖 pthread、POSIX socket、`fcntl` 和 `usleep`，控制台线程也没有可控退出协议；其他上游 Lua C 模块尚未形成跨平台构建、加载和最小功能基线。信号、daemon、终端和路径行为仍有平台降级。

## 目标

- 形成上游常用 Lua C 模块和 C 服务的跨平台构建清单；
- 让 `lua-clientsocket` 的网络、线程、休眠和控制台能力通过 skyuv 平台边界工作；
- 统一三平台动态模块名称、输出目录和 Lua 搜索路径；
- 明确 signal、daemon、stdin 和 Windows 终端无法等价实现的行为；
- 让主要自带示例在三个目标平台使用一致的启动方式和 Lua API。

## 不在范围内

- TLS、数据库驱动或外部服务的可用性验证；
- 重写上游 Lua 库或改变公开 Lua API；
- Windows Service、守护进程管理器或完整终端模拟；
- 性能基准、长时间稳定性和发布打包，它们分别属于阶段 5 和阶段 6；
- 为示例性质的 `lua-clientsocket` 增加上游没有的新协议能力。

## 已有能力审计

- `lua-socket.c` 已通过补丁处理 Winsock 头文件与 VLA，并随便携 `skynet` 模块构建；
- `service_harbor` 已处理 VLA、链接 libuv，并在三平台构建；
- Skynet 动态加载已通过 skyuv 平台接口映射到 libuv；
- Windows C 服务与 Lua C 模块已经统一到配置文件声明的无配置子目录；
- Windows signal 与 daemon 当前采用最小兼容边界，daemon 明确失败。

## 设计决策

### 模块接入

- 不直接修改 `3rd/skynet`；必要源码调整使用可重复应用的独立补丁；
- 每个 Lua C 模块或 C 服务使用独立 CMake 目标，只链接实际需要的 skyuv 平台能力；
- 第三方源码目标不继承 skyuv 自有代码的警告即错误策略，但 skyuv 适配代码继续严格编译；
- 先按“可独立加载并执行最小功能”接入，再扩大到依赖外部系统的功能。

### client socket 与控制台

- socket 句柄仅保留在 `client.socket` 模块内部，不进入普通 skyuv 公共 API；
- 平台 socket 差异集中在适配头或小型平台实现中，不把 Winsock 条件编译扩散到上游主体；
- pthread mutex/thread 和 `usleep` 映射到 skyuv 平台接口；
- stdin 后台线程必须有明确所有权；模块卸载和进程退出不得留下访问已释放 Lua userdata 的线程；
- 队列满、EOF 和读取错误应返回可诊断状态，不得由模块直接终止整个进程。

### 信号和进程行为

- Unix 保留 SIGHUP 等上游可用语义；
- Windows 不伪造 POSIX signal，只有能稳定映射的进程内控制才进入公共行为；
- daemon 在 Windows 继续明确不支持；Windows Service 不属于本阶段；
- 启动脚本和测试负责可控终止常驻 Skynet，不依赖强制关闭掩盖资源错误。

## 实施步骤

1. 建立 Lua C 模块、C 服务、示例及其平台依赖清单，固化动态加载基线；
2. 为 `lua-clientsocket` 建立独立目标，先完成三平台编译和模块加载；
3. 替换其 socket、线程、锁和休眠依赖，验证 connect/send/recv/shutdown/close；
4. 重构 stdin 队列生命周期，覆盖输入、EOF、队列满和退出；
5. 接入不依赖外部服务的常用 Lua C 模块，并逐个增加加载与最小功能测试；
6. 审计并补齐 gate/harbor 等主要 C 服务的加载和最小行为；
7. 统一路径、扩展名、多配置输出目录和错误文本；
8. 审计 signal、daemon、Windows 终端与进程退出，记录支持矩阵；
9. 在 Windows、macOS 和 Linux 运行主要示例、并发退出和重复加载测试；
10. 记录差异并关闭阶段 4。

## 测试方案

- CMake 严格配置与完整构建；
- Lua `require` 动态加载每个纳入模块，并调用无外部依赖的最小 API；
- `client.socket` 使用回环服务验证完整收发、拒绝连接、对端关闭和本端 shutdown；
- stdin 使用可控管道输入验证多行、EOF、空行、长行、队列耗尽和退出；
- 重复启动/停止模块和 Skynet 节点，检查线程、socket 与动态库生命周期；
- Linux 对照上游模块的返回值与错误路径；
- Windows 使用本机 VS2022，Linux/macOS 使用手动 CI 完整验证。

## 验收标准

- 纳入范围的模块在三平台可构建、可加载，并有最小功能测试；
- `client.socket` 回环网络 API 在三平台保持一致，错误可诊断；
- 控制台线程和队列没有已知泄漏、悬空访问或退出挂起；
- Windows 多配置生成器与单配置生成器使用同一配置文件即可找到模块；
- Unix signal 行为保留，Windows 不支持行为明确失败；
- 主要自带示例无需平台专属 Lua 修改即可运行；
- 第三方工作树保持干净，所有补丁可重复应用。

## 风险与回退

- Winsock `SOCKET` 宽于 `int`，不得沿用 POSIX fd 假设；模块内部应使用可容纳原生句柄的类型；
- 阻塞 stdin 无法靠普通标志可靠唤醒，必要时将控制台读取限制为进程生命周期资源并明确卸载限制；
- 部分 Lua 模块依赖外部库或服务，只验证构建与加载会产生虚假完成感，必须在清单中区分测试等级；
- 信号和终端语义平台差异较大，无法可靠映射时保持明确限制，不增加脆弱模拟层。

## 未决问题

开始步骤 1 前没有会改变整体架构的未决问题。以下细节通过清单与基线测试确定：

- 阶段 4 纳入的“常用 Lua C 模块”精确集合及外部依赖等级；
- stdin 线程是否能够在不改变 Lua API 的条件下支持模块级卸载；
- Windows 控制台 EOF、编码和管道输入的可观察差异；
- 上游 signal 命令中哪些行为属于服务内信号，哪些依赖操作系统信号。

## 完成记录

- 已完成上游默认 C 服务、Lua C 模块和运行环境能力清单，见 [`docs/LUA_RUNTIME_MATRIX.md`](../docs/LUA_RUNTIME_MATRIX.md)；
- 清单确认 `snlua`、`logger`、`harbor` 和最小 `skynet` 模块已有可运行基础，首个实现缺口为 `client` 模块。
- 已增加独立 `client.socket` 动态模块目标，统一三平台输出为
  `luaclib/client/socket`；Windows 已完成编译、动态加载和 API 表面验证；
- stdin 后台线程改为首次调用 `readstdin` 时才启动，仅 `require` 模块不再因
  重定向 stdin 的 EOF 直接终止 Skynet。网络功能和完整线程退出协议仍待后续验证。
- `client.socket` 的 Windows 句柄已改为模块内部保留原生 `SOCKET` 宽度，
  Lua 边界通过 `lua_Integer` 往返；Windows 回环测试已覆盖 connect、send、
  非阻塞 recv、写 shutdown 和 close，既有 echo 与模块加载测试保持通过。
- stdin 队列不再从后台线程调用 `exit(1)`：EOF、读取错误和队列溢出通过
  `readstdin` 的可选第二返回值报告；空行和 CRLF 得到正确处理，分配失败可诊断，
  EOF/错误后回收线程句柄。Windows 管道测试已覆盖普通行、空行、UTF-8 和 EOF。
- 已接入独立 `bson` 动态模块；Windows 实际编解码测试覆盖嵌套文档、数组、
  UTF-8、布尔值、null 和固定 ObjectID。ObjectID 所需的原子操作与进程 ID
  由既有 skyuv 兼容边界提供，不修改上游源码。
- 已接入 `sproto.core` 与其 schema 解析依赖 `lpeg`；Windows 实际测试从文本
  schema 创建协议，覆盖整数、字符串、布尔值、数组、UTF-8、普通编码解码
  以及 pack/unpack 压缩往返，两模块均使用上游内置源码且不新增外部依赖。
- 已接入 `md5.core` 并沿用上游 `md5.lua` 封装；Windows 实际测试覆盖标准摘要
  向量、HMAC-MD5、按位异或、固定种子加解密往返及参数错误，不新增外部依赖。
- `skynet` 聚合模块已补入 `skynet.crypt` 与 SHA-1 实现；Windows 实际测试覆盖
  SHA-1、HMAC-SHA1、Base64、Hex、循环异或、DES、随机密钥及错误输入。
  `random/srandom/getpid` 的平台差异集中在既有 POSIX 兼容头中。
- `skynet` 聚合模块已补入 `skynet.stm`；Windows 实际测试由两个独立服务
  共享同一对象，覆盖首次读取、无变更读取、写入更新及跨服务更新可见性，
  同时经过服务退出和对象回收流程。
- `skynet` 聚合模块已补入 `skynet.sharetable.core`，使用内置 Lua 的共享对象
  扩展；Windows 实际测试覆盖嵌套只读表、UTF-8、从 Lua 源码加载、选择性
  批量查询以及保留既有代理引用的版本替换。
- `skynet` 聚合模块已补入 `skynet.sharedata.core`；Windows 实际测试覆盖共享
  对象新建与查询、嵌套只读代理、深拷贝隔离、两轮更新传播、被删除子节点的
  旧代理失效、对象删除及刷新回收。
- `skynet` 聚合模块已补入 `skynet.datasheet.core`；Windows 实际测试覆盖
  builder 创建数据、两个独立服务查询同一数据、根与嵌套代理原位更新、数组
  和 UTF-8 访问、表到标量的结构变化、旧嵌套代理失效及服务退出释放。
- `skynet` 聚合模块已补入 `skynet.netpack`；Windows 实际测试覆盖二字节
  大端长度头、空包、含零字节与 UTF-8 的载荷、65535 字节上限、超限错误，
  并通过 `tostring` 验证 C 缓冲区向 Lua 字符串转移后的释放路径；`filter/pop`
  已由 `snax.gateserver` 真实 TCP 联调覆盖，测试同时发送拆分帧与合并帧，验证
  半包、粘包、二进制载荷和回显所有权。
- 上游 C `gate` 服务已纳入便携构建并在 Windows 直接加载；实际回环测试覆盖
  监听、accept、拆分长度头、watchdog 文本事件、`PTYPE_CLIENT` 回写、连接关闭
  通知和监听句柄关闭，确认旧 C 服务路径不依赖 POSIX 网络接口。
- Windows 双节点 harbor 已完成最小联调：节点 1 同时运行 cmaster、cslave 和
  全局服务，节点 2 完成握手后查询全局名并执行跨节点 Lua RPC，随后验证节点
  掉线通知。联调同时修复 `socket.start` 对已连接 socket 的服务所有权转移语义，
  防止握手后数据继续投递给已调用 `socket.abandon` 的原 Lua 服务。
- harbor 异常生命周期测试确认上游 cmaster 在自身生命周期内保留已注册节点，
  因此相同 harbor ID 重启会被明确拒绝，不提供透明热重连；使用新 ID 的替代
  节点可以正常加入并继续访问既有全局服务。该流程在 Windows 连续运行 10 次，
  未出现端口占用、残留进程或 socket 生命周期污染。
- `skynet` 聚合模块已补入 `skynet.memory`。便携运行时保留 `total`、`block`、
  `current`、`info`、`jestat`、`mallctl`、dump 和 profiling 等完整 Lua API；当前
  后端不伪造上游 malloc hook 的服务级统计，基础查询返回零、服务表为空且
  profiling 保持关闭，诊断调用给出明确的系统分配器提示。
- `skynet` 聚合模块已补入 `skynet.multicast.core`；Windows 单节点集成测试使用
  两个 Actor 订阅同一频道，验证共享消息向两个订阅者发布并各自释放引用，
  随后覆盖取消订阅、仅剩订阅者接收以及频道删除流程。
- `skynet` 聚合模块已补入 `skynet.cluster.core`；Windows 测试先固定名称判断、
  节点名、trace 帧和成功/失败响应的协议编解码，再启动两个独立节点，覆盖
  cluster 监听、服务注册、连接建立、按名称寻址以及携带 UTF-8 数据的跨节点
  RPC。cluster sender 持有长连接，测试在确认结果后由驱动统一终止节点。
- `skynet` 聚合模块已补入 `skynet.debugchannel`；上游源码通过既有 skyuv
  自旋锁兼容头避免 MSVC 误用 GCC `__sync_*` 内建函数。Windows 实际测试覆盖
  channel 创建与连接、双向 FIFO 读写、包含零字节和 UTF-8 的命令、空队列、
  双端垃圾回收，以及安装、触发和移除 Lua count hook。
- `skynet` 聚合模块已补入 `skynet.mongo.driver`。Windows 无服务器测试覆盖
  MongoDB OP_MSG 长度、请求 ID、操作码、标志、BSON 载荷封装、短响应拒绝和
  `skynet.db.mongo` 高层模块加载；独立 Linux 手动/每周工作流使用固定 MongoDB
  8.0 服务容器验证连接、插入、查询、更新、唯一索引、删除和资源关闭。由于
  GitHub Actions 服务容器仅支持 Linux，macOS 当前只承担构建回归。
- `client.socket` 回环测试已扩大到 connect、send/recv、写 shutdown、对端关闭、
  拒绝连接和无效 shutdown 模式，并改为成功后执行 Skynet 正常 ABORT；stdin
  测试同样等待节点正常退出，不再以强制终止作为成功条件。Linux 使用同一 Lua
  夹具和回环驱动对照上游与便携版的可观察行为。
- Windows 路径测试会从含空格的临时目录加载 C 服务和 Lua C 模块。非 ASCII DLL
  路径受内置 Lua `package.loadlib` 的 ANSI Windows 接口限制，当前明确记录为不支持；
  若后续消除此限制，应以独立 Lua 补丁或自定义模块搜索器处理。
- 修正 daemon 平台实现选择：Linux 和 macOS 便携目标重新使用上游
  `skynet_daemon.c`，Windows 保持不支持并在配置了 `daemon` 时以非零状态退出，
  同时输出包含 pidfile 的明确诊断；Windows 自动测试覆盖该失败路径。
- 增加跨平台日志控制模块 `skyuv.control`，其 `reopen_log()` 向 logger 投递
  `PTYPE_SYSTEM`，Windows 已完成模块加载和调用测试。Unix 的 `SIGHUP` watcher
  位于现有 socket libuv loop 中，只向 socket 兼容层产生进程信号事件，再复用
  同一日志重开动作；Linux CI 通过重命名日志、发送 `SIGHUP` 和核对新旧文件验证
  实际重开语义。Skynet 服务内部 `SIGNAL` 命令未修改。
- `gate` 与 C `gate` 的运行测试已统一为跨平台 Python 驱动，三平台使用相同
  TCP 客户端覆盖拆分帧、合并帧、二进制载荷、回显和连接关闭；Linux/macOS
  工作流显式构建便携 Skynet、`gate` 服务及其运行依赖，不再只验证编译。
- 双节点 `harbor` 联调已统一为跨平台 Python 驱动，三平台覆盖节点握手、全局名
  传播、跨节点 RPC、掉线通知、重复 ID 拒绝，以及使用新 ID 的替代节点加入；
  测试独占固定端口并在所有路径回收四个子进程。
- `logger` 文件输出测试在三平台使用相同配置和 Python 驱动，覆盖含空格目录、
  UTF-8 日志内容，以及调用 `skyuv.control.reopen_log()` 前后的实际文件写入；
  Linux 继续通过 SIGHUP 测试验证日志轮转后不会写回旧文件。
- `logger` 错误路径测试使用确定不存在的父目录，验证文件无法创建时 Skynet
  以非零状态退出、输出 `Can't launch logger service`，且不会遗留目标文件；
  该用例不依赖管理员权限、只读挂载或平台 ACL 差异。
- `bson` 测试已补齐错误输入，覆盖非表参数、数字字典键、不支持的值类型、
  非法 UTF-8、错误 ObjectID 长度、非法有序字典、未知 BSON 子类型和循环引用；
  连续触发错误后再次完成编码解码，验证临时缓冲区清理不会破坏模块状态；
  测试已改用跨平台 Python 启动驱动并纳入 Linux/macOS CTest。
- 第一批无外部服务模块已接入三平台运行测试：`sproto` 覆盖 schema 与编解码，
  `md5` 覆盖摘要、HMAC 和加解密，`skynet.crypt` 覆盖 SHA-1、Base64、Hex、
  XOR 与 DES；`lpeg` 新增独立夹具覆盖模式组合、捕获、完整匹配、UTF-8 和零字节。
