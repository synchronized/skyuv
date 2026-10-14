# 开发流程

本文说明 skyuv 的日常开发、评审和验证流程。代码边界以根目录 [`AGENTS.md`](../../AGENTS.md)
为准，文档职责以 [`DOCUMENTATION_GUIDE.md`](../../DOCUMENTATION_GUIDE.md) 为准；版本发布流程见
[`release.md`](release.md)。

## 任务与计划

- 一个提交或 Pull Request 聚焦一个可独立理解和验证的目标，不混入无关清理。
- 小型修复或文档调整直接按本指南实施；涉及多个依赖步骤、平台行为或交付边界时，先在
  `docs/plans/` 建立计划，写清目标、非目标、步骤和验收条件。
- 开始计划实施前，将计划状态改为“就绪”；开始编码后改为“进行中”。完成后补充验收摘要和
  `docs/records/` 记录，再更新 `docs/plans/README.md` 与必要的路线图状态。
- 先确认改动归属：skyuv 自有代码放在 `src/`、`include/skyuv/`；第三方改动通过可重复应用的
  补丁维护，不直接修改 `3rd/`。

## 分支与提交

`main` 是集成分支。推荐每个完整任务使用一个短期分支，例如 `feat/client-socket-poll`、
`fix/windows-linker` 或 `docs/release-process`；同一任务的设计、实现、测试和文档留在同一分支，
通过一个 Pull Request 合入 `main`。小型维护工作也可按仓库维护者约定直接提交到当前分支。

提交应聚焦一个目的。每轮变更完成相关本地验证后，检查 `git diff --check` 和提交范围，创建提交
并推送当前任务分支；不要把未验证的半成品推到集成分支。推送、合并和发布遵循用户授权及仓库
维护者要求。

## 本地验证与 CI

1. 修改前检查 `git status`，阅读相关代码、测试、CMake 文件、计划和文档，明确复现方式及验收标准。
2. 修改后先运行受影响目标的 CMake 配置、构建和测试；网络生命周期、并发或平台抽象改动扩大到
   对应压力测试和平台矩阵。
3. 源码、CMake、测试、第三方补丁或工作流变更推送后，手动触发相关 Linux/macOS CI，并在本机
   完成 Windows MSVC 验证；持续查看到最终结论。发现失败时，先修复并复验，再开始依赖该结果的
   后续工作。
4. 纯文档修改不要求运行完整 CI；如改动文档索引、相对链接或文档规范，可按需要运行
   Documentation 检查。
5. 只有所需验证通过后才将 Pull Request 合入 `main`。Pull Request 更新或普通推送本身不会替代
   手动触发的完整平台验证。

常用 Windows 命令：

```powershell
.\scripts\msvc.ps1 -Action all -Preset windows-msvc-debug
.\scripts\msvc.ps1 -Action all -Preset windows-clang-debug
```

Linux 和 macOS 使用仓库中的 `linux-*`、`macos-*` CMake preset，并在 GitHub Actions 手动运行
对应工作流。修改范围和平台覆盖应在提交或 Pull Request 说明中列出；无法覆盖的平台要明确标注。

三平台完整构建默认生成 `build/<preset>/project/`，复用 Runtime 安装规则组装
`bin/`、`cservice/`、`luaclib/`、`lualib/`、`service/`、`examples/` 和 `licenses/`。
原编译产物继续由 CMake 管理；Windows 为 DLL 装载保留程序及共享 `uv.dll` 相邻的布局。
独立 Lua 解释器启用时也出现在 `project/bin/`，但不改变 Runtime 发行组件的内容。

例如 Linux 从任意目录运行：

```shell
/path/to/build/linux-clang-release/project/bin/skyuv examples/skyuv.conf
```

使用完整 `cmake --build --preset ...` 构建会执行 `skyuv_stage_runtime`；只构建某个源码目标
不保证部署更新，也可显式构建该目标补齐运行树。源码嵌入时默认不自动组装；顶层可通过
`-DSKYUV_STAGE_PROJECT=OFF` 关闭完整运行树组装。每个 preset 应保持独立构建目录，多配置
生成器按最后部署的配置使用 `project/`，不同时混用 Debug/Release。
Windows CTest 保留运行文件及 DLL 哈希检查，失败时跳过依赖测试，避免缺失 DLL 弹窗。
`runtime.project` 验证三平台目录外启动、echo、Lua 模块加载和删除产物后的再次部署。

## 第三方构建诊断
本地生成发行归档与未压缩安装目录见 [打包指南](packaging.md)。


Windows MSVC 与 clang-cl 日常构建仅对 libuv 目标局部关闭 C4244、C4245、C4267、
C4701、C4152、C4702，
不传播给 skyuv 自有代码。升级依赖或进行诊断审查时恢复提示并全量重建：

```powershell
cmake --preset windows-vs2022-debug -DSKYUV_LIBUV_DIAGNOSTICS=ON
cmake --build --preset windows-vs2022-debug --clean-first
```

该开关默认 OFF；审查结束后重新配置为 OFF 恢复日常策略。Clang GNU 前端、Linux 和
macOS 不受该 MSVC 编号策略影响。Windows Clang 两种前端还通过该开关控制已审查的
libuv unused 标记与 timer.c/core.c 未使用函数例外；ON 时恢复原始提示。
清单外的新警告继续保留；关闭输出不代表
已证明第三方转换安全。诊断历史见 [构建诊断记录](../records/windows-build-diagnostics.md)。
旧转换开关仅在新开关尚未设置时作为初始值迁移；后续使用新开关。
cmocka 在 MSVC/clang-cl 下使用 PRIVATE `/utf-8`，按真实编码消除 C4819，未屏蔽该诊断。

## 完成标准


- 实现、测试、CMake 和文档与任务范围一致，第三方源码工作树干净；
- 相关本地验证及要求的远端 CI 均通过，未覆盖的平台和已知限制已说明；
- `git diff --check` 通过，提交内容单一且可审查；
- 计划状态、路线图摘要和实施记录（如适用）已更新。
