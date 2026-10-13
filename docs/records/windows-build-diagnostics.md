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

## 2026-10-13 Windows Clang Debug 首批治理

基线源码提交 `a627242`，Clang 23.1.1，GNU 命令行前端、MSVC ABI。
使用 `scripts/msvc.ps1` 初始化环境，再执行 `cmake --build --preset windows-clang-debug --clean-first`。
完整重建成功；本机日志为 `build/beta4-clang-before.log` 和 `build/beta4-clang-after.log`。

| 诊断 | 修复前 | 首批处理后 |
|---|---:|---:|
| CRT 弃用提示 | 45 | 26 |
| 未使用静态函数 | 8 | 0 |
| 初始化子对象缺少括号 | 1 | 1 |
| warning 总数 | 54 | 27 |
| note 上下文 | 177 | 91 |

完整日志比用户提供的截断片段多出第三方 CRT 提示。首批处理仅作用于 Windows Clang GNU
前端的 libuv 目标：定义 `_CRT_SECURE_NO_WARNINGS`，通过 `UV__UNUSED` 扩展入口为红黑树
生成函数提供 unused 属性；heap-inl.h 的静态实现仅在 timer.c 与 win/core.c 两个翻译单元
局部豁免 `-Wunused-function`。作用域为 libuv 子目录，不传播到 skyuv 自有目标。

CRT 宏只移除 Microsoft CRT 的供应商弃用标记，不代表所有调用已证明无缺陷，也不修改
函数、缓冲区或 ABI。堆函数例外只针对完整采样中确认的共享头文件实现；libuv 升级或调整
这两个翻译单元时应重新审查并移除不再必要的例外。
libuv 的 winsock.c 初始化括号提示，以及 cmocka/Skynet 的 CRT 提示本批保留，后续分别处理。
MSVC、clang-cl、Release 及 beta.4 完整验收仍待完成，不能宣称 beta.4 已发布。
Windows Clang Debug 本轮 CTest 69/69 通过，耗时 98.62 秒，包含交付和稳定性标签测试。
提交 `c69c549` 的 [Linux CI](https://github.com/synchronized/skyuv/actions/runs/38340119711)
及 [macOS CI](https://github.com/synchronized/skyuv/actions/runs/38340122894) 全部成功。
Windows MSVC Debug 配置、客户端运行时构建和对应 CTest 通过，第三方子模块工作树干净。
这些验证不替代 beta.4 要求的六种 Windows 组合完整验收。
