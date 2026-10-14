# 本地运行时打包指南

本文说明如何从源码生成本地运行时归档、校验文件及可选的安装目录。所有命令从仓库根目录
执行；打包使用项目的 CMake 安装规则与 CPack，不手工压缩构建目录。
公开版本的准备与发布见 [版本发布流程](release.md)，解压、启动与业务迁移见
[运行时发行包指南](runtime-distribution.md)。

## 选择构建组合

| 平台 | 推荐 Release preset | libuv |
|---|---|---|
| Windows | `windows-vs2022-release` | 默认静态；自定义共享构建需携带 `uv.dll` |
| Linux | `linux-gcc-release` | 共享 |
| macOS | `macos-clang-release` | 共享 |

编译环境与常规验证见 [开发流程](development.md)。构建与 CPack 的配置必须一致：
Release 构建使用 `-C Release`；多配置生成器尤其不能省略或混用该参数。

## Windows 打包

在 PowerShell 中执行，先通过脚本把 Visual Studio 编译环境导入当前进程：

```powershell
. ./scripts/msvc.ps1 -Action configure -Preset windows-vs2022-release
cmake --preset windows-vs2022-release -DSKYUV_USE_SHARED_LIBUV=OFF
cmake --build --preset windows-vs2022-release --parallel 4
ctest --preset windows-vs2022-release -L delivery --output-on-failure
cpack --config build/windows-vs2022-release/CPackConfig.cmake -C Release
```

完整构建会生成部署检查需要的程序和模块。Windows 交付测试会自动先运行运行目录检查，
若检查失败，先补齐构建产物再复验。任何配置、构建或测试失败时停止，不继续打包。
需要自定义共享 libuv 时将配置选项改为 ON；安装规则会把 `uv.dll` 放入发行包的 `bin/`。

## Linux 与 macOS 打包

Linux 示例：

```shell
cmake --preset linux-gcc-release -DSKYUV_USE_SHARED_LIBUV=ON
cmake --build --preset linux-gcc-release --parallel 4
ctest --preset linux-gcc-release -L delivery --output-on-failure
cpack --config build/linux-gcc-release/CPackConfig.cmake -C Release
```

macOS 将以上 preset 替换为 `macos-clang-release`，CPack 配置路径替换为
`build/macos-clang-release/CPackConfig.cmake`。交付测试包含安装、归档内容和解包运行检查。

## 产物位置与校验

CPack 将产物写入对应构建目录的 `packages/`：

```text
build/<preset>/packages/
├── skyuv-<版本>-<系统>-<架构>.zip          # Windows
├── skyuv-<版本>-<系统>-<架构>.zip.sha256
├── skyuv-<版本>-<系统>-<架构>.tar.gz       # Linux/macOS
└── skyuv-<版本>-<系统>-<架构>.tar.gz.sha256
```

每个平台只生成其对应格式。版本来自 CMake 当前版本及预发布标识，系统和架构名由配置生成，
以实际文件名为准；生成本地归档不会创建 Git tag 或 GitHub Release。

PowerShell 校验示例：

```powershell
Get-ChildItem build/windows-vs2022-release/packages/skyuv-*.zip
Get-FileHash "<归档路径>" -Algorithm SHA256
Get-Content "<归档路径>.sha256"
```

将计算结果与 `.sha256` 中的哈希比较。Linux 可在 packages 目录运行
`sha256sum --check <归档名>.sha256`；macOS 校验方法见运行时发行包指南。
分享归档时一并提供其 `.sha256` 文件。

## 构建运行目录与发行包的区别

| 目录或组件 | 用途 |
|---|---|
| 三平台 `build/<preset>/project/` | 完整构建自动组装的运行树，含程序、模块、Lua 库、服务、示例和许可证；可额外含构建工具 |
| CMake `Runtime` 安装组件 | 主程序、运行模块、Lua 库与服务、示例配置、许可证及必要共享依赖 |
| `build/<preset>/packages/` | 根据 Runtime 安装规则生成的交付归档和校验文件 |

CPack 当前仅打包 Runtime 组件，不把整个 project 目录塞入归档。`project/bin/lua` / `lua.exe`
是可选的构建解释器，目前没有 Runtime 安装规则；测试程序也不随运行时包交付。
需要解释器或自有业务服务随包交付时，应先明确安装规则和验证范围。
开发头文件与 CMake 导出属于 Development 组件，不在当前运行时归档中。
发行包的完整目录结构和启动入口由运行时发行包指南维护。

## 可选：生成未压缩的运行时安装目录

Windows 示例：

```powershell
cmake --install build/windows-vs2022-release --config Release --component Runtime --prefix "$PWD/build/windows-vs2022-release/runtime-install"
```

Linux/macOS 替换构建目录即可。建议使用新的安装目录，避免不同版本的残留文件混入验证。
该命令安装运行时组件，不生成归档或 `.sha256`；需要压缩包时仍使用 CPack。
安装目录中的最小节点与 TCP echo 验证命令见运行时发行包指南。
