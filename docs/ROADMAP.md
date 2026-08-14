# skyuv 路线图

## 1. 项目目标

skyuv 使用 libuv 和独立的平台适配层替换 Skynet 的 Linux/POSIX 专属能力，使 Skynet 能够运行在 Windows、macOS 和 Linux 上，同时尽可能保持以下行为兼容：

- Actor/Service 并发模型；
- 全局消息队列和工作线程调度；
- 服务间消息传递；
- Skynet 时间轮与 timeout 消息；
- `skynet.socket` 等上层 Lua API；
- Linux 上已有的部署和运行能力。

项目不是使用 libuv 重新设计一个类似 Skynet 的框架。libuv 负责跨平台 I/O、事件通知和部分平台能力，Skynet 继续负责 Actor 和消息调度。

## 2. 总体技术路线

采用“保留 Skynet 核心，替换平台边界”的方案：

```text
Skynet 上层
├── Actor 与 context
├── 消息队列和调度器
├── Lua 服务体系
├── 时间轮
└── skynet_socket API
          │ 保持接口和行为
          ▼
skyuv 替换层
├── 基于 libuv 的 socket_server
├── 线程和同步原语
├── 原子操作
├── 时间来源
├── TLS
└── 动态模块加载
          │
          ▼
libuv 与编译器/操作系统能力
```

关键决策：

1. 保留 Skynet 的 Actor、消息队列、工作线程调度和时间轮。
2. 保持 `socket_server.h` 的上层契约，使用 libuv 重写其实现。
3. 首期每个 Skynet 节点使用一个专用网络线程和一个 `uv_loop_t`。
4. 使用 `uv_async_t` 和跨线程命令队列替换 pipe 唤醒机制。
5. 所有 libuv handle 仅由所属网络线程创建、操作和关闭。
6. 平台能力通过 skyuv 接口封装，不让 libuv 或操作系统类型扩散到 Skynet 核心。
7. 第三方源码原则上保持不变，必要修改维护为小规模、可追踪的补丁。

## 3. 功能替换范围

| 能力 | Skynet 当前实现 | skyuv 方案 | 影响范围 |
|---|---|---|---|
| 网络轮询 | epoll、kqueue | 重写 `socket_server`，使用 libuv TCP/UDP/async API | 高 |
| 跨线程网络命令 | pipe | 命令队列和 `uv_async_t` | 高 |
| 工作线程 | pthread | `skyuv_thread`，内部使用 `uv_thread_t` | 中 |
| mutex/cond/rwlock | pthread | skyuv 同步接口，内部使用 libuv 原语 | 中 |
| TLS | `pthread_key_t` | `skyuv_tls`，内部使用 `uv_key_t` | 低 |
| 原子操作 | GCC/MSVC 内建实现 | `skyuv_atomic` 多编译器后端 | 高 |
| 单调与实时时钟 | `clock_gettime` | skyuv 时间接口 | 中 |
| 线程 CPU 时间 | `CLOCK_THREAD_CPUTIME_ID` | 分平台实现，允许能力降级 | 中 |
| 动态模块 | `dlopen/dlsym` | `uv_dlopen/uv_dlsym` | 中 |
| 守护进程 | fork、文件锁 | Unix 保留；Windows 单独定义运行方式 | 低，后置 |
| 信号处理 | POSIX signal | 按平台实现或明确降级 | 低，后置 |
| Lua C 模块 | POSIX socket、unistd、pthread | 核心闭环后逐个适配 | 中，后置 |

## 4. 网络层设计

### 4.1 接口边界

保留以下调用关系：

```text
Lua socket API
    ↓
skynet_socket.c
    ↓
socket_server.h
    ↓
skyuv 的 libuv 实现
```

上层继续使用整数 socket ID 和 `socket_message`，不得接触 `uv_tcp_t`、`uv_udp_t` 等 libuv 类型。

### 4.2 线程模型

- 网络线程独占 `uv_loop_t` 和全部 libuv handle。
- worker 线程把 listen、connect、send、close 等操作写入命令队列。
- worker 线程通过 `uv_async_send()` 唤醒网络线程。
- 网络线程消费命令、调用 libuv，并把完成事件转换为 Skynet socket 消息。
- `uv_async_send()` 只负责唤醒，不承载命令数据。

### 4.3 事件兼容

需要将 libuv 回调映射为原有事件：

