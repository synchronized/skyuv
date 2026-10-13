# 版本发布流程

本文说明 skyuv 的版本决策、发布准备、三平台发布和发布后核验。日常开发及代码评审见
[`development.md`](development.md)。0.x 阶段不承诺稳定 ABI；发布范围和限制以对应的版本文档为准。

## 流程概览
本地 CPack 命令、产物位置与 Runtime 组件边界见 [打包指南](packaging.md)。
本文负责版本身份、发布流程与公开资产验收。


```text
确定版本范围 → 准备版本提交 → 合入 main → 三平台 Release 验证
                                             ↓ 全部通过
                                      创建 tag / Release
                                             ↓
                               核对公开资产并记录验收
```

Release workflow 在三平台构建、交付测试和 SHA-256 复核全部通过后创建 tag 与 GitHub Release。
失败时不会进入发布步骤；不要手工移动已发布 tag 或替换公开归档。

## 1. 确定版本身份和范围

- 检查当前版本文档、路线图和未完成计划，决定本次发布是正式版还是预览版，以及实际包含的内容。
- 0.x 阶段按交付范围选择版本：新阶段或能力里程碑使用新的 `0.y.0`，兼容性修复使用递增的
  `0.y.z`；候选验证使用明确的预发布标识，例如 `0.2.0-beta.2`。已有公开版本号和 tag 不复用。
- 版本号与 tag 必须和 `CMakeLists.txt` 中的 `project(skyuv VERSION ...)`、
  `SKYUV_VERSION_PRERELEASE` 一致。
- 在 `docs/versions/vX.Y.Z[-prerelease].md` 固定本次范围、明确未包含项、兼容性声明和发布门槛。
  预览版不得暗示未完成的正式版计划已经验收。
- 将已实现的用户可见变化从 `CHANGELOG.md` 的 `Unreleased` 移入带日期的版本章节；计划中的能力
  不能写成已交付。
- 更新 `docs/versions/README.md`、`docs/README.md` 及必要的发布指南链接。

当前发布版和正式版准备状态见[版本索引](../versions/README.md)。

## 2. 准备并审查版本提交

推荐从最新 `main` 建立短期 `release/vX.Y.Z` 分支，只放版本号、Changelog、版本文档和必要索引
变更；通过 Pull Request 合入 `main`。该分支不长期保留，也不承载新功能。若仓库维护者明确采用
直接推送方式，可在当前分支完成相同审查和验证。

版本文件准备完成并合入 `main` 后，可在仓库根目录执行只读检查：

```powershell
python scripts/check_release.py
```

检查器核对 CMake 版本、版本文档标题和索引、Changelog 章节、干净工作区、`main` 分支与
`origin/main` 同步状态，以及本地和远端是否已有同名 tag。它不替代版本范围审查、构建测试或
GitHub Release workflow，也不会修改文件或 Git 状态。

合入前确认：

- `git diff --check` 通过，版本号、预发布标识、Changelog、版本范围和 tag 完全一致；
- 版本文档写明测试覆盖、限制和后续任务；正式版所有硬性发布门槛均有验收证据；
- 计划索引、版本索引和文档入口没有过期的“当前版本”描述；
- Release 目标提交位于 `main`，工作区干净，相关开发 CI 已通过。

## 3. 执行三平台 Release

发布入口为 `.github/workflows/release.yml`，必须从 `main` 手动触发。先确认 Release 页面没有同名
tag 或版本；Git tag 和 GitHub Release 均不可复用。

可先用 `publish_release=false` 演练构建、测试和 artifact 上传；正式发布时触发：

```powershell
gh workflow run release.yml --ref main `
  -f publish_release=true `
  -f tag=vX.Y.Z
```

例如预览版需填写 `tag=v0.2.0-beta.2`。也可以在 GitHub Actions 页面选中 `main`，填写相同参数。
正式运行时，工作流依次构建 Windows、Linux 和 macOS 发行包，执行 `delivery` 测试并复核全部
SHA-256；三平台全部成功后才创建 GitHub Release。包含连字符的版本会创建为 Pre-release。

持续查看工作流直到最终结论。任一平台失败时，先修复问题、更新版本准备提交并重新验证；不要
绕过失败步骤手工打 tag 或发布不完整资产。

## 4. 发布后核验

确认 GitHub Release 的 tag、目标提交、Pre-release 状态、三个平台归档及 `.sha256` 文件正确，
并确认 Release workflow 的发布 job 成功。工作流会在发布前校验待上传资产；还可从公开 Release
下载归档并按随附校验文件复核。

GitHub Release 创建的远端 tag 不一定自动出现在本地。检查并同步：

```powershell
git ls-remote --tags origin "refs/tags/vX.Y.Z*"
git fetch origin tag vX.Y.Z
git show -s --format="%h %D %s" vX.Y.Z
```

不要仅因本地 `git tag` 没显示就重复创建 Release；先分别检查远端 Git tag 和 GitHub Release。

## 5. 记录结果

发布完成后新增 `docs/records/YYYY-MM-DD-vX.Y.Z-release-acceptance.md`，记录版本、发布提交、tag、
Release/Actions 链接、平台结果、资产名称、校验结果和剩余限制；更新版本文档状态及
`docs/records/README.md`。该提交只补充已发生的发布事实，不更改已打 tag 的代码或替换发布资产。

可在发布后生成待补充的记录草稿：

```powershell
python scripts/new_release_record.py --tag vX.Y.Z
python scripts/new_release_record.py --tag vX.Y.Z-beta.2 --date YYYY-MM-DD
```

生成器拒绝覆盖已有文件。请人工补全提交、Actions 链接、平台结果、资产与 SHA-256、版本内容和限制，
并更新 `docs/records/README.md`、`docs/README.md` 及版本文档状态；草稿本身不是验收证据。

## 发布边界

| 组件 | skyuv 当前声明 | 发布说明要求 |
|---|---|---|
| Runtime | 0.x，未承诺稳定 ABI | 说明三平台已验证的运行行为和平台限制 |
| CMake Consumer | 实验性 | 列出已验证的安装导出、子项目和 Lua 模块接入方式 |
| skyuv 公共 C 接口 | 未冻结 | 不得宣称 ABI 稳定 |
| Skynet 内部头文件与模块 | 内部构建接口 | 不作为稳定 SDK 或任意模块二进制兼容承诺 |
