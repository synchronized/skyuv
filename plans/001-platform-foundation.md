# 001：平台基础层

## 状态

进行中。

## 背景

阶段 0 已固定原版 Skynet 在 Linux 上的启动、网络、消息、定时器和关闭行为。下一步需要先把线程、同步、线程局部存储、时间与动态加载集中到 skyuv 平台接口，再替换网络层。这样可以避免平台条件编译和 libuv 类型扩散到 Actor 与消息调度代码。

原子操作由 [002：原子操作抽象](002-atomic.md) 单独实施，两份计划共同构成路线图中的阶段 1。

## 目标

- 提供 thread、mutex、cond、rwlock 和 TLS 接口；
- 提供单调时间、实时时间和线程 CPU 时间接口；
- 提供动态库打开、符号查询、关闭和错误信息接口；
- 在 Windows、macOS 和 Linux 上编译并测试平台层；
- 迁移 Skynet 核心的对应调用，同时保持 Linux 基线行为。

## 不在范围内

- 不替换 `socket_server.c`、epoll 或 pipe；
- 不改变 Actor 调度模型、工作线程数量和消息队列算法；
- 不实现 daemon、signal 或 Windows Service；
- 不做未经测量的锁优化；
- 不在本计划中实现原子操作。

## 设计决策

- 公共接口位于 `include/skyuv/`，实现位于 `src/platform/`；
- 公共接口不暴露 `uv_*`、pthread 或 Windows 原生类型；
- 首版线程与同步后端使用 libuv 提供的跨平台原语；
- 首版自旋锁映射为 mutex，后续依据性能测试决定是否优化；
- 时间轮保持不变，只替换时间来源；
- 线程 CPU 时间允许平台后端返回明确的“不支持”，不得伪造数据；
- 动态加载优先封装 `uv_dlopen`、`uv_dlsym`、`uv_dlclose` 和错误文本。

## 接口范围

- `skyuv_thread_create`、`skyuv_thread_join`；
- `skyuv_mutex_*`、`skyuv_cond_*`、`skyuv_rwlock_*`；
- `skyuv_tls_*`；
- `skyuv_time_monotonic`、`skyuv_time_realtime`、`skyuv_time_thread_cpu`；
- `skyuv_dlopen`、`skyuv_dlsym`、`skyuv_dlclose`、`skyuv_dlerror`。

具体类型应采用不透明存储或内部结构，调用方不得依赖后端布局。

## 实施步骤

1. 建立公共错误约定和平台目标 `skyuv_platform`。
2. 实现 thread、mutex、cond、rwlock 与 TLS，并增加生命周期测试。
3. 增加多线程条件等待、TLS 隔离和重复创建/销毁测试。
4. 实现三类时间来源，验证单调性、合理精度和能力降级。
5. 实现动态加载接口，用测试模块验证符号查询、缺失符号和关闭。
6. 以小规模补丁让 Skynet 核心调用平台接口，不修改网络实现。
7. 在 Linux 上重跑阶段 0 全部行为基线。
8. 增加 Windows MSVC/clang-cl 与 macOS Apple Clang CI。

## 测试方案

- 正常创建、等待和 join 线程；
- mutex、cond、rwlock 的正常路径与重复压力测试；
- 每个线程拥有独立 TLS 值；
- 单调时间不倒退，实时时间处于合理范围；
- 动态库成功加载、符号缺失、路径错误和安全关闭；
- Linux ThreadSanitizer 运行同步层压力测试；
- 阶段 0 的启动、echo、Actor、timer 和关闭测试全部回归。

## 验收标准

- 平台层在 Windows、macOS、Linux 上构建并通过单元测试；
- skyuv 自有目标保持严格警告和警告即错误；
- Skynet 核心中目标范围内不再直接调用 pthread、`clock_gettime` 或 `dlopen`；
- Linux 行为基线无未解释差异；
- 资源初始化失败和关闭路径均有测试；
- 第三方源码仍保持干净，必要改动以补丁管理。

## 风险与回退

- 若 libuv 原语无法表达 Skynet 所需语义，先在 skyuv 接口内部增加最小平台后端，不向调用方暴露差异；
- 若线程 CPU 时间在某平台不可用，返回明确能力错误，并把相关统计标记为不可用；
- 若一次迁移范围过大，按 TLS、线程同步、时间、动态加载顺序独立提交并逐项回归。

## 未决问题

当前没有阻塞实现的问题。具体错误码类型在第一个平台接口提交前确定，并保持所有平台接口一致。

## 完成记录

已开始：

- 建立 `skyuv::platform` 静态库；
- 增加不暴露 libuv 类型的 thread 与 mutex 接口；
- 增加参数校验、状态校验、多线程计数、trylock、join 和重复销毁测试。
- 增加条件变量、读写锁与线程局部存储接口及生命周期测试。
- 增加纳秒精度的单调时间、实时时间和线程 CPU 时间接口；不支持线程 CPU 时间的平台返回明确错误。
- 增加动态库加载、符号查询、错误文本和安全关闭接口，并使用测试模块验证成功与失败路径。
- 将 `skynet_timer.c` 的实时、单调和线程 CPU 时钟映射到 skyuv 时间接口，保持原有单位换算。
- 将 `service_snlua.c` 和 `lua-skynet.c` 的线程 CPU、单调时钟映射到 skyuv 时间接口，完成 Skynet 当前 `clock_gettime` 调用迁移。
- 通过兼容头将 `skynet_module.c` 的动态库打开、符号查询和错误信息迁移到 skyuv 动态加载接口，保持原有模块句柄 ABI。
- 将 skyuv TLS 改为内嵌不透明存储，初始化和销毁不再动态分配，并以编译期断言约束后端大小与对齐。
- 单独将 `skynet_server.c` 的线程局部存储映射到 skyuv TLS 接口，不扩大到调度线程和同步原语。
- 将 `skynet_start.c` 的线程、互斥锁和条件变量映射到 skyuv 平台接口，保持调度循环与唤醒条件不变。
- 按上游模块组成新增 `client.so` CMake 目标，并将 `lua-clientsocket.c` 的线程与互斥锁映射到 skyuv 平台接口。
