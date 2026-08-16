# skyuv 依赖决策

## 1. 依赖原则

- 核心运行时保持最小依赖，不为模拟 POSIX 接口引入兼容层。
- 优先使用 libuv 已提供的跨平台能力。
- libuv 未覆盖但操作系统能够稳定提供的能力，由 skyuv 平台层封装。
- 测试、TLS 和崩溃收集等非核心能力必须可选，不能影响最小运行时。
- 所有第三方库通过固定版本引入，并与 skyuv 自有代码的严格警告策略隔离。
- 新增依赖前必须明确许可证、维护状态、平台支持、二进制体积和升级责任。

## 2. 当前依赖

| 依赖 | 类型 | 用途 | 构建条件 |
|---|---|---|---|
| Skynet | 核心上游 | Actor、消息队列、调度、Lua 服务体系 | 始终需要 |
| libuv | 核心运行时 | 网络、事件循环、线程和平台能力 | 始终需要 |
| Lua | 核心运行时 | 使用 Skynet 自带版本 | 始终需要 |
| jemalloc | 运行时分配器 | 内存分配、统计及 Skynet malloc hook | Linux 默认使用，允许 system 回退 |
| CMocka | 测试依赖 | 平台接口、状态机和错误路径单元测试 | `SKYUV_BUILD_TESTS=ON` |

## 3. 明确不引入的兼容层

### pthreads-win32 或 winpthreads

不引入。线程和同步能力由 skyuv 接口封装，并使用 libuv 原语。引入 pthread 兼容层会与 libuv 重叠，使 Skynet 核心继续暴露 POSIX 类型，并增加 MSVC 与 MinGW 的构建差异。

### wepoll

不作为正式网络后端。Skynet 上游包含 MinGW/wepoll 兼容代码，可以用于行为参考，但 skyuv 网络层统一使用 libuv，避免维护两套 Windows 事件后端。

### dlfcn-win32

不引入。动态库加载统一使用 libuv 的 `uv_dlopen()`、`uv_dlsym()` 和 `uv_dlclose()`。

### 完整 POSIX 兼容层

不依赖 Cygwin 等环境。skyuv 的目标是原生支持 Windows，而不是在 Windows 上模拟 Unix 运行环境。

## 4. 内存分配器：统一接口，平台选择后端

Skynet 已经引入 jemalloc，并且 `malloc_hook.c` 和部分内存统计能力使用了 jemalloc 专属接口。
但上游的成熟接入主要面向 Linux：上游 Makefile 在 macOS 明确使用 `NOUSE_JEMALLOC`，其 bundled
jemalloc 构建流程也不提供 skyuv 所需的 Windows/MSVC 与动态模块边界保证。

当前配置为：

```text
Linux   -> jemalloc（默认），可选择 system
macOS   -> system
Windows -> system，通过动态 CRT 共享进程堆
```

这不是要求三个平台永久使用不同 allocator。当前优先统一调用接口、所有权规则和失败语义，后端
由平台及性能证据决定。内部 `skyuv_malloc`、`skyuv_calloc`、`skyuv_realloc`、`skyuv_free` 和
对齐分配接口的基础实现及语义测试已建立；Skynet 跨模块调用尚待迁移。完成迁移后，所有可能跨
主程序与动态模块边界的内存都通过进程唯一实现管理：

```text
C 服务与 Lua C 模块
        -> skyuv 内存接口
        -> 进程唯一实现
        -> Linux jemalloc/system、macOS system、Windows process heap
```

Windows 选择 process heap 是为了让所有模块在静态 CRT 下仍能安全共享内存，不表示将 Windows
API 泄漏到上层。Linux 保留 jemalloc 默认值，以兼容 Skynet 的服务级统计和既有部署行为；macOS
继续使用系统 allocator，除非固定环境性能数据证明有必要引入其他后端。

统一接口需要明确：

- 零尺寸分配、`realloc(ptr, 0)` 和 `free(NULL)` 的语义；
- `calloc` 乘法溢出和对齐参数校验；
- 普通内存与对齐内存的释放接口；
- 分配失败返回 `NULL`，Skynet 兼容层是否保持上游 OOM 终止语义；
- 主程序与动态模块之间分配和释放的归属；
- `malloc_hook.c` 统计接口在不同后端上的能力差异；
- system 回退模式下不支持的统计命令是否能给出明确错误。

统一接口属于内部稳定边界，首版不作为公共 SDK 安装。不得让每个动态模块静态嵌入独立 allocator
实例，也不得用全局 `malloc/free` 宏替换污染 libuv、Lua 或其他第三方代码。

mimalloc 当前不引入。只有现有后端存在无法解决的问题，并且性能数据证明系统分配器不能满足
需求时，才重新评估新的第三方 allocator。

## 5. TLS：后续选择 OpenSSL

