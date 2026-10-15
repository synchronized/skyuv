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
[项目路线图](docs/ROADMAP.md)，当前阶段的实施步骤见 [开发计划](docs/plans/README.md)。

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

当前已完成 Linux 兼容基线、跨平台基础层、libuv TCP 最小闭环、常用 Socket 语义兼容，以及 Lua 模块和运行环境。Windows、Linux 和 macOS 均可启动便携 Skynet，并通过 client.socket、gate、harbor、cluster、共享数据和常用 Lua C 模块测试；Linux 会自动对照原版 epoll 与 libuv 版的关键 TCP/UDP 行为。Unix 支持接管已连接 TCP socket fd，Windows 受上游 `int fd` ABI 限制而明确返回失败。已完成三平台长时稳定性验证，并建立固定 VMware 的自身参考基线；正式版收口状态见
[v0.2.0](docs/versions/v0.2.0.md)。

## 获取源码

Skynet 和 libuv 以 Git submodule 固定版本，克隆时需要同时初始化子模块：

```shell
git clone --recurse-submodules https://github.com/synchronized/skyuv.git
```

已有工作副本可以执行：

```shell
git submodule update --init --recursive
```

libuv 已接入 CMake，并提供统一目标 `skyuv::libuv`。Linux 和 macOS 默认链接动态 libuv，确保主程序与动态模块共享同一个进程级运行时；Windows 默认静态链接，也可通过 `SKYUV_USE_SHARED_LIBUV` 显式调整。启用 `SKYUV_BUILD_TESTS` 时会提供测试依赖 `cmocka::cmocka`。Skynet 核心与便携服务通过 CMake 构建，适配补丁仅应用到构建副本，不修改第三方源码工作树。

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

安装 LLVM/Clang 后，也可通过相同脚本加载 Windows SDK 与链接器环境，构建 Clang Debug/Release：

```powershell
.\scripts\msvc.ps1 -Preset windows-clang-debug
.\scripts\msvc.ps1 -Preset windows-clang-release
```

这两个 preset 使用 `clang` 驱动器（MSVC ABI），与已有的 `windows-clang-cl-*`（`clang-cl` 驱动器）分开。

不要在未初始化的普通终端中直接调用 `windows-msvc-*` preset。也可以改用能够自行定位工具链的 `windows-vs2022-*` preset。

## 参与开发

便携客户端模块统一生成 `luaclib/client.dll`（Unix 为 `client.so`），包含
`client.socket` 与 `client.crypt`，SHA-1 通过 `client.crypt.sha1()` 使用。
下游可移除自行构建 crypt 的目标，将 `package.cpath` 指向 `luaclib/?.dll`（Unix 为 `?.so`）。
从旧布局升级时，应删除生成目录或安装目录内的 `client/socket.dll`（Unix 为 `socket.so`），
避免 Lua 优先加载旧模块；源码目录不需要修改。
便携 `client.socket` 使用 libuv 专属 I/O 线程，整数参数是连接 ID，不是原生句柄。
超时、就绪等待、半关闭与 GC 行为见 [客户端指南](docs/guides/client-socket.md)。
独立 Lua 解释器默认启用，CMake 目标为 `skyuv_lua_interpreter`，产物为构建目录
`3rd/lua`（Windows 为 `3rd/lua.exe`，Visual Studio 生成器另含 `Debug` 或 `Release` 子目录）。
例如执行 `cmake --build --preset windows-vs2022-debug --target skyuv_lua_interpreter`。
配置时传入 `-DSKYUV_BUILD_LUA_INTERPRETER=OFF` 可关闭解释器，Lua 库仍保留。
解释器暂不加入安装包。


项目代码采用接近 Skynet 的 C 代码风格：使用制表符缩进、花括号与控制语句同行，并优先保持实现直接、依赖边界清晰。详细协作约定见 [AGENTS.md](AGENTS.md)，文档职责与维护方式见 [文档规范](DOCUMENTATION_GUIDE.md)。

## 许可证

本项目采用 [MIT License](LICENSE)。第三方依赖分别遵循其自身许可证。
