# 阶段 4：Lua 模块与运行环境实施记录

## 记录范围

本文记录计划 [`005：Lua 模块与运行环境`](../plans/005-lua-runtime.md) 的实际接入过程、
验证范围和平台差异。当前模块能力以
[`Lua 模块与运行环境清单`](../LUA_RUNTIME_MATRIX.md) 为权威来源；本文不承担当前 API 参考职责。

## 基础运行环境

- 建立上游默认 C 服务、Lua C 模块和运行环境清单，确认首个缺口为 `client.socket`。
- 统一 C 服务、Lua C 模块的输出目录与 Lua 搜索路径，Windows 单配置和多配置生成器使用
  相同配置文件即可加载模块。
- Windows 含空格路径已经验证；内置 Lua `package.loadlib` 使用 ANSI Windows 接口，
  非 ASCII DLL 路径当前明确不支持。
- Linux 和 macOS 便携目标使用上游 `skynet_daemon.c`；Windows 配置 daemon 时以非零状态
  和包含 pidfile 的诊断明确失败。
- 增加 `skyuv.control.reopen_log()`。Unix 的 SIGHUP watcher 复用同一日志重开动作，
  Windows 不把控制台关闭解释为日志重开。

## client.socket

- 建立独立动态模块目标，三平台输出为 `luaclib/client/socket`。
- Windows 内部使用原生 `SOCKET` 宽度，Lua 边界通过 `lua_Integer` 往返，避免句柄截断。
- pthread、锁和休眠在 Windows 通过 skyuv 兼容层实现；Unix/macOS 使用系统 pthread，避免
  Windows 兼容头遮蔽系统头文件。
- stdin 线程改为首次调用 `readstdin` 时启动；EOF、读取错误和队列溢出通过可选第二返回值
  报告，不再从后台线程直接终止进程。
- Windows stdin 测试覆盖普通行、空行、UTF-8 和 EOF，并验证线程句柄回收。
- Windows/Linux 回环覆盖 connect、send/recv、写 shutdown、对端关闭、拒绝连接和无效模式；
  Linux 使用同一 Lua 夹具对照上游与便携版行为。
- 三平台已经完成模块编译和加载；macOS 回环与 stdin 行为仍是阶段剩余验收项。

## 独立 Lua C 模块

- `bson` 覆盖嵌套文档、数组、UTF-8、null、固定 ObjectID，以及类型、键、UTF-8、ObjectID、
  子类型、有序字典和循环引用错误；错误后仍可继续编码解码。
- `sproto.core` 与 `lpeg` 覆盖文本 schema、结构编解码、pack/unpack、模式组合、捕获、
  完整匹配、UTF-8 和零字节。
- `md5.core` 覆盖标准摘要、HMAC-MD5、按位异或、固定种子加解密和参数错误。
- `skynet.crypt` 覆盖 SHA-1、HMAC-SHA1、Base64、Hex、循环异或、DES、随机密钥和错误输入。
- 上述无外部服务模块均已使用跨平台 Python 驱动进入 Windows、Linux 和 macOS CTest。

## 共享数据与运行辅助模块

- `skynet.stm` 通过两个 Actor 验证首次读取、无变更读取、写入更新和跨服务可见性。
- `sharetable` 覆盖嵌套只读表、Lua 源码加载、批量查询和版本替换。
- `sharedata` 覆盖新建、查询、深拷贝、更新传播、旧代理失效、删除和回收。
- `datasheet` 覆盖多服务查询、根与嵌套代理更新、结构变化和旧代理失效。
- `multicast` 覆盖两个 Actor 的订阅、共享消息引用、取消订阅和频道删除。
- `netpack` 覆盖长度头、空包、二进制与 UTF-8 载荷、65535 字节边界和缓冲区所有权；
  `filter/pop` 另由 gate 的 TCP 分帧联调覆盖。
- `memory` 保留完整 Lua API；系统分配器后端不伪造服务级 jemalloc 统计，诊断接口明确提示降级。
- `skyuv.control` 验证不依赖平台信号的日志重开入口。
- 以上模块均已使用相同驱动在三平台运行测试。

## C 服务与多节点能力

- Lua gate 与 C gate 使用同一 Python TCP 客户端验证监听、accept、拆分帧、合并帧、二进制
  载荷、watchdog 转发、回显和连接关闭。
- harbor 三平台双节点测试覆盖握手、全局名传播、跨节点 RPC、掉线通知、重复 ID 拒绝和
  使用新 ID 的替代节点加入，并在所有路径回收子进程。
- harbor 联调修复了 `socket.start` 接管已连接 socket 时的服务所有权转移，避免数据继续
  投递给已执行 `socket.abandon` 的旧服务。
- cluster.core 覆盖名称、节点、trace 帧和成功/失败响应编解码；Windows 双节点测试覆盖
  监听、注册、连接、名称寻址和 UTF-8 RPC。cluster sender 的长连接由驱动统一终止。
- logger 三平台覆盖含空格路径、UTF-8 内容、显式 reopen 前后写入，以及父目录不存在时的
  非零退出和明确诊断；Linux 另验证 SIGHUP 轮转。

## 调试与数据库模块

- `skynet.debugchannel` 覆盖 channel 创建和连接、双向 FIFO、二进制与 UTF-8 命令、空队列、
  双端回收以及 Lua count hook 的安装、触发和移除。
- `skynet.mongo.driver` 覆盖 OP_MSG 长度、请求 ID、操作码、标志、BSON 载荷、短响应拒绝和
  `skynet.db.mongo` 高层模块加载。
- Linux MongoDB 8.0 服务容器验证连接、插入、查询、更新、唯一索引、删除和资源关闭；
  GitHub Actions 服务容器仅支持 Linux，macOS 不承担完整数据库集成测试。
- 普通启动、client 模块加载、cluster.core、debugchannel 和 Mongo 驱动封包均已进入三平台 CTest。

## 验证方式演进

- 简单模块统一使用 `portable_smoke.py`，不再依赖 PowerShell 启动驱动。
- gate、harbor、logger 分别使用面向协议和生命周期的跨平台 Python 驱动。
- 成功后应等待正常退出；仅 cluster、harbor 等有设计内长连接的多节点测试由驱动统一终止。
- Linux 与 macOS 完整矩阵采用手动触发和每周回归；源码、构建或测试变化必须查看到最终结果。

## 当前差异与遗留项

- Windows 不支持上游 `int fd` 表达原生宽句柄的接口，不进行有损转换。
- Windows daemon 保持明确不支持；Windows Service 尚未接入。
- 非 ASCII DLL 路径受 Lua Windows 动态加载接口限制。
- macOS 尚需补齐与 Windows/Linux 同等级的 `client.socket` 回环和 stdin 行为测试。
- TLS、性能、长时间稳定性和发布打包不属于本阶段默认交付。
