# Windows 构建诊断首轮记录

## 采样身份与方法

- 日期：2026-10-12；源码提交：`310989f20c2cd6108c7b452f41cf2a046c819895`。
- 平台：Windows x64，Visual Studio 2022，MSVC `19.41.34123.0`。
- 命令：`cmake --build --preset windows-vs2022-debug --clean-first`，退出码 0。
- 本机原始日志：`build/windows-warning-audit-msvc.log`，属于生成文件，不提交。
- 本表统计日志中 `warning Cxxxx` 的出现次数；本次无重复汇总警告，
  不将随附说明计作独立警告。后续跨工具链比较应按源位置和诊断编号去重。

## 结果与处置优先级

| 诊断 | 数量 | 来源 | 初步处置 |
|---|---:|---|---|
| C4244 类型窄化 | 107 | libuv | 审查范围与边界，不能仅增加强制转换 |
| C4245 有符号/无符号转换 | 28 | libuv | 审查负值哨兵和符号语义 |
| C4267 size_t 窄化 | 20 | libuv | 审查长度上限和溢出 |
| C4701 可能未初始化 | 7 | libuv Windows 后端 | 高优先级核对控制流，确认是否真实缺陷 |
| C4819 当前代码页无法表示字符 | 3 | cmocka 头文件 | 优先在具体目标设置源编码 |
| C4152 函数/数据指针转换 | 2 | libuv Windows thread | 核对 Windows API 契约及编译器扩展 |
| C4334 32 位移位转为 64 位 | 2 | libuv Windows thread | 高优先级核对位宽与移位范围 |
| C4702 不可达代码 | 2 | libuv Windows tty/thread | 核对条件与平台分支，记录保留理由或修复 |

共 171 条，libuv 168 条、cmocka 3 条。该构建未发现 skyuv 自有源码警告。
这证明当前采样存在第三方诊断，不证明其中每条都是运行时缺陷。

C4701 涉及 `fs.c` 的 `original_position`、`splitchar`，`core.c` 的 `user_timeout`，
`winsock.c` 的 `iosb`，`process-stdio.c` 的 `fdopt`，`pipe.c` 的 `duplex_flags`。
C4334 位于 `win/thread.c` 的 197、198 行，后续审查以固定子模块提交为准。

“编译源文件”、声明位置和 `note` 应关联主诊断阅读；本轮 MSVC 输出不能代表
clang-cl/Clang 的 `note` 清单。未采集 Release 和其他 Windows 工具链的完整诊断。

## 后续与验证边界

实施与多工具链验收见 [010 计划](../plans/010-windows-build-diagnostics.md)。
本轮只建立诊断基线和版本计划，没有修复编译警告，也未改变依赖版本或警告选项。
全量重建后执行 `ctest --preset windows-vs2022-debug --output-on-failure`，
64/64 测试通过，耗时 77.31 秒。本轮仅修改文档，不要求重新触发平台 CI。