| libuv 行为 | Skynet 事件 |
|---|---|
| accept 完成 | `SOCKET_ACCEPT` |
| connect 完成 | `SOCKET_OPEN` |
| TCP 收到数据 | `SOCKET_DATA` |
| UDP 收到数据 | `SOCKET_UDP` |
| EOF 或正常关闭 | `SOCKET_CLOSE` |
| 网络异常 | `SOCKET_ERR` |
| 写缓冲超过阈值 | `SOCKET_WARNING` |

实现前需要记录原版在连接失败、半关闭、主动关闭和写失败等情况下的事件顺序，并用兼容性测试固定行为。

### 4.4 socket ID 和生命周期

- 继续对外提供整数 socket ID。
- 内部记录 socket 状态、类型和 generation，避免迟到事件命中已复用的 ID。
- `uv_close()` 是异步操作，只有 close callback 完成后才能释放 handle 所在内存。
- 关闭连接时必须处理未完成的 connect、read、write 和命令请求。
- 退出 loop 前必须完成或取消全部 handle，避免资源泄漏和退出挂起。

### 4.5 写缓冲所有权

所有发送请求进入网络线程。首版不保留跨线程直接写优化，以行为正确和所有权清晰为优先目标。

需要明确：

- send 命令入队后缓冲区所有权何时转移；
- `uv_write_t` 完成后由谁释放缓冲区；
- 关闭连接时如何清理尚未完成的写请求；
- 高低优先级写队列如何保持原有语义；
- 如何依据 libuv 写队列大小产生 `SOCKET_WARNING`。

完成兼容性和压力测试后，再评估批量发送、减少复制和直接写优化。

### 4.6 外部 socket 接管

Skynet 的 `socket.bind(fd)` 使用 `int` 传递外部 socket。在 Unix 上，原生文件描述符可以由 `int` 无损表示；在 64 位 Windows 上，`SOCKET` 通常具有指针宽度，直接经过 `int` 传递可能发生截断。因此，`SKYUV_SOCKET_EXTERNAL_FD` 仅作为迁移阶段的保护开关，不作为长期公共能力或最终平台抽象。

最终方案分为内部接口和 Skynet 兼容边界两层：

- skyuv 内部定义 `skyuv_os_socket`，底层对应 `uv_os_sock_t`；
- `skyuv_socket_runtime_bind()`、bind 命令和 socket 条目均使用 `skyuv_os_socket`，不再假设原生句柄是非负整数；
- Unix 和 Windows 的网络线程统一通过 `uv_tcp_open()` 接管有效的原生 socket；
- Skynet 原有 `int fd` 接口仅保留在兼容层，不允许该类型进入 skyuv socket 核心；
- Unix 兼容层可将 `int fd` 无损转换为 `skyuv_os_socket`；
- Windows 兼容层不得把旧接口中的 `int` 解释为 `SOCKET`，该入口应明确返回失败；
- skyuv 提供显式的平台扩展接口，例如 `skyuv_socket_server_bind_native()`，供 Windows 调用者传递完整宽度的原生 socket。

所有权规则：

- 接管成功后，原生 socket 的所有权转移给 skyuv/libuv，调用者不得再次关闭或继续操作；
- 接管失败时，所有权仍属于调用者；
- 重复接管同一原生 socket 必须失败；
- 关闭由所属网络线程发起，并以 libuv close callback 完成为生命周期终点。

实施顺序：

1. 引入 `skyuv_os_socket` 以及无效值、相等比较等辅助定义。
2. 修改内部 bind 接口、命令载荷、条目字段和测试，覆盖两个平台的成功、失败、重复接管及关闭所有权。
3. 在 Skynet 兼容层保留旧 ABI，并增加完整宽度的 native 扩展入口。
4. 删除 `SKYUV_SOCKET_EXTERNAL_FD` 及对应条件编译，让平台实现自然决定能力。

不对 Skynet 第三方源码中的所有 `int fd` 做全局替换，避免句柄类型扩散到 Lua API、消息结构和无关模块。

## 5. 平台基础层

### 5.1 线程与同步

skyuv 提供自己的接口，内部映射到 libuv：

- `skyuv_thread` → `uv_thread_t`；
- `skyuv_mutex` → `uv_mutex_t`；
- `skyuv_cond` → `uv_cond_t`；
- `skyuv_rwlock` → `uv_rwlock_t`；
- `skyuv_tls` → `uv_key_t`。

首版自旋锁使用 mutex 实现，待性能数据证明必要后，再考虑基于原子操作的短临界区自旋锁。

### 5.2 原子操作

