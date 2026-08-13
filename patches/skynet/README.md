# Skynet 补丁管理

`3rd/skynet` 必须保持为可直接更新的干净 Git submodule。无法通过适配层、编译定义或源码选择解决的上游修改，以补丁形式保存在本目录。

## 使用原则

- 只修补跨平台接入所必需的代码。
- 网络实现整体替换时，优先在 skyuv 中提供替代源文件，不修补上游 `socket_server.c`。
- pthread、时间和动态加载等分散调用，可以通过小型补丁改为调用 skyuv 平台接口。
- 每个补丁只处理一个明确目的。
- 不包含格式化、重命名或与跨平台无关的整理。
- 能够提交 Skynet 上游的通用修复，应优先按上游可接受的形式设计。

## 命名约定

```text
0001-platform-thread-abstraction.patch
0002-platform-time-abstraction.patch
0003-platform-module-loader.patch
```

编号表示应用顺序。移除补丁后不复用编号，避免历史记录含义变化。

## 补丁说明

每个补丁需要在提交信息中说明：

- 修改原因；
- 对应的 Skynet 上游提交；
- 无法通过 skyuv 外部适配完成的原因；
- 影响的平台和模块；
- 验证方法；
- 是否计划提交上游。

## 生成方式

在临时 Skynet 分支上完成单一修改后，使用 `git format-patch` 生成补丁。不要直接编辑生成后的补丁来隐藏实际提交内容。

示例：

```shell
git -C 3rd/skynet format-patch --stdout BASE..HEAD > patches/skynet/0001-description.patch
```

生成后恢复 submodule 到主仓库记录的提交，并确认：

```shell
git submodule status
git -C 3rd/skynet status --short
```

## 应用与验证

- CMake 配置阶段不得静默修改 submodule。
- 补丁应由显式准备命令应用到构建目录中的源码副本，或由开发者明确执行的脚本应用。
- CI 必须从干净 submodule 验证全部补丁能够按编号顺序应用。
- 应用补丁后必须运行对应计划中的构建和测试。
- 升级 Skynet 前先验证无补丁基线，再逐个重新应用并审查语义。

## 当前状态

- `0001-Fix-ownership-of-variable-temporary-buffers.patch`：将 Actor 核心中
  7 个无上限 VLA 改为具有明确生命周期和失败处理的堆缓冲区，同时消除
  `_try_open` 的长度截断风险。对应 Skynet 提交
  `2251550a785480fb04c343da1eb8b42f9a8484fd`；仅应用到构建目录的源码副本。
- `0005-Port-client-socket-headers-to-Windows.patch`：为示例客户端 socket
  增加 Winsock 头文件、非阻塞设置、关闭和错误处理，并把 stdin 线程延迟到
  首次调用 `readstdin` 时创建，避免仅加载模块就在无控制台环境中提前退出。
  补丁只应用到构建目录副本；句柄宽度和 stdin 完整退出协议仍由计划 005
  后续步骤处理。
- `0006-Preserve-client-socket-native-handle-width.patch`：让 `client.socket`
  在 Windows 内部始终使用 Winsock `SOCKET`，仅在 Lua 边界与 64 位
  `lua_Integer` 转换，避免经由 32 位 `int` 截断原生句柄；Unix 继续使用 fd。
- `0007-Make-client-stdin-queue-diagnostic-and-reclaimable.patch`：让客户端
  stdin 队列在 EOF、读取错误和队列满时返回诊断状态，不再由后台线程直接
  终止进程；保留空行并在 EOF/错误后 join 已结束线程。
- `0008-Make-netpack-pointer-arithmetic-standard-C.patch`：将 `lua-netpack.c`
  中两处 GNU C `void *` 指针运算改为显式的字节指针运算，使 MSVC 可编译；
  不改变缓冲区偏移、所有权或 Unix 行为，适合提交上游。
