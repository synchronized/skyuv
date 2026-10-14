# v0.2.0 Linux 功能与交付收口

## 范围与状态

2026-10-14，对当前 v0.2.0 范围进行 Linux 收口。实现与新增验收提交为 `519278c`、`2064fd0`。
本次完成当前环境可执行的 Linux 功能和交付验证；正式固定机性能基线仍未完成，不能据此
宣告所有 Linux 门槛、计划 006 或正式 v0.2.0 已完成。Windows 新行为验证也不由本记录替代。

## 各计划的 Linux 验收

| 计划 | 已完成的 Linux 结果 | 尚未满足的条件 |
| --- | --- | --- |
| 006 性能与稳定性 | 五类探索性原版对照；背压 OOM 负载修复；并发压力及 Sanitizer 回归 | 固定 Runner、双时段权威采集及回归阈值 |
| 007 交付 | Runtime 安装、发行归档、SHA-256、许可证、包外启动及 TCP echo | 正式发布仍依赖性能门槛与其他平台验收 |
| 008 路径 | 新旧入口、覆盖顺序、日志重开恢复、PID 生命周期、Unix 只读安装树与解压包 | Windows 新模式与只读 ACL 验收 |
| 009 外部模块 | 源码嵌入与可重定位安装 package 的构建、独立 Lua 动态 require | Windows 新动态加载及完成记录收口 |

## 独立 Lua 动态加载

`cmake.consumer_dynamic` 安装 Development 组件后移动安装前缀，再从独立工程执行
`find_package(skyuv)`；另一轮使用 `add_subdirectory` 嵌入源码。两种方式都编译真实 MODULE，
由独立 Lua 解释器通过模块实际输出目录的 `package.cpath` 执行 `require("skyuv_consumer")`，
校验导出函数；清除模块缓存和搜索路径后确认加载失败。该验证不以 `luaL_requiref` 静态注册
代替动态加载，不承诺任意第三方 Lua 二进制模块兼容性。

示例自有目标已启用 C11 和项目警告策略，并为 Windows 动态模块配置符号导出；Windows
新路径尚未执行本机验收，配置存在不等于已验证。

## 解压包新模式

`package.extracted_runtime` 在已有校验、许可证、启动和 echo 测试之后，增加以下实际运行：

- 解压目录、配置、日志、运行目录及两个调用目录均使用含空格路径；
- Unix 解压程序树移除写权限；包外配置仍可加载随包模块并将日志写到外置目录；
- 通过 `skyuv.control.reopen_log()` 请求重开，并等待本次日志标记落盘再结束；
- 两个不同调用目录的前台启动使用相同日志根，且不创建运行目录；
- Unix daemon 在同一外置运行根创建 PID，冲突启动失败，正常退出删除 PID 并保留协调锁文件；
- 测试结束恢复临时程序树原权限，生成目录与归档不提交到源码仓库。

该测试也在 macOS 运行；Windows 运行前台路径部分，但不以 Unix chmod 验证替代 Windows ACL。

## 最终验证

- 本机 Linux Clang Release 配置、严格构建、CTest 72/72 通过。
- 提交 `2064fd0` 的 [Linux 完整 CI](https://github.com/synchronized/skyuv/actions/runs/38543392301)
  全部通过：GCC/Clang Debug/Release、system allocator Debug、TSan、ASan/UBSan。
- 同提交的 [macOS Debug/Release CI](https://github.com/synchronized/skyuv/actions/runs/38543398612)
  全部通过，包括新增的独立模块动态加载与解压包新模式验收。
- 第三方源码工作树保持干净；所有新增测试通过 CMake 接入。

## 性能基线外部条件

GitHub 仓库的自托管 Runner 列表当前为空。本机为 Windows 宿主机上的 VMware Linux，调频
接口未暴露；按已确认的本轮约定不补录调频和宿主机电源计划，数据保持探索性属性。
固定环境是否采用此虚拟机、以及是否为它修改正式预检规则，尚需明确；不通过伪造调频数据、
把探索结果标成权威结果或跳过既有预检完成门槛。

后续需要部署固定 Runner，在相同提交和参数下完成两个独立时段的采集，评估噪声后再设置
阈值。原始探索数据和现有基线状态见 [性能基线记录](performance-baseline.md)，部署要求见
[固定 Runner 指南](../guides/performance-runner.md)。
