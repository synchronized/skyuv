# 000：Linux 兼容基线

## 状态

进行中。

## 背景

在替换 pthread、epoll 和其他 POSIX 能力之前，需要先证明固定版本的原版 Skynet 能够通过 skyuv 的 CMake 构建，并建立可重复的运行与行为基线。

如果直接开始跨平台替换，构建系统问题、依赖接入问题和行为兼容问题会混在一起，难以判断回归来源。本阶段只改变构建与测试入口，不改变 Skynet 的运行模型和网络实现。

## 目标

- 在 Linux x86_64 上通过 CMake 构建固定提交的 Skynet。
- 接入 Skynet 自带 Lua 和 jemalloc。
- 构建启动最小节点所需的核心服务与 Lua C 模块。
- 提供最小配置和 TCP echo 示例。
- 建立原版 Makefile 构建与 skyuv CMake 构建的行为对照。
- 为后续平台层和网络层替换提供可重复的测试基线。

## 不在范围内

- 不替换 epoll、pipe 或 `socket_server.c`。
- 不替换 pthread、时间、动态加载或原子操作。
- 不支持 Windows 或 macOS 运行 Skynet。
- 不保证所有 Skynet 自带示例和测试均可运行。
- 不接入 TLS、Crashpad 或其他可选能力。
- 不进行网络性能优化。
- 不改变 Skynet 上层 Lua API。

## 首期支持矩阵

| 项目 | 本阶段要求 |
|---|---|
| 操作系统 | Linux |
| 架构 | x86_64 |
| 编译器 | GCC、Clang |
| 构建类型 | Debug、Release |
| 构建系统 | CMake Presets + Ninja |

Linux arm64、musl 和其他 Unix 系统不作为本阶段强制验收环境。

## 兼容目标

- 以主仓库记录的 `3rd/skynet` 提交为唯一上游基线。
- 保持 Lua API、配置格式、服务启动流程和主要可观察行为兼容。
- 本阶段不承诺 Skynet 内部 C ABI 或第三方二进制模块 ABI。
- 动态模块必须基于本工程提供的头文件和工具链重新编译。

## 模块范围

### 必须构建

- `skynet` 主程序；
- `skynet-src` 核心；
- Skynet 自带 Lua；
- jemalloc；
- `snlua`；
- `logger`；
- `gate`；
- Lua 的 `skynet`、`socket` 和最小启动链路所需模块。

### 按依赖纳入

如果 bootstrap 或 echo 示例实际依赖其他模块，应记录原因并以最小集合加入。

### 暂缓

- harbor；
- `lua-clientsocket`；
- 调试控制台；
- 完整 Lua C 模块集合；
- 与最小启动和 echo 无关的示例。

## 设计决策

### CMake 目标边界

- 第三方库目标在 `3rd/` 中定义，不应用 skyuv 的警告即错误策略。
- Skynet 核心和模块由 skyuv 的 CMake 文件显式列出源码，避免使用递归 glob 隐式扩大范围。
- skyuv 自有辅助代码调用 `skyuv_target_compile_warnings()`。
- 构建产物集中在 preset 对应的构建目录，不写入 `3rd/skynet`。

### 上游源码保持干净

本阶段优先通过编译定义、include 路径和 CMake 目标完成接入。确实必须修改上游源码时，按 [`patches/skynet/README.md`](../patches/skynet/README.md) 管理补丁，不直接提交脏的 submodule。

### 分配器

- 默认接入 Skynet 已有的 jemalloc。
- 保留 system allocator 回退路径，便于诊断构建和内存问题。
- 本阶段不引入其他高性能分配器。

### 输出目录

最终目录需要清晰区分：

- 主程序；
- C service 动态库；
- Lua C 模块；
- Lua 脚本和配置；
- 测试程序。

运行示例不得依赖源码目录中的偶然相对路径，应通过 CMake 生成或复制明确的运行目录。

