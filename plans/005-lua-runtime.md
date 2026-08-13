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
