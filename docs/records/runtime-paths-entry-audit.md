# 运行路径入口审计

## 范围与日期

2026-10-14，计划 008 的前置源码审计、接口确认和旧路径行为回归。本文记录现状及已执行验证；
待实现接口以 [计划 008](../plans/008-runtime-paths.md) 为准。本轮不改变运行时入口行为。

## 当前行为与证据

| 对象 | 当前路径基准 | 权威实现 |
| --- | --- | --- |
| 主配置 | 优先调用方工作目录；不存在时才尝试 `bin/` 父目录 | `src/runtime/launcher.c` |
| 随包相对模块 | 进程工作目录；配置回退到安装根时启动器才切换目录 | `src/runtime/launcher.c` 与配置中的模块字段 |
| 配置 include | 主配置路径及递归 include 的当前路径 | `3rd/skynet/skynet-src/skynet_main.c` 的 `load_config` |
| `$NAME` 替换 | 进程环境；发生在 Lua 配置执行之前 | 同上 |
| 相对 logger 文件 | 进程工作目录，而非配置文件父目录 | `3rd/skynet/service-src/service_logger.c` |
| 日志重开 | 保存的原路径，使用追加模式 `freopen` | 同上 |
| Unix 相对 PID | 进程工作目录 | `3rd/skynet/skynet-src/skynet_daemon.c` |
| Windows daemon | 返回不支持，不创建 PID | `src/compat/skynet/start/skyuv_daemon.c` |

绝对日志/PID 路径绕过相对目录基准。logger 初始化不创建父目录，创建文件失败导致服务启动失败；
当前启动失败的通用诊断并不保证包含最终路径。logger 重开失败后直接替换原 `FILE *`，缺少
保留可用句柄的错误处理，后续日志回调存在空句柄风险；后续重开错误路径验收必须覆盖此情况。

配置加载器按 `package.config` 的目录分隔符识别路径，绝对路径判断只检查根分隔符；Windows
驱动器路径和混合分隔符需要专门覆盖。配置环境替换直接发生在源码字符串上，根目录注入应使用
Lua 值而非拼接 Lua 代码，以避免路径中的引号或反斜杠破坏配置。

## Unix daemon 所有权审计

- `check_pid` 先读 PID 并通过 `kill(pid, 0)` 判断，随后才 daemonize 和加锁，存在并发检查窗口。
- `write_pid` 持有 `flock`，但成功后的 `FILE *` 未保存为显式状态；`fdopen` 失败分支遗失 fd。
- 写 PID 的错误判断未完整覆盖负返回值和 `fflush` 错误。
- `redirect_fds` 失败分支可能遗失 `/dev/null` 描述符。
- 正常 `daemon_exit` 直接 unlink 路径，未核对当前实例的文件身份；异常退出会留下 PID 文件。
- macOS 跳过 Linux 的 daemon fork，之后仍写 PID 并重定向标准描述符；测试须保持这一差异。
- daemon 在 logger 初始化之前启动；logger 随后的启动失败没有统一经过 daemon_exit 清理。

结论：路径重定向之外，还需在平台适配层明确 PID 文件的锁、文件身份和关闭所有权。
不能把上游当前实现直接宣称为具备完整的并发与错误路径保证。

## 本轮验证

新增 CTest `skynet.portable.paths_legacy` 使用临时的含空格配置目录和两个含空格调用目录：
同一配置的相对日志落在各调用目录，绝对日志由两次启动共享；双方均验证重开前后标记。
测试保留全部模块绝对路径，避免将模块定位失败混入日志基准结论。

Linux Clang Release 配置、构建与完整 CTest 67/67 通过。提交 `e389e55` 的
[Linux 完整 CI](https://github.com/synchronized/skyuv/actions/runs/38531579908) 与
[macOS Debug/Release CI](https://github.com/synchronized/skyuv/actions/runs/38531585782)
全部通过。Windows 本机不可用，Windows 行为仍需 Windows 验证，不以 Unix 结果替代。

## 后续边界

计划状态为“就绪”，当前行为与待实现接口分别保存。本轮没有实现 `--runtime-paths` 等新选项，
没有修改发行模板或宣告计划 008 完成。后续依次实现配置入口、根目录解析、日志和 PID 错误路径，
再验证只读安装树与发行归档。

## 新入口第一批实现

后续实施新增 `--runtime-paths`、显式安装根与日志/运行根选项，使用独立配置加载适配层；
旧模式保留原加载脚本和路径行为。根目录作为 Lua 值注入，嵌套 include 以包含它的文件为基准，
配置后按命令行、进程环境、配置、默认值解析根目录。默认文件 logger 与 Unix PID 的相对路径
转换为明确绝对路径，必要时创建选定根目录；绝对文件路径不要求默认根目录可写。

适配入口补丁 `0013` 仅应用于构建副本。平台宏、分隔符、目录创建和 Windows 驱动器路径
处理集中在 `src/platform/runtime_paths.c`；本轮没有替换 Unix daemon 的 PID 锁和关闭流程，
也没有修复 logger 重开失败后的句柄处理。只读安装树和三平台交付验收仍待后续完成。

新增 `skynet.portable.runtime_paths` 覆盖跨工作目录、绝对主配置、含空格及引号的目录、
嵌套 include、三级覆盖及默认值、日志重开、根目录文件冲突、缺失配置、错误配置类型、
保留变量覆盖、空目录值、重复/未知参数，以及不创建未启用目录。Windows 测试分支还检查
新模式 daemon 拒绝且不创建日志/运行目录；本机 Windows 验证仍不可用。

本批 Linux Clang Release 配置、严格构建及完整 CTest 68/68 通过；随后对最后的入口调整
重新运行新旧路径两组测试，并通过新增的真实安装树外置配置测试。此安装测试同时验证自动
定位安装根、随包相对模块加载、日志落在外置配置根以及前台不创建运行目录。
