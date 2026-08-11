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

当前项目处于基础设施和技术方案建设阶段，尚未提供可运行版本。

## 获取源码

Skynet 和 libuv 以 Git submodule 固定版本，克隆时需要同时初始化子模块：

```shell
git clone --recurse-submodules https://github.com/synchronized/skyuv.git
```

已有工作副本可以执行：

```shell
git submodule update --init --recursive
```

libuv 已接入 CMake，并提供统一目标 `skyuv::libuv`。默认构建并链接静态 libuv；需要动态版本时可设置 `SKYUV_USE_SHARED_LIBUV=ON`。Skynet 上游没有 CMake 构建入口，后续将在不修改其源码的前提下选择性接入所需核心模块。

## 构建要求

- CMake 3.23 或更高版本
- 支持 C11 的 C 编译器
- Windows、macOS 或 Linux

后续将在核心模块接入后提供完整的 skyuv 构建和运行命令。

当前可以通过 CMake Presets 配置和构建工程，例如：

```shell
cmake --preset windows-vs2022-debug
cmake --build --preset windows-vs2022-debug
```

使用 `cmake --list-presets` 查看当前平台可用的配置。项目自有目标默认启用严格编译警告，并将警告作为错误处理；这些选项不会应用到 `3rd/` 中的第三方目标。

## 参与开发

项目代码采用接近 Skynet 的 C 代码风格：使用制表符缩进、花括号与控制语句同行，并优先保持实现直接、依赖边界清晰。详细协作约定见 [AGENTS.md](AGENTS.md)。

## 许可证

本项目采用 [MIT License](LICENSE)。第三方依赖分别遵循其自身许可证。