## 影响范围

预计新增或修改：

- 根 `CMakeLists.txt`；
- `3rd/CMakeLists.txt`；
- `cmake/` 中的 Skynet、Lua 和 jemalloc 接入模块；
- `src/` 下的初始构建目标；
- `tests/` 下的基线测试；
- `examples/` 或明确命名的运行示例目录；
- Linux 相关 CMake Presets；
- README 构建和运行说明；
- 必要时新增 `patches/skynet/` 补丁。

## 实施步骤

### 1. 记录上游构建清单

- 解析 Skynet Makefile 和 `platform.mk`。
- 记录核心源文件、模块源文件、编译定义、include 路径和链接库。
- 记录 GCC 与 Clang 的差异。
- 记录 jemalloc 配置参数和符号前缀。

验证：形成可审查的 CMake 源码清单，能够与上游 Makefile 逐项对应。

### 2. 接入 Lua

- 为 Skynet 自带 Lua 建立静态库目标。
- 保持上游使用的编译定义和平台源文件。
- 不修改 Lua 上游源码。

验证：构建一个最小程序，创建并销毁 `lua_State`，执行简单 Lua 表达式。

### 3. 接入 jemalloc

- 确定 CMake 下的构建方式和生成目录。
- 保持 Skynet 使用的 `je_` 符号前缀。
- 建立 jemalloc 与 system allocator 的选择入口。
- 防止生成文件写入 submodule。

验证：分别使用 jemalloc 和 system allocator 构建最小分配测试。

### 4. 构建 Skynet 核心

- 显式列出核心源码。
- 设置 Linux 所需编译定义和链接库。
- 构建 `skynet` 主程序。
- 检查动态模块搜索路径和导出符号。

验证：主程序能够启动并进入配置加载流程。

### 5. 构建最小服务与 Lua 模块

- 构建 snlua、logger 和 gate。
- 构建最小启动链路所需 Lua C 模块。
- 统一模块名称、前缀、后缀和输出目录。

验证：bootstrap、logger 和最小 Lua 服务能够加载。

### 6. 提供最小运行环境

- 增加最小配置文件。
- 增加 TCP echo 服务与客户端或测试驱动。
- 通过 CMake 准备独立运行目录。
- 提供 Debug 和 Release 运行命令。

验证：echo 请求和响应内容一致，进程能够正常停止。

### 7. 建立行为对照

使用同一测试驱动分别连接：

1. 上游 Makefile 构建的 Skynet；
2. skyuv CMake 构建的 Skynet。

记录并比较：

- 启动日志中的关键事件；
- listen、accept、read、write 和 close 行为；
- TCP 消息内容；
- 主动关闭与对端关闭；
- timeout 消息的基本顺序；
- 正常退出行为。

不比较时间戳、地址和线程调度导致的非确定内容。

### 8. 补齐 CI 入口

- Linux GCC Debug/Release 配置和构建；
- Linux Clang Debug/Release 配置和构建；
- 单元测试和 echo 基线测试；
- submodule 干净状态检查。

## 测试方案

### 单元测试

- Lua 初始化和执行；
- 分配器基本分配、释放和重分配；
- 配置路径与模块路径生成。

### 集成测试

- 最小 Skynet 节点启动；
- snlua 和 logger 加载；
- TCP echo；
- 定时器消息；
- 正常关闭和重复启动。

### 行为对照测试

- 同一输入分别运行于 Makefile 和 CMake 产物；
- 比较协议数据和关键事件序列；
- 差异必须分类为构建缺陷、测试非确定性或已接受差异。

### 构建验证

- GCC Debug；
- GCC Release；
- Clang Debug；
- Clang Release；
- jemalloc 模式；
- system allocator 模式。

## 验收标准

