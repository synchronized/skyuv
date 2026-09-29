# 发布验收

本文是 skyuv 维护者执行 Beta 或正式发布前的检查清单。当前 0.x 不承诺稳定 ABI；完成本清单
也不能替代版本文档中对具体 component 和限制的声明。

## 1. 确认发布身份

- 在根目录 `CMakeLists.txt` 的 `project(skyuv VERSION ...)` 设置数字版本；
- 用相邻的 `SKYUV_VERSION_PRERELEASE` 设置 Beta 或其他预发布标识；
- 确认组合版本、CPack 文件名和预期标签一致；
- 将面向使用者的变化写入根目录 `CHANGELOG.md`，将固定范围写入对应版本文档。

当前 Beta 版本的唯一身份由 [`v0.1.0-beta.1.md`](../versions/v0.1.0-beta.1.md) 维护；后续正式版
必须建立新的版本文档和对应验收记录。

## 2. 逐项确认交付边界

| 组件 | 当前状态 | 发布前证据 |
|---|---|---|
| Runtime | Beta | 三平台安装树启动、TCP echo、Lua 模块和正常退出测试 |
| CMake Consumer | 实验性 | 安装 package、`add_subdirectory` 和外部 Lua C 模块 smoke test |
| skyuv 公共 C 接口 | 未冻结 | 不得在发布说明中标记为稳定 ABI |
| Skynet 内部头文件与模块 | 未包含 | 仅作为内部构建实现，不进入稳定 SDK 承诺 |

## 3. 本地验证

在干净构建目录执行对应平台的 Release preset：

```powershell
cmake --preset windows-vs2022-release
cmake --build --preset windows-vs2022-release
ctest --preset windows-vs2022-release -L delivery
```

Linux 和 macOS 使用各自的 `linux-gcc-release`、`macos-clang-release` preset。随后检查安装树
不包含源码或构建目录绝对路径，并生成 CPack 归档：

```powershell
cpack --config build/windows-vs2022-release/CPackConfig.cmake -C Release
```

## 4. CI 与发布工作流

发布提交必须通过 Linux、macOS 和 Windows 的完整构建与交付验证。正式入口是
`.github/workflows/release.yml`，手动运行时：

- `publish_release=false`：只构建、测试和上传临时 artifact；
- `publish_release=true`：在三平台成功后创建 GitHub Release；
- `tag` 必须与 CMake 计算出的版本一致，例如当前版本使用 `v0.1.0-beta.1`。

发布工作流会复核三个归档及其 SHA-256。预发布标签会创建为 GitHub pre-release；不要手工移动
已经公开的标签或替换已发布归档。

## 5. 发布后记录

发布完成后新增 `docs/records/YYYY-MM-DD-vX.Y.Z-release-acceptance.md`，记录版本、提交、Release
链接、三平台 artifact、测试结论、已知限制和未完成计划，并在对应版本文档中链接该记录。
