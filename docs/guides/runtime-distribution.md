# 运行时发行包使用与迁移指南

## 适用场景

本文说明如何使用 skyuv 的 Windows、Linux 和 macOS 免安装运行时压缩包，以及如何把已有
Skynet 服务迁移到该目录结构。本文只描述已经由三平台候选发布流程验证的能力。

首个发行包是运行时组件，不提供稳定的 C 开发 SDK，也不承诺第三方二进制 Lua 模块 ABI。

## 前置条件

- 获取与目标系统和 CPU 架构匹配的 `skyuv-<版本>-<系统>-<架构>` 压缩包；
- 同时获取同名的 `.sha256` 文件并在解压前校验；
- 选择普通用户可读写的目录解压，不需要安装到系统目录；
- Windows 当前使用 MSVC 构建，系统需要能够加载 `VCRUNTIME140.dll` 和 UCRT。该前置条件
  尚待全新 Windows 环境独立验收，发行包目前不主动安装 MSVC 运行库。

Linux 和 macOS 发行包已经携带所需的共享 libuv，不依赖系统预装 libuv。Linux 默认静态包含
jemalloc；macOS 和 Windows 使用系统分配器。

## 校验与解压

Linux：

```shell
sha256sum --check skyuv-0.1.0-linux-x86-64.tar.gz.sha256
tar -xzf skyuv-0.1.0-linux-x86-64.tar.gz
```

macOS：

```shell
shasum -a 256 skyuv-0.1.0-darwin-arm64.tar.gz
cat skyuv-0.1.0-darwin-arm64.tar.gz.sha256
tar -xzf skyuv-0.1.0-darwin-arm64.tar.gz
```

PowerShell：

```powershell
Get-FileHash .\skyuv-0.1.0-windows-amd64.zip -Algorithm SHA256
Get-Content .\skyuv-0.1.0-windows-amd64.zip.sha256
Expand-Archive .\skyuv-0.1.0-windows-amd64.zip
```

macOS 和 PowerShell 示例需要人工比较输出的哈希值与 `.sha256` 文件首列是否一致。实际文件名
以下载的 artifact 为准，版本和架构可能不同。

解压后只有一个顶层目录，其关键内容为：

```text
bin/         主程序和必要平台运行库
cservice/    Skynet C 服务
luaclib/     Lua C 模块
lualib/      Lua 库
service/     Lua 服务
examples/    已验证的配置与示例服务
licenses/    许可证和依赖版本清单
```

## 启动最小节点

当前配置中的模块路径相对于进程工作目录解析，因此必须先进入解压后的顶层目录。

Linux 和 macOS：

```shell
cd skyuv-0.1.0-<系统>-<架构>
./bin/skyuv_skynet_portable examples/skyuv.conf
```

Windows PowerShell：

```powershell
Set-Location .\skyuv-0.1.0-windows-amd64
.\bin\skyuv_skynet_portable.exe examples\skyuv.conf
```

进程应以状态码 `0` 退出，并输出 `SKYUV_RUNTIME_SMOKE_OK`。发行流程已在包含空格的解压路径中
验证该启动方式。

## 验证 TCP echo

从安装根目录启动：

```shell
./bin/skyuv_skynet_portable examples/skyuv-echo.conf
```

Windows 使用：

```powershell
.\bin\skyuv_skynet_portable.exe examples\skyuv-echo.conf
```

服务监听回环地址 `127.0.0.1:25490`。连接后发送一行数据，服务会原样返回并正常退出。启动日志
包含 `SKYUV_RUNTIME_ECHO_READY`，成功退出前包含 `SKYUV_RUNTIME_ECHO_OK`。

## 迁移已有 Skynet 服务

迁移时保持 Actor、消息调度和 Lua 服务模型不变，先只调整运行时入口和文件布局：

1. 将业务 Lua 服务放入安装根目录下的 `service/`，共享 Lua 模块放入 `lualib/`。
2. 复制 `examples/skyuv.conf` 作为业务配置起点，保留其中相对的 `cpath`、`lua_path`、
   `lua_cpath`、`luaservice` 和 `lualoader`。
3. 将配置的 `start` 改为业务启动服务；按原 Skynet 配置补充线程数、节点和业务参数。
4. 从安装根目录启动 `bin/skyuv_skynet_portable`，不要引用源码树或构建树路径。
5. 逐项验证网络服务、定时器、消息顺序和关闭流程，再替换生产入口。

若业务包含自编译 Lua C 模块，需要使用目标平台重新构建，并自行保证其 Lua ABI、依赖库和
运行库与当前发行包兼容。首个版本不发布公共头文件，因此该能力不属于正式 SDK 承诺。

## 平台差异与限制

| 能力 | Linux | macOS | Windows |
| --- | --- | --- | --- |
| libuv 交付 | 随包共享库 | 随包共享库 | 静态链接 |
| 分配器 | 默认 jemalloc | 系统分配器 | 系统分配器 |
| 外部 socket 接管 | 支持 Unix `int fd` | 支持 Unix `int fd` | 旧 `int fd` API 返回不支持 |
| daemon 模式 | 保留 Unix 语义 | 保留 Unix 语义 | 不支持 |
| Unix 信号语义 | 支持适用信号 | 支持适用信号 | 仅提供可等价的平台行为 |

Windows 的原生 `SOCKET` 通常具有指针宽度，不能安全通过 Skynet 旧 `socket.bind(int fd)` 接口
传递。skyuv 不进行截断转换；需要外部句柄接管的业务应保留在 Unix，或等待平台安全的扩展接口。

Linux/macOS 必须让主程序和动态模块共享同一份 libuv 进程状态，因此发行包携带共享 libuv。
不要把它们改成分别静态嵌入 libuv 的自定义构建。

## 验证结果

候选发布流程在 Windows x64、Linux x64 和 macOS arm64 上执行以下检查：

- 构建全部运行时目标；
- 安装到包含空格的临时路径；
- 检查禁止文件和本机路径泄漏；
- 生成压缩包并复核内容和 SHA-256；
- 在干净目录解压后运行最小节点、TCP echo 和正常退出测试。

平台与架构名称描述当前候选构建矩阵，不代表其他架构已经验收。

## 常见问题

### 启动后找不到 Lua 服务或动态模块

先确认当前目录是解压后的 skyuv 顶层目录。当前发行配置不按配置文件所在位置解析路径。

### Linux 或 macOS 报告找不到 libuv

确认 `bin/` 中的随包 libuv 未被删除，并使用原始目录结构启动。不要只复制主程序到其他目录。

### Windows 报告缺少运行库 DLL

先安装与当前 MSVC 构建兼容的 Microsoft Visual C++ Redistributable。此问题仍需在全新 Windows
环境完成正式验收，遇到时请记录系统版本、缺失 DLL 名称和发行包版本。

### TCP echo 无法监听

确认 `127.0.0.1:25490` 未被占用，并检查本机安全软件或防火墙策略。示例只监听本机回环地址。

### 能否在任意工作目录启动

当前不能。必须从安装根目录启动；支持任意工作目录需要后续增加启动器或可靠计算安装根目录。
