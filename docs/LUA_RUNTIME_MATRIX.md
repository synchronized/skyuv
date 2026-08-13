# Lua 模块与运行环境清单

本文记录 Skynet 上游默认构建产物在 skyuv 便携构建中的接入状态。状态描述的是可重复验证的能力，不把“源码已参与编译”等同于“功能已完成”。

## C 服务

| 产物 | 当前状态 | 验证等级 | 后续工作 |
|---|---|---|---|
| `snlua` | 已构建、可加载 | Windows 最小启动与 Lua 服务执行；Linux 对照启动；macOS 构建测试 | 扩大三平台示例覆盖 |
| `logger` | 已构建、可加载 | 三平台启动路径使用；Windows 多配置目录已验证 | 补充文件输出路径测试 |
| `harbor` | 已构建、可加载 | Windows 已验证双节点握手、socket 所有权转移、全局名传播、跨节点 RPC、掉线通知、重复 ID 拒绝和新 ID 替代节点加入；三平台构建通过 | 上游 master 生命周期内不允许复用 harbor ID；补充 Linux、macOS 双节点运行测试 |
| `gate` | 已构建、可直接加载 | Windows 已验证 C 服务监听、连接、二字节分帧、watchdog 转发、客户端回写和关闭；`snax.gateserver` 也已完成真实 TCP 分帧回显 | 补充 Linux、macOS 运行测试和异常连接测试 |

## Lua C 模块

| 产物 | 上游组成 | 当前状态 | 依赖与风险 | 计划验证 |
|---|---|---|---|---|
| `skynet` | 核心、序列化、socket、mongo、netpack、memory、multicast、cluster、crypt、sharedata、stm、debugchannel、datasheet、sharetable | 已纳入核心、序列化、socket、crypt、stm、sharetable、sharedata、datasheet、netpack、memory、multicast 和 cluster | 多个源文件依赖 Skynet 内部符号；`lua-socket` 已完成 Winsock/VLA 适配；便携 memory 后端不伪造 jemalloc 服务级统计；multicast 依赖原子引用计数和消息所有权转移；cluster sender 持有长连接，服务退出不等价于节点退出 | multicast 已验证双 Actor 订阅、共享发布、引用释放、取消订阅和频道删除；cluster 已验证核心协议编解码、节点监听、服务注册和双节点 RPC；其余模块分批验证 |
| `client` | clientsocket、crypt、sha1 | 未构建 | pthread、POSIX socket、`fcntl`、`usleep`、stdin 后台线程生命周期、Winsock 句柄宽度 | 回环 connect/send/recv/shutdown/close；可控 stdin |
| `bson` | lua-bson | 已构建、可加载 | ObjectID 初始化需要原子操作、时间和进程 ID，已通过 skyuv 兼容层提供 | 文档、数组、UTF-8、null 与固定 ObjectID 编解码已验证；错误输入待补充 |
| `md5` | lua-md5 | 已构建、可加载 | 独立第三方源码，无新增外部依赖 | 已知摘要、HMAC、异或、加解密往返及错误输入已验证 |
| `sproto` | sproto、lsproto | 已构建、可加载 | schema 的 Lua 解析器依赖 lpeg | schema 解析、结构编码/解码及 pack/unpack 往返已验证 |
| `lpeg` | 上游内置 lpeg | 已构建、可加载 | 独立第三方源码，被 sprotoparser 使用 | 已通过 sproto schema 解析进行集成验证；独立模式边界测试待补充 |
| `ltls` | OpenSSL TLS 模块 | 上游默认关闭，skyuv 未构建 | OpenSSL、证书与平台分发 | 不属于本阶段默认交付；依赖策略确定后单独计划 |

## 运行环境能力

| 能力 | 当前状态 | 平台边界 |
|---|---|---|
| 动态库加载 | 已通过 skyuv 动态加载接口接入 | 扩展名、搜索路径和错误文本仍需矩阵测试 |
| 模块输出目录 | Windows VS 多配置与单配置已统一 | Linux/macOS 继续使用同一无配置子目录布局 |
| Lua 搜索路径 | 最小配置可加载 `cservice`、`lualib`、`luaclib` | 需覆盖空格和非 ASCII 工作路径 |
| stdin | 便携构建未接入 `client.socket` | Unix 与 Windows 管道/控制台 EOF、编码不同；不得直接 `exit(1)` |
| signal | Unix 保留上游行为；Windows 使用最小兼容定义 | Windows 不伪造 POSIX 信号 |
| daemon | Unix 使用上游能力；Windows 明确不支持 | Windows Service 不在本阶段范围 |
| 进程退出 | 测试脚本观察成功标志后终止常驻节点 | 需增加模块线程与 socket 正常回收验证 |

## 接入顺序

1. `client`：平台依赖最多，先确定 socket 与控制台适配边界；
2. `bson`、`sproto`、`lpeg`、`md5`：独立模块均已建立加载和功能测试；
3. 补齐 `skynet` 聚合模块中的剩余源文件；
4. `gate`、`harbor`：在基础模块齐备后运行主要网络服务示例；
5. 统一验证路径、终端、信号和退出行为。

`ltls` 继续遵循依赖决策文档，不因阶段 4 自动引入 OpenSSL。
