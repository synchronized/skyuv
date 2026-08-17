# 阶段 6 运行闭包审计

## 结论

首个跨平台交付版本应先提供运行时压缩包，不发布尚未承诺 ABI 稳定性的头文件开发组件。
发行包需要安装主程序、全部已构建的 C 服务与 Lua C 模块，并整体安装 Skynet 的 `lualib/`
和 `service/`。当前构建树中的测试配置包含源码树和构建树绝对路径，不能直接进入发行包。

本记录描述 2026-08-16 对当前仓库和三平台 Release 构建产物的审计结果，并记录安装、打包与
候选发布验证形成的运行库约束。

## 审计范围

- 根 CMake 项目、第三方接入和 portable 目标定义；
- `build/windows-vs2022-release` 中的实际 Release 产物；
- Skynet 的 Lua 加载器、bootstrap 服务及模块搜索路径；
- libuv、Lua、jemalloc 和 Windows 运行库的链接方式；
- 仓库内随二进制分发所需的许可证文件。

## 两层运行闭包

### 最小启动闭包

最小节点至少需要：

- `bin/skyuv_skynet_portable`；
- `cservice/snlua`，用于启动 Lua 服务；
- `luaclib/skynet`，提供 `skynet.core` 等基础 Lua C 模块；
- `lualib/loader.lua`、bootstrap 实际加载的 Skynet Lua 库；
- `service/bootstrap.lua`、`launcher.lua`、`cdummy.lua`、`datacenterd.lua`、
  `service_mgr.lua` 及其动态依赖；
- 一份仅引用安装树相对路径的配置和一个可正常退出的启动服务。

这组文件适合解释启动链和编写冒烟测试，但不适合作为人工维护的发行清单。Lua 的
`require`、服务名和配置可以在运行期拼接，静态枚举容易漏掉合法路径。

### 支持的运行时闭包

首版发行包采用以下完整闭包：

| 安装位置 | 内容 | 当前数量或目标 |
| --- | --- | --- |
| `bin/` | `skyuv_skynet_portable` 和必要平台运行库 | 1 个主程序 |
| `cservice/` | `snlua`、`logger`、`harbor`、`gate` | 4 个模块 |
| `luaclib/` | `skynet`、`bson`、`lpeg`、`client/socket`、`md5/core`、`skyuv/control`、`sproto/core` | 7 个模块，保留子目录 |
| `lualib/` | Skynet 上游 `lualib/` 的完整运行时内容 | 当前 68 个文件 |
| `service/` | Skynet 上游 `service/` 的完整运行时内容 | 当前 22 个文件 |
| `examples/` | 最小启动、TCP echo 和正常退出示例 | 后续生成并验证 |
| `licenses/` | skyuv、Skynet、libuv，以及实际链接依赖的许可证 | 按依赖清单校验 |

整体安装 Lua 目录保留 Skynet 用户熟悉的模块集合，也避免每次新增示例或服务时修改隐式依赖
白名单。发行测试仍以最小启动闭包验证基础能力。

## 平台运行库边界

### Windows

当前 Windows Release 主程序静态包含 libuv，并统一使用 `/MT` 静态 CRT。发行测试逐个审计
EXE/DLL 的 PE 导入表，未发现 `VCRUNTIME`、`MSVCP`、`UCRTBASE` 或 `api-ms-win-crt-*` 动态
依赖；候选 ZIP 也已在全新 Windows Sandbox 中成功启动，因此不声明 Visual C++
Redistributable 前置条件。

Lua C 模块目录中生成的 `.lib` 和 `.exp` 是链接辅助文件，不属于运行时包。PDB 若发布，应放入
独立符号 artifact。

### Linux

Linux 默认使用静态 `libjemalloc_pic.a`，因此无需单独分发 jemalloc 共享库，但必须携带其
许可证。发行构建使用共享 libuv，安装正确的 SONAME 链，并通过 `$ORIGIN` 设置可搬运的运行时
搜索路径，不依赖宿主机预装 libuv。

候选发布验证曾强制静态链接 libuv。主程序和 Lua/C 服务动态模块因此各自嵌入独立的 libuv
全局状态，最小节点完成业务逻辑后在关闭阶段中止。该方案不能用于 Unix 模块化运行时。

### macOS

macOS 使用系统分配器。发行构建与 Linux 一样使用随包共享 libuv，并通过 `@loader_path`
形成可搬运的 install name，避免主程序和动态模块分别持有 libuv 全局状态。

## 必须排除的内容

- `tests/`、基准和 Soak 程序及其 fixture；
- CMocka、测试专用 Python 脚本和测试配置；
- CMake 缓存、对象文件、生成器元数据和本机绝对路径；
- `.lib`、`.exp` 等 Windows 链接辅助文件；
- 第三方源码树和应用补丁后的临时源码树；
- 未经单独组件定义的 PDB、dSYM 和其他调试符号；
- Skynet 私有头文件以及尚未承诺稳定的 skyuv 开发头文件。

## 配置与启动约束

当前测试配置把 `cpath`、`lua_path`、`lua_cpath`、`luaservice` 和 `lualoader` 展开为源码树或
构建树绝对路径。发行配置必须改为安装根目录下的相对路径，并在源码树之外验证。

Skynet 的路径解释依赖进程工作目录，而不是配置文件所在目录。skyuv 已增加发行包启动器：当调用方
工作目录不存在指定配置时，启动器根据 `bin/skyuv_skynet_portable` 的位置定位安装根目录，并切换
到该目录后进入 Skynet；调用方工作目录存在配置时仍保持原有行为。

## 许可证闭包

当前已发现以下直接相关文件：

- `LICENSE`：skyuv；
- `3rd/skynet/LICENSE`：Skynet；
- `3rd/libuv/LICENSE`、`LICENSE-extra` 和 `LICENSE-docs`：libuv；
- `licenses/Lua-LICENSE`：Skynet 内嵌修改版 Lua；
- `licenses/LPeg-LICENSE`：LPeg；
- `licenses/lua-md5-LICENSE`：lua-md5；
- `3rd/skynet/3rd/jemalloc/COPYING`：Linux 默认分配器。

CMocka 只用于测试，不进入运行时包，因此其许可证不属于运行时许可证闭包。后续安装规则应由
明确清单复制许可证，并以测试检查清单和实际启用依赖一致。Lua、LPeg 和 lua-md5 的上游目录
没有独立许可证文件，仓库从其权威源码或上游授权说明中单独维护完整许可文本。

## 对后续实施的约束

1. 先实现 `Runtime` 组件，不安装 `include/skyuv/`；开发组件另行评审 ABI 后再加入。
2. 安装动态模块必须使用 CMake 目标，不能从已知构建目录复制文件。
3. Lua 目录整体安装，但排除源码仓库中的测试、示例和开发辅助内容。
4. 发行构建在 Windows 使用静态 libuv，在 Linux 和 macOS 使用随包共享 libuv。
5. 安装后测试必须在含空格的临时目录，并从安装根之外的工作目录运行。
6. Windows、Linux 和 macOS 分别检查最终二进制动态依赖，不能仅凭 CMake 配置推断。

## 持续验证

当前运行时闭包没有尚待实测的必需平台依赖。新增动态模块或更改链接策略时，仍必须重新执行
安装树测试、PE 导入审计和干净环境验证。
