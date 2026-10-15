# v0.2.0 Windows Actions 专项验收

## 范围与结论

2026-10-15，维护者指定将剩余 Windows 验收放到 GitHub Actions 执行。本次在提交
`a3999cf` 的 `windows-2022` 托管 Runner 上，使用 Visual Studio 2022 x64 Release 和静态
libuv 完整构建，CTest **80/80 通过**，耗时 114.37 秒。计划 008、009 的 Windows 验收缺口已补齐。
这是托管 Windows 环境验收，不表示在维护者的 Windows 宿主机本地执行过相同测试。

## 运行路径与只读程序树

`skynet.portable.paths_legacy`、`skynet.portable.runtime_paths` 和
`package.extracted_runtime` 均通过，覆盖旧入口兼容、新目录模式、配置 include、覆盖顺序、
含空格路径、两个不同调用目录、日志写入与重开、错误路径，以及 Windows daemon 快速失败且
不创建日志/运行目录。

解压包测试对临时程序树递归使用 `icacls`，按当前用户 SID 显式拒绝写数据、追加数据、写属性和
扩展属性。测试先确认新建文件和追加现有文件均报权限错误，再执行：

- 只读程序树的标准输出 smoke；
- 两个包外调用目录使用包外配置启动；
- 日志写入包外目录，并通过 `skyuv.control.reopen_log()` 重开；
- 前台运行不创建 PID 运行目录。

测试在 `finally` 中删除本次加入的拒绝 ACE，再清理临时树。ACL 仅作用于本次临时解压目录，
不改变仓库、Runner 工具目录或用户目录权限。该验证使用实际 Windows ACL，不以只读文件属性
或 Unix chmod 替代。

## Lua 动态模块与第三方消费

`cmake.consumer_dynamic` 通过，耗时 34.82 秒。源码 `add_subdirectory` 与移动安装前缀后的
`find_package(skyuv)` 两种方式均构建真实 Lua MODULE，再由独立 Lua 解释器通过
`package.cpath` 动态 `require` 并校验返回值；清除搜索路径后的加载失败也已验证。
此结果不依赖 `luaL_requiref` 静态注册，不承诺任意第三方 Lua 模块二进制兼容或稳定 ABI。

## Actions 证据

- [三平台发行演练](https://github.com/synchronized/skyuv/actions/runs/38717430148)：
  Windows 完整 80 项测试、Linux/macOS delivery 测试、归档生成及上传均成功。
- [Linux 完整矩阵](https://github.com/synchronized/skyuv/actions/runs/38717429503)：
  GCC/Clang Debug/Release、system allocator、TSan、ASan/UBSan 全部成功。
- [macOS Debug/Release](https://github.com/synchronized/skyuv/actions/runs/38717429995)：全部成功。
- 本地 Linux CMake 配置、严格构建及受影响的 `package.extracted_runtime` 测试通过。

Release 工作流现对 Windows 执行完整 CTest，`PYTHONUTF8=1` 保证中文测试输出可用。
本次 `publish_release=false`，未创建 tag 或 GitHub Release；发行 artifact 保留 14 天。

## 剩余发布条件

计划 008、009 可以完成。正式 `v0.2.0` 仍需收敛网络性能噪声、确定回归阈值，以及准备版本号、
Changelog、迁移和发布说明。固定虚拟机采集现状见[性能基线记录](performance-baseline.md)；
共享 Hosted Runner 的短性能测试不能替代固定机双时段稳定基线。
