# skyuv

skyuv = Skynet + libuv。

skyuv 旨在使用 libuv 替换 Skynet 中依赖 Linux 的底层能力，使这个经典的游戏服务器框架能够运行在 Windows、macOS 和 Linux 上，同时保留原生的 Actor 并发模型、消息队列和服务调度机制。

## 项目目标

- 使用 libuv 实现跨平台网络与事件驱动能力。
- 为线程、锁、条件变量、信号量和线程局部存储等能力提供跨平台适配。
- 保持 Skynet 的 Actor 模型、消息调度语义和上层 Lua API 尽可能兼容。
- 使用 CMake 提供统一的跨平台构建流程。
- 将上游依赖与 skyuv 自有代码分离，降低同步 Skynet 和 libuv 更新的成本。

## 设计原则

- **上层兼容优先**：跨平台改造应尽量限制在网络层和平台适配层。
- **调度机制不变**：libuv 负责 I/O 与平台能力，不取代 Skynet 的 Actor 调度器。
- **第三方代码只读**：`3rd/` 中的源码原则上不直接修改；必要变更以可追踪的补丁维护。
- **逐步替换**：网络与并发平台层分别设计、实现和验证。
- **行为一致**：除平台本身差异外，各系统上的可观察行为应尽可能一致。

完整文档索引和推荐阅读顺序见 [文档中心](docs/README.md)。技术路线见
[项目路线图](docs/ROADMAP.md)，当前阶段的实施步骤见 [开发计划](plans/README.md)。

## 计划中的目录结构

```text
skyuv/
├── 3rd/                 # Skynet、libuv 等第三方依赖
├── cmake/               # CMake 模块和辅助脚本
├── include/             # skyuv 公共头文件
├── src/
│   ├── integration/     # Skynet 接入与替换逻辑
│   ├── network/         # 基于 libuv 的网络实现
│   └── platform/        # 线程、同步原语等平台适配
└── tests/               # 跨平台测试
```

当前已完成 Linux 兼容基线、跨平台基础层、libuv TCP 最小闭环和常用 Socket 语义兼容。Windows 已能启动最小 Skynet 节点并运行 Lua TCP echo；Linux 会自动对照原版 epoll 与 libuv 版的关键 TCP/UDP 事件序列。UDP、写队列优先级与 warning、socket info 均已实现；Unix 支持接管已连接 TCP socket fd，Windows 受上游 `int fd` ABI 限制而明确返回失败。下一阶段将处理核心网络之外的 Lua 模块和运行环境。

## 获取源码

Skynet 和 libuv 以 Git submodule 固定版本，克隆时需要同时初始化子模块：

```shell
git clone --recurse-submodules https://github.com/synchronized/skyuv.git
```

已有工作副本可以执行：

```shell
git submodule update --init --recursive
```

libuv 已接入 CMake，并提供统一目标 `skyuv::libuv`。Linux 和 macOS 默认链接动态 libuv，确保主程序与动态模块共享同一个进程级运行时；Windows 默认静态链接，也可通过 `SKYUV_USE_SHARED_LIBUV` 显式调整。启用 `SKYUV_BUILD_TESTS` 时会提供测试依赖 `cmocka::cmocka`。Linux 上已开始在不修改 Skynet 源码的前提下，通过 CMake 选择性构建其核心模块。

## 构建要求

- CMake 3.23 或更高版本
- 支持 C11 的 C 编译器
- Windows、macOS 或 Linux

Linux 上可以使用 GCC Debug preset 配置、构建和测试：

```shell
cmake --preset linux-gcc-debug
cmake --build --preset linux-gcc-debug
ctest --preset linux-gcc-debug
```

运行 CMake 版 Skynet 最小启动示例：

```shell
build/linux-gcc-debug/3rd/skynet build/linux-gcc-debug/3rd/skyuv-smoke.conf
```

Linux 还提供 `linux-clang-debug`、`linux-gcc-release`、`linux-clang-release` 和 `linux-gcc-system-debug` preset。最后一个组合使用系统分配器，其余组合默认使用 jemalloc。

Windows 已支持最小 Skynet 节点和 Lua TCP echo 验证。Visual Studio 生成器可以直接运行：

```shell
cmake --preset windows-vs2022-debug
cmake --build --preset windows-vs2022-debug
```

使用 `cmake --list-presets` 查看当前平台可用的配置。项目自有目标默认启用严格编译警告，并将警告作为错误处理；这些选项不会应用到 `3rd/` 中的第三方目标。

在普通 PowerShell 中使用 Ninja + MSVC 时，通过包装脚本自动加载 Visual Studio 开发环境：

```powershell
.\scripts\msvc.ps1 -Preset windows-msvc-debug
```

不要在未初始化的普通终端中直接调用 `windows-msvc-*` preset。也可以改用能够自行定位工具链的 `windows-vs2022-*` preset。

## 参与开发

项目代码采用接近 Skynet 的 C 代码风格：使用制表符缩进、花括号与控制语句同行，并优先保持实现直接、依赖边界清晰。详细协作约定见 [AGENTS.md](AGENTS.md)，文档职责与维护方式见 [文档规范](DOCUMENTATION_GUIDE.md)。

## 许可证

本项目采用 [MIT License](LICENSE)。第三方依赖分别遵循其自身许可证。
