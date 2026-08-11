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

当前没有 Skynet 补丁。
