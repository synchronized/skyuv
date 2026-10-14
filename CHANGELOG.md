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

## 0.2.0-beta.4 - 待发布

本预览版已完成 Windows 本地验收，远端 CI 和三平台发行验收尚待完成，尚未发布。

### 构建与诊断

- Windows Microsoft CRT 工具链按第三方源码归属局部处理 CRT 弃用提示；cmocka 使用
  UTF-8 编码，Clang 两种前端局部处理已审查的 libuv 未使用函数提示。
- 增加 `SKYUV_LIBUV_DIAGNOSTICS` 审查入口，默认关闭已审查的 Windows libuv 日常提示；
  开启后恢复原始诊断，自有目标的严格警告与警告即错误保持开启。
- Windows 构建运行根目录统一为 `build/<preset>/project`，程序与共享 `uv.dll` 放在
  `bin`，服务与 Lua DLL 分别放在 `cservice`、`luaclib`；安装包布局不变。
- CTest 先检查运行文件和 DLL 哈希，缺少依赖时跳过后续运行测试，并验证新目录下真实
  加载 `client.socket` 与 `client.crypt`。

### 修复

- 通过构建副本补丁修复 libuv Windows IPv6 初始化括号、线程亲和性高位移位、
  stat 回退路径恢复及路径起点前读取问题，保持第三方源码工作树不变。
- 进程指标测试增加启动同步，避免子进程先结束导致监控窗口不足。
- Windows 含空格路径测试只复制启动链必需的 DLL，避免无关模块和调试符号复制占用验收时间。

本版不包含固定 Linux 权威性能基线、配置/日志/PID 路径收口或正式 v0.2.0 验收。

## 0.2.0-beta.3 - 2026-10-12

本预览版交付 libuv 客户端网络，不代表 v0.2.0 正式版的全部门槛已满足。
Windows 构建诊断治理仍按 beta.2 计划独立跟踪，未包含在本版完成范围。

### 新增与变化

- 便携 client.socket 改用专属 libuv I/O 线程，支持同步连接、完整发送、有界接收、
  select/poll 就绪等待、半关闭及 Lua GC 清理；整数参数改为模块实例内的连接 ID。
- connect 增加可选的秒级超时参数，失败异常包含目标地址、端口、libuv 错误名和原因。
- 统一 client.dll/client.so，提供 client.socket 与 client.crypt；SHA-1 使用 crypt.sha1。
- 提供默认启用的可选 Lua 解释器目标 skyuv_lua_interpreter，可通过
  SKYUV_BUILD_LUA_INTERPRETER=OFF 关闭，解释器暂不加入安装包。
- stdin 保持行与状态协议，使用进程级共享队列和 libuv 读取，避免 GC 与退出锁等待。

### 兼容边界

- 连接 ID 不是原生 fd/SOCKET，不能交给其他网络库；旧 client/socket.dll 或 socket.so 应清理。
- 读半关闭执行原生适配，缓存消费完统一报告 EOF；Windows 原生 WSAESHUTDOWN 不直接暴露。
- 使用过 stdin 的 client 模块持有进程级加载引用，驻留到进程退出。
- 不包含 DNS、IPv6、异步 Lua API、共享 I/O 线程或稳定 ABI 承诺。

详细约定见 [客户端指南](docs/guides/client-socket.md)。

## 0.2.0-beta.1 - 2026-10-12

本预览版本用于验证新增 socket 就绪轮询能力，不代表 `v0.2.0` 正式版的全部交付范围。详见
[`docs/versions/v0.2.0-beta.1.md`](docs/versions/v0.2.0-beta.1.md)。

### 新增

- 为 `client.socket` 增加 `select` 和 `poll`，支持对整数 socket 句柄等待读写就绪；
- 为 MSVC/clang-cl 下指定的 skyuv 目标增加独立 UTF-8 编译选项，避免代码页 936 的 C4819 警告。

### 已知限制

- `client.socket` 仅覆盖出站 TCP 客户端用途，不等同于完整 lsocket；
- 性能基线、配置/日志/PID 路径收口和独立 Lua 解释器动态模块 `require` 验证仍未完成。

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