- Linux x86_64 上 GCC 和 Clang 均能完成 Debug、Release 构建。
- 构建过程不会修改 `3rd/skynet`、Lua 或 jemalloc submodule。
- 最小节点能够启动、加载 Lua 服务并正常退出。
- TCP echo 测试可以重复执行且无已知泄漏、崩溃或挂起。
- jemalloc 和 system allocator 模式均能构建和运行最小示例。
- Makefile 与 CMake 产物的基线测试没有未解释的行为差异。
- CTest 能统一执行本阶段自动化测试。
- README 包含可复制执行的 Linux 构建和运行命令。

## 风险与回退

### jemalloc 生成流程难以直接纳入 CMake

回退方式：首个可运行基线允许暂时通过 CMake ExternalProject 驱动上游配置流程，但产物必须位于构建目录。不得把生成文件写入 submodule。

### 动态模块符号解析差异

回退方式：先显式导出 Skynet 模块所需符号，并用最小 snlua 加载测试确认；不提前扩大到全部模块。

### 构建范围持续膨胀

回退方式：以 bootstrap 和 TCP echo 的真实依赖为边界，非必要模块转入阶段 4。

### 行为测试不稳定

回退方式：只比较协议数据和关键状态事件，对时间戳、线程号、地址和无序日志做归一化。

## 已确认事项

- 使用 GitHub Actions Ubuntu 24.04 作为首个 Linux CI 和权威验证环境。
- 日常开发在 Windows 进行；Linux 构建和运行暂时通过 CI 反馈，后续可增加 WSL2 缩短调试周期。
- jemalloc 为主要组合，system allocator 至少保留一个 Debug 组合。
- 新增最小、确定性的 skyuv echo 示例，不把交互式上游示例作为自动化基线。
- 兼容性任务同时构建上游 Makefile 和 skyuv CMake 产物。
- gate 只在最小示例实际需要时纳入，否则后移。

## 未决问题

当前没有阻塞阶段 0 实施的未决问题。最低 glibc 兼容范围将在获得 Ubuntu 24.04 基线后，根据实际发布需求单独确定。

## 完成记录

已开始：

- 建立 Ubuntu 24.04 的 GCC/Clang CI；
- CI 在临时目录构建原版 Skynet，避免污染 submodule；
- 增加原版 Skynet 启动冒烟测试；
- 增加 skyuv CMake 配置和 libuv、CMocka 构建验证。

首次基线结果：

- 提交 `f6be8cf` 的 GitHub Actions 运行 `31489707911` 通过；
- GCC Debug 与 Clang Debug 均成功构建；
- 原版 Skynet 能够完成启动冒烟测试；
- libuv 和 CMocka 能够通过 skyuv CMake Presets 构建；
- 构建后主仓库及三个直接 submodule 均保持干净。

Lua 接入进展：

- 按上游 Makefile 的 `CORE_O` 和 `LIB_O` 建立显式 CMake 源码清单；
- 提供静态目标 `skyuv::lua`；
- 按平台保持上游 Lua 编译定义；
- 增加创建 Lua 状态并执行表达式的 CMocka 测试。
- Windows/MSVC 暂不构建定制 Lua；其上游原子操作分支不兼容 MSVC，将由阶段 1 的 `skyuv_atomic` 解决。

分配器接入进展：

- 提供 `SKYUV_ALLOCATOR=jemalloc/system` 选择；
- Linux 默认使用 jemalloc，其他平台在完成适配前默认使用 system；
- jemalloc 在构建目录的源码副本中执行 Autotools，不修改 submodule；
- 两种模式统一提供 `skyuv::allocator`；
- 增加分配、写入、重分配和释放测试。

Skynet 核心接入进展：

- 按上游 Makefile 的 `SKYNET_SRC` 建立显式 CMake 源码清单；
- Linux 基线目标链接 Lua、分配器、线程、动态加载、数学和实时库；
- 保留主程序动态符号导出能力，为后续加载 C service 做准备；
- system allocator 会向 Skynet 传递 `NOUSE_JEMALLOC`，保持上游回退语义。
