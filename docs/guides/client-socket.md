# 客户端 socket

便携运行时的 `client.dll`（Unix 为 `client.so`）同时提供 `client.socket` 和
`client.crypt`。独立 Lua 与 Skynet 服务可使用相同调用方式。

## 连接与收发

`connect(ipv4_address, port[, timeout_seconds])` 同步等待连接结果，成功返回正整数连接 ID。
省略或使用负超时保持无限等待；零超时立即失败，有限超时取消并释放连接。
端口范围为 0 至 65535，超时必须有限且不超过 1000000 秒；不支持 DNS 和 IPv6。
失败异常包含地址、端口、libuv 错误名及原因。

`send(id, data)` 等待全部写请求完成，不表示对方应用已经读取。
`recv(id)` 非阻塞地读取最多 4096 字节；暂无数据不返回值，EOF 返回空字符串，错误抛出异常。
接收缓存上限为每连接 64 KiB，满时暂停读取，消费后恢复，不丢数据。
`close(id)` 等待关闭回调，已返回过的连接 ID 可重复关闭。

连接 ID 只在所属模块实例内有效，不是操作系统 fd/SOCKET，不能传给其他网络库。
卸载模块并回收最后一个函数引用后，GC 关闭该实例全部连接及 I/O 线程。

## 就绪与半关闭

`select(read_ids[, write_ids[, timeout_seconds]])` 和别名 `poll` 返回就绪 ID 表，
无就绪事件且超时返回 false。只监测写方向时返回空读表和写表。无连接的无限等待报错。
负 ID 忽略，未知非负 ID 报错，重复 ID 保留；超时参数的有限值上限同 connect。
读就绪包含缓存数据、EOF 和待报告错误，写就绪表示可接受发送，不能代替实际发送结果。

`shutdown(id, "w")` 使用 libuv 写半关闭，已接受写请求完成后发送 EOF，仍可读响应。
`shutdown(id, "r")` 通过最小平台适配执行原生读半关闭；已进入模块缓存的数据可继续读取，
缓存读完后返回空字符串。`"rw"` 组合关闭两个方向，重复调用安全。
读关闭后的 Lua EOF 是统一约定；Windows 原生 recv 的 WSAESHUTDOWN 不直接暴露。

`usleep(microseconds)` 使用 libuv 休眠，向上取整到毫秒；允许 0 至 1000000000 微秒。

## 线程与 stdin

每个模块实例首次连接时创建一个专属 I/O 线程，该线程独占 libuv loop。Lua 操作串行提交，
同步连接和发送会占用调用 worker；线程归属不随 Skynet worker 迁移。
就绪等待通过状态版本号和条件变量通知，不依赖忙轮询。

`readstdin()` 保持行读取及 nil + eof/error/overflow 状态约定，空行返回空字符串。
stdin 属于进程，所有实例共享队列；后台线程首次调用时启动，不持有 Lua 对象。
EOF 后线程可回收，仍等待控制台输入的线程由进程退出终止；任一实例 GC 不关闭共享 stdin。
首次使用 stdin 时会持有 client 动态库的进程级引用，防止后台线程访问已卸载模块；
因此该库使用过 stdin 后会驻留到进程退出。
本版本不承诺重置 stdin 或各 Lua 实例独立消费输入流。
