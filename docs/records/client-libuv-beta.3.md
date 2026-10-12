# beta.3 客户端 libuv 迁移验收

## 实现与验证身份

2026-10-12，最终开发代码提交 `ad199ff`。版本元数据和发布记录在后续提交维护。
本记录描述开发验收，不代表 GitHub Release 已发布。

| 验证 | 结果与证据 |
|---|---|
| Windows MSVC Debug | 首轮全套 69/69；最终代码相关六项全部通过；客户端连续三轮通过 |
| Windows MSVC Release | 最终代码全套 69/69，76.21 秒，包含安装、归档与静态 CRT 校验 |
| Linux GCC/Clang Debug/Release、系统分配器 | [38239107323](https://github.com/synchronized/skyuv/actions/runs/38239107323) 全部成功 |
| Linux ASan/UBSan 与 TSan | 同一工作流成功；覆盖运行时，ASan/UBSan 另覆盖独立 Lua 模块加载及生命周期 |
| macOS Apple Clang Debug/Release | [38239111876](https://github.com/synchronized/skyuv/actions/runs/38239111876) 全部成功 |

## 覆盖与设计结论

- runtime 测试包含 100 次创建销毁、2000 次命令、100 次 TCP 连接和重复关闭、线程迁移、
  动态端口失败连接、无效参数、超时、读半关闭及销毁存活连接。
- 128 KiB 二进制回环验证完整发送、64 KiB 缓存暂停/恢复、EOF 和关闭后错误。
- 独立 Lua 完成十轮收发与写半关闭、五次存活连接 GC、就绪超时与重复 ID，
  以及 stdin 管道仍打开时的 GC/退出。
- 原有 Skynet 客户端、stdin 与 Linux 原版对照继续通过；Actor 调度未改变。
- 自有 Lua/socket 代码独立启用严格警告，第三方 crypt 源码不继承该策略。

读半关闭保留原生 shutdown 操作，但 Lua 接收缓存消费完统一报告 EOF，明确区别于 Windows
原生 recv 的 WSAESHUTDOWN；下游已确认不依赖原生句柄或读半关闭。
同步调用仍占用 worker；每实例按需创建专属 I/O 线程。stdin 队列及模块引用属于进程，
不会因某个 Lua 状态 GC 销毁，避免阻塞读取访问卸载代码。
现行 API、限制和目录迁移以 [客户端指南](../guides/client-socket.md) 为准。

## 验证中修复的问题

- 接入严格 POSIX 功能宏后，Linux crypt 的 random/srandom 声明被隐藏，补齐默认功能宏。
- 新动态加载适配需开启 macOS Darwin 扩展声明；平台静态库需 PIC 才能供 ASan 模块链接。
- 原 fgets 在 Linux 等待输入时持有 stdio 锁，阻塞进程退出；改为 libuv 文件读取并保持行协议。
- 未监听的绑定端口在 macOS 可报告超时，失败测试接受有依据的平台结果，并检查 ID 归零。

## 尚未由本记录证明的事项

正式发行包和公开资产核验以独立发布验收记录为准；固定 Linux 性能基线、运行时路径、
Windows 构建警告治理等其他计划未因本次迁移自动完成。