libuv 不实现 TLS，Skynet 原生核心也不直接提供 TLS。TLS 应作为 socket 上层的可选模块，不能侵入 `socket_server` 的基础 TCP/UDP 语义。

### OpenSSL

优点：

- 服务器生态成熟，TLS、X.509 和证书工具完整；
- Windows、macOS 和 Linux 支持成熟；
- 更容易兼容现有 Lua TLS 模块和第三方服务；
- 协议问题的排查资料和运维经验丰富。

缺点：

- 体积和 API 规模较大；
- 不是原生 CMake 工程，接入成本较高；
- 需要持续跟进安全更新；
- 与 libuv 集成时需要维护 TLS 状态机和加密前后的缓冲区。

### Mbed TLS

优点：

- 纯 C、体积较小、模块可裁剪；
- CMake 接入直接；
- 源码和 API 相对容易阅读；
- 适合嵌入式和资源受限环境。

缺点：

- 通用服务器和 Lua 生态兼容性通常不如 OpenSSL；
- 裁剪组合需要额外的配置和测试；
- 部分高级协议特性和第三方扩展的选择较少。

### 决策

skyuv 面向通用游戏服务器，不以嵌入式体积为主要目标，因此后续优先选择 OpenSSL。当前不引入，等待原生 TCP/UDP 兼容完成后再增加：

```text
SKYUV_ENABLE_TLS=OFF
SKYUV_TLS_BACKEND=openssl
```

## 6. 崩溃诊断：当前使用系统能力

Crashpad 和 libunwind 不是完全相同的方案：Crashpad 是崩溃报告收集系统，libunwind 主要负责调用栈展开。

### Crashpad

优点：

- 能够捕获崩溃、生成 minidump、保存元数据和管理报告；
- 适合生产环境离线保存和后续上传；
- 能为多个桌面平台提供相对统一的崩溃收集流程。

缺点：

- 依赖和构建体系较重；
- 主要使用 C++，会扩大 skyuv 的语言和工具链边界；
- CMake 接入、handler 进程、符号文件和报告数据库都需要额外维护。

### libunwind

优点：

- C 接口，体积和职责比 Crashpad 小；
- 适合调试命令和 Linux/macOS 下的简单调用栈输出。

缺点：

- 不负责 minidump、报告存储、上传和聚合；
- 不能作为 Windows、macOS、Linux 的完整统一方案；
- 崩溃信号处理器中的安全调用受到严格限制。

### 决策

当前不引入二者，初期按平台使用：

| 平台 | 初期方案 |
|---|---|
| Windows | MiniDumpWriteDump、DbgHelp |
| Linux | 系统 core dump、gdb、addr2line |
| macOS | 系统 crash report、lldb、atos |

如果未来需要线上自动收集三平台崩溃报告，再将 Crashpad 作为独立可选组件引入，例如 `SKYUV_ENABLE_CRASHPAD=OFF`。如果只需要 Linux/macOS 运行时栈输出，可以单独评估 libunwind。

## 7. 测试框架：选择 CMocka

### Unity

优点：

- 核心只有少量 C 源文件，体积很小；
- 纯 C、移植和嵌入简单；
- 适合小型模块和受限环境。

缺点：

- mock、fixture、过滤和报告能力较弱；
- 完整 mock 通常还需要 CMock 或自行实现；
- 网络和平台抽象测试需要编写较多辅助设施。

### CMocka

优点：

- 纯 C，只依赖标准 C 库；
- 支持 GCC、Clang、MSVC 和 MinGW；
- 内置 mock、参数检查、预期调用和 group fixture；
- 支持 TAP 和 xUnit XML 等 CI 友好格式；
- 适合平台抽象、状态机和失败路径测试。

缺点：

- 比 Unity 稍重；
- mock 宏需要学习；
- 过度使用 mock 会使测试与内部实现绑定。

### 决策

选择 CMocka，并且只在测试构建中启用。CMocka 负责：

- 平台接口单元测试；
- libuv 回调到 Skynet 事件的状态机测试；
- 分配失败和系统调用失败路径；
- 未完成 connect/write 时关闭等生命周期边界。

CTest 继续负责测试注册和执行，并承载真实 TCP/UDP、多线程并发、Skynet 行为对照及压力测试。mock 测试不能替代真实集成测试。

## 8. 依赖引入时机

| 阶段 | 新增依赖 |
|---|---|
| 当前基础设施 | CMocka，仅测试构建 |
| Linux 兼容基线 | 不新增，接入已有 Lua 和 jemalloc |
| 平台层与 libuv 网络 | 不新增 |
| TLS 扩展 | 可选 OpenSSL |
| 跨平台生产诊断 | 按需求可选 Crashpad |

未经路线图阶段目标或实际测试数据证明，不提前引入可选依赖。
