# libuv 补丁

补丁以固定 libuv 子模块版本为基线，仅应用到构建目录副本，不修改第三方源码工作树。

- `0001-Fix-Windows-IPv6-initializer-braces.patch`：Windows SDK 的 IN6_ADDR 是结构体包裹
  联合体与 Byte[16] 数组，而 IN6ADDR_LOOPBACK_INIT 只提供数组层括号。补齐结构体与
  联合体层括号，保留 ::1 的全部初始化字节，消除 Clang -Wmissing-braces。
  仅替换 Windows winsock.c 编译输入；静态和共享 libuv 均使用副本。子模块升级或 SDK
  宏形态改变后需重新检查；补丁失配时配置失败，不能静默跳过。
