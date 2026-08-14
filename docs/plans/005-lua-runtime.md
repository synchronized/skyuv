# 005：Lua 模块与运行环境

## 状态

进行中。

## 背景

阶段 3 已完成常用 Socket 语义，便携 Skynet 已能在 Windows、macOS 和 Linux 构建。
Windows 可运行最小节点与 Lua TCP echo，Linux 可执行原版与 libuv 版 TCP/UDP 行为对照。

本阶段继续处理上游 Lua C 模块、C 服务和运行环境能力，重点消除 pthread、POSIX socket、
signal、daemon、终端和动态加载路径等平台假设。

当前真实接入状态以 [`LUA_RUNTIME_MATRIX.md`](../LUA_RUNTIME_MATRIX.md) 为准，
逐项实施与验证过程见
[`records/lua-runtime-stage-4.md`](../records/lua-runtime-stage-4.md)。

## 目标

- 形成上游常用 Lua C 模块和 C 服务的跨平台构建清单；
- 让 `lua-clientsocket` 的网络、线程、休眠和控制台能力通过 skyuv 平台边界工作；
- 统一三平台动态模块名称、输出目录和 Lua 搜索路径；
- 明确 signal、daemon、stdin 和 Windows 终端无法等价实现的行为；
- 让主要自带示例在三个目标平台使用一致的启动方式和 Lua API。

## 不在范围内

- 默认启用 TLS 或增加新的外部运行依赖；
- 重写上游 Lua 库或改变公开 Lua API；
- Windows Service、守护进程管理器或完整终端模拟；
- 性能基准、长时间稳定性和发布打包；
- 为示例性质的 `lua-clientsocket` 增加上游没有的新协议能力。

## 已有能力审计

- `lua-socket.c` 已处理 Winsock 头文件与 VLA，并随便携 `skynet` 模块构建；
- `service_harbor` 已处理 VLA、链接 libuv，并在三平台构建；
- Skynet 动态加载已通过 skyuv 平台接口映射到 libuv；
- Windows C 服务与 Lua C 模块已经统一到配置文件声明的无配置子目录；
- Windows signal 与 daemon 采用明确的平台边界，不伪造不可等价的 Unix 行为。

## 已确认决策

### 模块接入

- 不直接修改 `3rd/skynet`；必要源码调整使用可重复应用的独立补丁；
- 每个 Lua C 模块或 C 服务使用独立 CMake 目标，只链接实际需要的平台能力；
- 第三方源码目标不继承 skyuv 自有代码的警告即错误策略；
- 先验证可独立加载和无外部依赖的功能，再扩大到外部系统集成测试。

### client socket 与控制台

- socket 句柄仅保留在 `client.socket` 模块内部，不进入普通 skyuv 公共 API；
- 平台 socket 差异集中在适配边界，不向上游主体扩散；
- Windows 的 pthread mutex/thread 和 `usleep` 映射到 skyuv 平台接口，Unix 使用系统 pthread；
- stdin 后台线程具有明确所有权，EOF、错误和队列满通过返回值诊断；
- 测试成功后应验证正常退出，不以强制关闭掩盖资源错误。

### 信号和进程行为

- Unix 保留 SIGHUP、SIGINT 和 SIGTERM 等可用语义；
- Windows 不伪造 POSIX signal，仅实现稳定的进程内控制和控制台事件映射；
- daemon 在 Windows 明确不支持，Windows Service 不属于本阶段；
- 平台差异通过支持矩阵和测试固化。

## 实施顺序

1. 建立 Lua C 模块、C 服务、示例及其平台依赖清单；
2. 建立 `lua-clientsocket` 独立目标并完成三平台加载；
3. 验证 connect、send、recv、shutdown、close 和错误路径；
4. 重构 stdin 队列生命周期并覆盖输入、EOF 和退出；
5. 接入无外部服务依赖的常用 Lua C 模块；
6. 补齐 gate、harbor、logger 等 C 服务的加载和最小行为；
7. 统一路径、扩展名、多配置输出目录和错误文本；
8. 审计 signal、daemon、Windows 终端与进程退出；
9. 在 Windows、macOS 和 Linux 运行主要示例与重复加载测试；
10. 记录平台差异并关闭阶段 4。

当前主要剩余工作是完成跨平台 cluster 联调与阶段验收审计。

## 测试与验收

- CMake 严格配置与完整构建；
- Lua `require` 动态加载纳入模块，并调用无外部依赖的功能；
- `client.socket` 使用回环服务验证收发、拒绝连接、对端关闭和本端 shutdown；
- stdin 使用管道验证多行、EOF、空行、UTF-8、队列耗尽和退出；
- 重复启动和停止节点，检查线程、socket 与动态库生命周期；
- Linux 对照上游模块的返回值与错误路径；
- Windows 使用本机 VS2022，Linux/macOS 使用手动 CI 完整验证。

阶段完成必须满足：纳入模块三平台可构建、可加载且具备相应功能测试；控制台线程无已知泄漏、
悬空访问或退出挂起；Windows 多配置与单配置生成器使用相同模块布局；Unix signal 行为保留，
Windows 限制明确；第三方工作树保持干净且补丁可重复应用。

## 风险与未决问题

- Winsock `SOCKET` 宽于 `int`，不得沿用 POSIX fd 假设；
- 阻塞 stdin 无法靠普通标志可靠唤醒，需持续验证进程退出与模块生命周期；
- 外部服务模块必须区分构建、加载、协议和完整集成四种验证等级；
- signal 和终端语义无法可靠映射时保持明确限制，不增加脆弱模拟层；
- macOS 回环和 stdin 测试是否暴露新的 pthread、终端或退出差异仍待验证。

## 完成摘要

- 模块与运行环境清单已经建立，大多数默认 Lua C 模块和核心 C 服务已纳入便携构建；
- 无外部服务模块、共享数据模块、gate、harbor、logger 和 Mongo 驱动已形成跨平台测试；
- `client.socket` 已在三平台构建、加载，并使用统一驱动覆盖回环与 stdin 行为；
- signal、daemon、日志重开、路径和进程退出的平台边界已经明确并有相应测试；
- 详细实施过程、验证范围和已知限制见
  [`阶段 4 实施记录`](../records/lua-runtime-stage-4.md)。
