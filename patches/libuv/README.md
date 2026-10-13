# libuv 补丁

补丁以固定 libuv 子模块版本为基线，仅应用到构建目录副本，不修改第三方源码工作树。

- `0001-Fix-Windows-IPv6-initializer-braces.patch`：Windows SDK 的 IN6_ADDR 是结构体包裹
  联合体与 Byte[16] 数组，而 IN6ADDR_LOOPBACK_INIT 只提供数组层括号。补齐结构体与
  联合体层括号，保留 ::1 的全部初始化字节，消除 Clang -Wmissing-braces。
  仅替换 Windows winsock.c 编译输入；静态和共享 libuv 均使用副本。子模块升级或 SDK
  宏形态改变后需重新检查；补丁失配时配置失败，不能静默跳过。
- `0002-Fix-Windows-affinity-mask-width.patch`：线程亲和性掩码在移位前将 `1` 转为
  `DWORD_PTR`，避免 Windows x64 上对 32 位整数移位后才扩展到 64 位。
  仅替换 Windows thread.c 编译输入；不改变 CPU group 或调度模型。
  `libuv.affinity_mask_width` 逐位验证进程允许和不允许的 CPU 位，并恢复原线程亲和性。
- `0003-Fix-Windows-stat-path-restoration.patch`：stat 兼容回退函数仅在实际拆分路径后
  恢复分隔符，避免根路径、盘符及相对目录使用未初始化字符；读取分隔符前检查 split
  非零，避免空路径或纯点号路径读取起点之前的内存。静态和共享 libuv 使用 fs.c 副本。
  `libuv.stat_path_restoration` 使用完整函数的系统调用夹具覆盖成功、逐阶段失败、路径
  恢复和句柄关闭；此测试不依赖旧 Windows 或特定 ACL，不能替代这些系统环境的完整验收。
