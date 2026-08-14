# Lua 模块与运行环境清单

本文记录 Skynet 上游默认构建产物在 skyuv 便携构建中的接入状态。状态描述的是可重复验证的能力，不把“源码已参与编译”等同于“功能已完成”。

## C 服务

| 产物 | 当前状态 | 验证等级 | 后续工作 |
|---|---|---|---|
| `snlua` | 已构建、可加载 | Windows 最小启动与 Lua 服务执行；Linux 对照启动；macOS 构建测试 | 扩大三平台示例覆盖 |
| `logger` | 已构建、可加载 | 三平台使用同一 CTest 验证含空格文件路径、UTF-8 内容、显式 reopen 前后写入，以及父目录不存在时以非零状态和明确诊断启动失败；Linux 另验证 SIGHUP 文件轮转 | 权限拒绝依赖运行账户与平台 ACL，不作为固定 CI 用例 |
| `harbor` | 已构建、可加载 | 三平台使用同一 Python 驱动验证双节点握手、socket 所有权转移、全局名传播、跨节点 RPC、掉线通知、重复 ID 拒绝和新 ID 替代节点加入 | 上游 master 生命周期内不允许复用 harbor ID；后续补充更多异常网络测试 |
| `gate` | 已构建、可直接加载 | 三平台 CTest 使用同一 Python 驱动验证 C 服务和 `snax.gateserver` 的真实 TCP 分帧回显；覆盖监听、连接、拆分帧、合并帧、二进制载荷、watchdog 转发、客户端回写和关闭 | 补充异常连接测试 |

## Lua C 模块

| 产物 | 上游组成 | 当前状态 | 依赖与风险 | 计划验证 |
|---|---|---|---|---|
| `skynet` | 核心、序列化、socket、mongo、netpack、memory、multicast、cluster、crypt、sharedata、stm、debugchannel、datasheet、sharetable | 已纳入核心、序列化、socket、crypt、stm、sharetable、sharedata、datasheet、netpack、memory、multicast、cluster、debugchannel 和 mongo | 多个源文件依赖 Skynet 内部符号；`lua-socket` 已完成 Winsock/VLA 适配；便携 memory 后端不伪造 jemalloc 服务级统计；multicast 依赖原子引用计数和消息所有权转移；cluster sender 持有长连接，服务退出不等价于节点退出；debugchannel 使用现有跨平台自旋锁适配；mongo 完整功能需要外部 MongoDB Server | stm、sharetable、sharedata、datasheet 和 multicast 已在三平台验证共享数据、代理失效及多 Actor 生命周期；netpack、memory 和 control 已在三平台验证帧所有权、分配器降级及日志重开接口；cluster 已验证双节点 RPC；debugchannel 已验证队列与 Lua hook；mongo 在 Windows 验证驱动封包和模块加载，并由 Linux MongoDB 8.0 服务容器验证 CRUD 与索引；其余模块分批验证 |
| `client` | clientsocket、crypt、sha1 | 已构建、可加载 | Windows 保留原生 `SOCKET` 宽度；stdin 延迟启动并报告 EOF/错误/溢出 | Windows 回环和管道输入已验证；Linux 已与上游对照 connect/send/recv/shutdown/close、对端关闭和拒绝连接 |
| `bson` | lua-bson | 已构建、可加载 | ObjectID 初始化需要原子操作、时间和进程 ID，已通过 skyuv 兼容层提供 | 已验证文档、数组、UTF-8、null、固定 ObjectID，以及类型、键、UTF-8、ObjectID、子类型、有序字典和循环引用错误；错误后可继续编码 |
| `md5` | lua-md5 | 已构建、可加载 | 独立第三方源码，无新增外部依赖 | 三平台验证已知摘要、HMAC、异或、加解密往返及错误输入 |
| `sproto` | sproto、lsproto | 已构建、可加载 | schema 的 Lua 解析器依赖 lpeg | 三平台验证 schema 解析、结构编码/解码及 pack/unpack 往返 |
| `lpeg` | 上游内置 lpeg | 已构建、可加载 | 独立第三方源码，被 sprotoparser 使用 | 三平台独立验证范围、集合、选择、重复、捕获、完整匹配、UTF-8 与零字节边界，并由 sproto 集成覆盖 |
| `ltls` | OpenSSL TLS 模块 | 上游默认关闭，skyuv 未构建 | OpenSSL、证书与平台分发 | 不属于本阶段默认交付；依赖策略确定后单独计划 |

## 运行环境能力

| 能力 | 当前状态 | 平台边界 |
|---|---|---|
| 动态库加载 | 已通过 skyuv 动态加载接口接入；三平台单元测试覆盖加载、符号查询、缺失文件、缺失符号、重复打开/关闭和错误文本 | Windows 含空格模块路径已验证；Lua C 模块非 ASCII DLL 路径限制见下项 |
| 模块输出目录 | Windows VS 多配置与单配置已统一 | Linux/macOS 继续使用同一无配置子目录布局 |
| Lua 搜索路径 | 最小配置可加载 `cservice`、`lualib`、`luaclib`；Windows 自动测试覆盖含空格的 Lua C 模块目录 | Windows 内置 Lua 的 `package.loadlib` 仍使用 ANSI 系统接口，非 ASCII DLL 路径暂不支持；普通 Lua 源码路径不受该动态库限制 |
| stdin | 已通过 `client.socket` 接入；Windows 管道测试覆盖普通行、空行、UTF-8、EOF 和正常退出 | Unix 与 Windows 控制台编码不同；模块不会因 EOF 或读取错误直接 `exit(1)` |
| signal | Skynet 服务内部 `SIGNAL` 保持不变；Unix `SIGHUP` 重开日志，`SIGINT/SIGTERM` 正常停止；Windows 的 Ctrl+C、Ctrl+Break 和控制台关闭触发正常停止；三平台可调用 `skyuv.control.reopen_log()` | Windows 不把控制台关闭解释为日志重开；Windows Service 的 SCM 停止通知后续单独接入 |
| daemon | Unix 链接上游 daemon 实现；Windows 明确失败并输出诊断 | macOS 上游会提示 daemon 已废弃；Windows Service 不在本阶段范围 |
| 进程退出 | `client.socket` 网络和 stdin 测试均在成功后执行 ABORT 并等待节点退出；控制台或终止信号会先触发 Actor 全量退役 | Windows、Linux 和 macOS 的外部停止信号均在全量退役后返回 0；cluster/harbor 等持有长连接的多节点驱动仍由测试统一终止 |

## 接入顺序

1. `client`：平台依赖最多，先确定 socket 与控制台适配边界；
2. `bson`、`sproto`、`lpeg`、`md5`：独立模块均已建立加载和功能测试；
3. 补齐 `skynet` 聚合模块中的剩余源文件；
4. `gate`、`harbor`：在基础模块齐备后运行主要网络服务示例；
5. 统一验证路径、终端、信号和退出行为。

`ltls` 继续遵循依赖决策文档，不因阶段 4 自动引入 OpenSSL。