项目不直接把 C11 `<stdatomic.h>` 作为统一跨平台接口，因为 MSVC 的 C 模式尚未完整、稳定地实现标准原子库。

建立 `skyuv_atomic` 抽象，至少覆盖：

- load/store；
- exchange；
- compare exchange；
- fetch add/sub；
- 指针原子操作；
- acquire、release、acq_rel 和 relaxed 内存序。

后端规划：

| 编译器 | 后端 |
|---|---|
| GCC | `__atomic_*` 内建函数 |
| Clang/Apple Clang | `__atomic_*` 或 `__c11_atomic_*` |
| MSVC | `_Interlocked*` intrinsic |

迁移时先对照 Skynet 当前 `atomic.h` 的实际语义，不进行机械替换。消息队列、引用计数和 socket ID 分配必须有并发测试。

### 5.3 时间系统

保留 Skynet 时间轮，只替换时间来源：

- 单调时钟；
- 实时时钟；
- 线程 CPU 时间。

不使用大量 `uv_timer_t` 替换时间轮，避免改变定时器顺序、精度、性能和消息语义。

### 5.4 动态模块

模块加载优先使用 `uv_dlopen()`、`uv_dlsym()` 和 `uv_dlclose()`。同时处理不同平台的动态库扩展名、搜索路径和错误文本。

## 6. 上游源码管理策略

- `3rd/skynet` 和 `3rd/libuv` 保持为固定提交的 Git submodule。
- libuv 通过其 CMake 目标接入，不应用 skyuv 的警告即错误策略。
- skyuv 使用自己的 `socket_server` 实现，构建时排除上游 `socket_server.c`。
- pthread、时间和动态加载等分散调用，优先通过小型补丁改为调用 skyuv 平台接口。
- 补丁集中存放并附带用途说明，不提交修改后的第三方工作树。
- 升级 Skynet 时必须重新应用补丁并运行兼容性测试。

第三方库的选择依据、启用条件和替代方案见 [依赖决策](DEPENDENCIES.md)。

## 7. 分阶段路线图

各阶段进入实施前会在 [`plans/`](../plans/README.md) 中形成可执行计划。阶段 0 至阶段 3 已完成，下一步为阶段 4：Lua 模块和运行环境。

### 阶段 0：建立 Linux 兼容基线

目标：通过 CMake 构建并运行未经跨平台改造的 Skynet，形成行为对照基线。

交付物：

- Skynet 核心、Lua 和必要依赖的 CMake 目标；
- 最小启动配置和 echo 示例；
- socket、timer、消息调度的基线测试；
- 记录当前 Skynet 上游提交和构建参数。

验收标准：

- Linux 上可以启动、加载 Lua 服务并正常退出；
- TCP echo 服务可以稳定工作；
- 原版 Makefile 与 CMake 构建的主要行为一致。

### 阶段 1：平台基础层

目标：在不改变网络实现的前提下移除 Skynet 核心中的主要 pthread、时间和动态加载依赖。

交付物：

- thread、mutex、cond、rwlock 和 TLS 抽象；
- atomic 多编译器后端；
- 单调时间、实时时间和线程 CPU 时间接口；
- 动态模块加载接口；
- 对应的单元测试和并发测试。

验收标准：

- Linux 上替换前后调度和计时测试一致；
- Windows 和 macOS 可以编译并通过平台层测试；
- ThreadSanitizer 可用平台上不出现已知数据竞争。

### 阶段 2：libuv TCP 最小闭环

目标：完成基于 libuv 的最小 TCP 网络链路。

首期支持：

- listen、accept；
- connect；
- read、send；
- close 和 error；
- 跨线程命令队列与唤醒；
- socket ID 生命周期管理。

暂不支持：

- UDP；
- 高低优先级写队列；
- direct write 优化；
- half-close；
- socket warning；
- 外部 fd bind。

验收标准：

- 三个平台上的 echo 服务均可运行；
- 支持多 worker 并发发送；
- 重复连接、断开和退出无已知泄漏或挂起。

### 阶段 3：补齐 Skynet socket 语义

目标：使常用 `skynet.socket` 行为与上游实现兼容。

交付物：

- pause/start；
- shutdown 和 half-close；
- nodelay；
- UDP；
- 高低优先级写队列；
- 写缓冲 warning；
- socket info；
- 外部 fd/bind 的支持方案或明确的平台限制。

验收标准：

