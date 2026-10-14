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

最终实现提交 `fc28b9d` 的 [Linux 完整 CI](https://github.com/synchronized/skyuv/actions/runs/38534220727)
与 [macOS Debug/Release CI](https://github.com/synchronized/skyuv/actions/runs/38534226308) 全部通过。
首次 macOS 验证发现临时目录符号链接使测试预期路径与 realpath 输出不一致，已统一按规范
路径比较，并在 Linux 符号链接临时目录中复验通过；运行时的启动和日志写入不受该测试问题影响。
环境覆盖值改用 libuv 读取，超长值明确失败。Windows 未在本机验证。

## 日志与 Unix PID 生命周期实现

本批以 `src/platform/daemon.c` 替换便携目标的上游 Unix daemon 实现。PID 协调锁在 fork 前
取得，`<pidfile>.lock` 保留以固定锁 inode；PID 文件另行加锁以检测已有实例。文件描述符
设为 close-on-exec，在进程运行期间持续持有；错误分支释放资源，正常关闭与 `atexit` 重复
清理安全。只有持有的 PID 文件与路径仍指向同一设备和 inode 时才删除，避免删除替换文件。
异常退出留下的 PID 在取得锁且进程不存在后可恢复；普通 `exit()` 启动失败也会释放 PID。
macOS 保留不 fork 的上游模式，Windows 继续拒绝 daemon。

协调文件不自动删除，运行期间禁止部署者替换或删除 PID/锁文件。实现以支持 `flock` 的
本地文件系统为边界，不把网络文件系统或外部修改目录的竞态作为已支持能力。

logger 重开通过补丁 `0014` 委托到平台适配函数：先打开新文件，成功才替换并关闭旧句柄，
失败在 stderr 输出最终路径并继续使用旧文件；后续再次重开可恢复。Actor 串行处理 logger
回调，旧句柄不由其他线程并发关闭。

新增验证：

- `platform.logger_reopen`：20 次失败保留、继续写入旧文件及恢复切换。
- `platform.pid_lifecycle`：并发启动冲突、并发启停、SIGKILL 残留、活跃外部 PID、错误退出、
  文件身份保护、符号链接拒绝、重复关闭，以及 32 fd 限制下 100 次取得/发布/释放。
- `skynet.portable.daemon_lifecycle`：真实 daemon 从两处工作目录启动，使用同一个 PID 路径，
  冲突实例失败，正常退出清理，以及 logger 初始化失败后锁和 PID 清理。
- `install.runtime_smoke`：Unix 安装树设为只读，验证新模式标准输出和外置配置文件日志；
  测试结束后恢复临时树权限用于删除。Windows 未设置 ACL，不能据此宣称 Windows 只读验证。

Linux Clang Release 配置、严格构建及完整 CTest 71/71 通过；Linux/macOS CI 和 Windows
覆盖状态在本批最终验证后补充。计划 008 仍在进行中，Windows 与完整发行交付收口未完成。

本批最终提交 `9b94c9c` 的 [Linux 完整 CI](https://github.com/synchronized/skyuv/actions/runs/38540758583)
与 [macOS Debug/Release CI](https://github.com/synchronized/skyuv/actions/runs/38540764793) 全部通过。
Linux ASan/UBSan 已额外覆盖 PID 与日志恢复测试。GCC 首次发现适配静态库缺少 PIC，已为
共享 logger 使用的库启用 PIC，并在本机完成 GCC system Debug 构建及六项相关测试。
后续一次 Clang Debug 验证发现旧路径测试的日志标记数量未满足预期，原夹具只固定等待
50 毫秒；已改为核对本次追加的日志落盘后再关闭，失败断言包含日志内容。GCC 上日志、旧路径
与新路径三项各重复五次全部通过，最终 CI 也通过。Windows 新模式、只读 ACL 及解压包新模式
专项验收仍待补齐，不将 Unix 结果代替 Windows 验证。
