# 变更记录

本文件记录面向使用者的公共接口、行为、构建和兼容性变化。当前 skyuv 仍处于 0.x，
`Unreleased` 和未发布版本内容不代表已经形成稳定 ABI。

版本范围和发布门槛见 [`docs/versions/README.md`](docs/versions/README.md)，发布操作见
[`docs/guides/release.md`](docs/guides/release.md)。

## Unreleased

以下内容计划归入下一版本 `v0.2.0`，尚未构成正式发布承诺。

### 新增

- 固定 Linux Runner 性能基线和首批回归阈值；
- 增加配置、日志与 PID 路径约定及迁移验证；
- 增加独立 Lua 解释器动态模块搜索路径下的真实 `require` 验证。

### 修复

- 后续版本继续维护 skyuv 作为 `add_subdirectory` 或 FetchContent 子项目的路径隔离。

## 0.1.0 - 2026-09-29

固定范围和发布限制见
[`docs/versions/v0.1.0.md`](docs/versions/v0.1.0.md)。

### 新增

- 增加 CMake install/export 和外部 Lua C 模块消费示例，支持 `find_package(skyuv CONFIG REQUIRED)`。
- 增加外部消费示例的 Lua runtime smoke test。

### 修复

- 修复 skyuv 作为 `add_subdirectory` 或 FetchContent 子项目时的源码路径假设。

### 已知限制

- 固定 Linux 性能基线、配置/日志/PID 路径约定和独立 Lua 动态模块 `require` 验证移至 `v0.2.0`。

## 0.1.1 - 2026-09-29

固定范围和发布限制见
[`docs/versions/v0.1.1.md`](docs/versions/v0.1.1.md)。

### 变更

- 将主便携运行时的 CMake target 和运行时产物从 `skyuv_skynet_portable` 统一为 `skyuv`。
- 同步更新安装树、启动指南、测试、性能脚本以及三平台 Release workflow。

### 兼容性

- 本版本不改变运行时语义、Lua API 或公共 ABI 承诺；内部 `skyuv_skynet_portable_core` 目标保持不变。

## 0.1.0-beta.1 - 2026-08-19

固定范围和未纳入内容见
[`docs/versions/v0.1.0-beta.1.md`](docs/versions/v0.1.0-beta.1.md)。

### 新增

- 建立 Windows、Linux 和 macOS 的 CMake 构建、安装、CPack 打包和发布工作流。
- 提供跨平台 Skynet 运行时、Lua C 模块和安装树启动验证。

### 变更

- 发行版本由 CMake 数字版本与预发布标识组合生成，本版本为 `0.1.0-beta.1`。
- Beta 阶段不承诺公共 ABI 稳定性；Skynet 内部头文件和内部 Lua C 模块不属于稳定 SDK。

### 已知限制

- 固定 Linux 性能 Runner 基线和配置、日志、PID 路径约定尚未完全收口。
- 外部 Lua C 模块必须匹配 skyuv 的 Lua ABI、编译器、运行库和平台链接方式。