- 每项能力都有原版与 skyuv 的对照测试；
- 错误、关闭和重连事件顺序保持一致或有明确差异说明；
- 常用 Skynet 网络服务无需修改 Lua 代码即可运行。

### 阶段 4：Lua 模块和运行环境

目标：处理核心网络之外的 POSIX 依赖，形成完整可用的开发环境。

范围包括：

- `lua-clientsocket` 和控制台输入；
- `lua-socket` 的平台相关调用；
- `service_harbor`；
- 动态 Lua C 模块加载；
- 文件路径与动态库扩展名；
- 信号处理；
- Windows 进程和终端行为。

信号与管理边界：

- Skynet 的服务内部 `SIGNAL` 命令属于 Actor 控制机制，不依赖操作系统信号，
  三平台原样保留；
- Linux 和 macOS 的 `SIGHUP` 由现有 socket libuv loop 上的 `uv_signal_t` 接收，
  转换为 logger 的 `PTYPE_SYSTEM` 消息，不在异步信号处理函数中操作 Actor；
- Windows 不模拟 Unix `kill -HUP`，使用 `skyuv.control.reopen_log()` 显式请求日志重开；
- Unix 的 `SIGINT/SIGTERM`，以及 Windows 的 Ctrl+C、Ctrl+Break 和控制台关闭，
  均由 libuv watcher 转换为 Actor 全量退役请求，完成退役后三个平台均返回 0；
- Windows Service 的停止、关机等 SCM 通知不属于控制台信号，留待独立服务宿主接入；
- libuv watcher、socket handle 与 async handle 统一遵守 loop 线程和 close callback 生命周期。

验收标准：

- 主要自带示例和测试可运行；
- Lua 服务在三个平台上的启动方式和 API 基本一致；
- 不支持的平台特性有明确错误，而不是静默失效。

### 阶段 5：性能和稳定性

目标：验证长期运行、并发正确性和相对上游的性能影响。

测试范围：

- 大量连接建立和断开；
- 多 worker 并发发送；
- 写队列积压与背压；
- socket ID 快速复用；
- 关闭时存在未完成写请求；
- loop 退出和资源释放；
- Actor 消息公平性；
- Linux 原版与 skyuv 的吞吐、延迟、CPU 和内存对比。

验收标准将在形成基线数据后确定，不预先承诺未经测量的性能比例。

### 阶段 6：跨平台交付

目标：形成可持续验证和发布的工程。

交付物：

- Linux GCC/Clang CI；
- Windows MSVC/clang-cl CI；
- macOS Apple Clang CI；
- ASan、UBSan 和适用平台上的 TSan；
- 安装、打包和最小运行示例；
- 平台限制与迁移说明。

## 8. 主要风险

### 网络事件语义差异

epoll/kqueue 与 libuv 在 EOF、错误、半关闭和连接完成事件上的表达不同。应先用测试记录上游行为，再实现映射。

### 异步关闭与内存生命周期

libuv handle 必须等待 close callback 才能释放。socket ID 复用、迟到回调和未完成写请求容易引起 use-after-free。

### 跨线程写入性能

首版所有写请求进入网络线程，可能增加队列竞争和延迟。优化必须建立在性能测量和正确性测试之上。

### 原子内存序

错误的内存序可能只在高并发或特定 CPU 架构出现。应保持接口集中、语义明确，并在不同架构运行压力测试。

### 上游升级成本

Skynet 没有稳定的内部平台 ABI。应限制补丁范围，并在升级时自动执行兼容性测试。

### 平台能力不完全对等

Unix daemon、signal、外部 fd 等能力在 Windows 没有直接对应物。项目应提供合理替代或明确限制，不强行模拟不可靠的语义。

## 9. 暂不纳入范围

- 使用 libuv 重写 Actor 调度器；
- 为每个 Actor 创建独立 `uv_loop_t`；
- 用 `uv_timer_t` 替换 Skynet 时间轮；
- 首版实现跨线程直接写优化；
- 在核心网络闭环前实现完整 Windows Service；
- 为兼容低版本编译器使用 MSVC 实验性 C11 atomics。

## 10. 完成定义

当满足以下条件时，可以认为 skyuv 达到首个可用版本：

- Windows、macOS 和 Linux 均能构建并启动 Skynet；
- Actor、消息队列、工作线程调度和时间轮保持工作；
- 常用 TCP、UDP 和 Lua socket API 可用；
- 主要兼容性、并发、生命周期和压力测试通过；
- 第三方依赖版本、补丁和平台限制均有记录；
- 三个平台具备持续集成验证。
