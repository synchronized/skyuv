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

## 2026-10-13 libuv IPv6 初始化括号补丁

Windows SDK 10.0.22621.0 的 IN6_ADDR 包含结构体、联合体和 Byte[16] 数组，
IN6ADDR_LOOPBACK_INIT 只包含数组层括号。补丁补齐外层括号，不改变回环地址值。
见 [libuv 补丁说明](../../patches/libuv/README.md)。依赖提交保持不变。

构建入口复制单个 winsock.c 到生成目录，应用独立补丁，再替换静态/共享 libuv 编译输入。
每次配置从上游文件重新复制，允许重复配置；检查 Git 退出码、补丁后的内容和编译输入匹配，
任何失配均配置失败。首次尝试 Git 从构建子目录运行时跳过补丁却返回成功，已修正为从源码
根目录指定相对路径应用，并增加内容校验，避免仅凭退出码误判。

Clang Debug 全量重建仅剩 26 条第三方 CRT 警告、90 条 note，libuv 无警告。
全套 CTest 70/70 通过（62.15 秒）。新增测试直接检查 :: 转为 ::1 的 16 字节、端口保持、
非零 IPv6 地址保持及无效参数；最终头文件搜索顺序调整后，Clang 网络相关三项及 MSVC
初始化/客户端两项复验通过。共享 libuv 构建及初始化测试也通过。
第三方子模块不修改，cmocka/Skynet CRT 提示和其他编译组合仍待后续治理。
提交 `2defc29` 的 [Linux CI](https://github.com/synchronized/skyuv/actions/runs/38344122237)
及 [macOS CI](https://github.com/synchronized/skyuv/actions/runs/38344125789) 全部成功。

## 2026-10-13 统一 Windows 第三方 CRT 策略

用户确认三种 Windows 编译器统一局部处理第三方 CRT 提示。
`cmake/skyuv_third_party_crt.cmake` 在项目目标定义完毕后按源码目录归属遍历：
纯第三方目标定义 PRIVATE `_CRT_SECURE_NO_WARNINGS`，混合目标仅对第三方源文件设置该宏。
原始第三方目录与构建目录第三方副本均覆盖；生成器表达式不推断为纯第三方来源。
Windows MSVC/模拟 MSVC 工具链以外不启用；没有 PUBLIC 定义、全局标志或警告等级调整。

Clang Debug 全量重建为 0 warning、0 note，全套 70/70 测试通过（58.33 秒）。
clang-cl Debug 配置、全量构建与 70/70 测试通过（111.56 秒）；仍有 8 条 libuv
未使用函数提示，本规则不关闭它们。编译命令审查在两种 Clang 中均确认 117 个第三方条目
带宏、32 个自有条目不带宏。MSVC Debug 主程序、client 模块、解释器及相关测试通过。
首次 MSVC 模块测试缺少 Lua 解释器产物，补齐目标后复验成功，没有修改源码以绕过测试。
Release 组合、MSVC 高风险诊断及 beta.4 发布验收仍待完成。
提交 `a3a3a1a` 的 [Linux CI](https://github.com/synchronized/skyuv/actions/runs/38346082650)
及 [macOS CI](https://github.com/synchronized/skyuv/actions/runs/38346086989) 全部成功。

## 2026-10-13 MSVC 高风险诊断复核

当前提交全量 MSVC Debug 重建退出码 0，日志 `build/beta4-msvc-review-build.log`。
仍为 171 条，编号和数量与首轮相同；CRT 策略不影响这些诊断。

| 位置 | 控制流结论 | 本轮处置 |
|---|---|---|
| thread.c 两处 C4334 | `uv_cpumask_size()` 在 x64 返回 64，原表达式在 32 位 `int` 上移位后才转换为 `DWORD_PTR` | 独立补丁在移位前转换为指针宽度无符号整数 |
| fs.c 两处 original_position | 仅当 SetFilePointerEx 成功并设置 restore_position 后使用输出值 | 保留诊断，不用默认值掩盖系统调用契约 |
| core.c user_timeout | 仅在 UV_METRICS_IDLE_TIME 分支赋值且设置 reset_timeout，同一标志保护读取 | 保留诊断，属于关联状态控制流 |
| pipe.c duplex_flags | open_named_pipe 的每条成功返回路径均写出参数，失败则不读取 | 保留诊断，属于函数成功返回契约 |
| process-stdio.c fdopt | 超过 stdio_count 时只设 UV_IGNORE，只有其他分支读取 data 联合体 | 保留诊断，不改写整个结构体以消除提示 |
| winsock.c iosb | 先写 iosb_ptr->Status；等待完成后通过局部 iosb.Status 读取，同一对象存在指针别名 | 保留诊断；本轮未覆盖 NtDeviceIoControlFile 全部错误组合 |
| fs.c splitchar | 仅拆分路径分支赋值，cleanup 却只以 split 非零保护恢复；不拆分的根路径、相对目录也可能进入恢复 | 记录潜在真实缺陷，需针对 stat 兼容回退路径建立复现后独立修复，不关闭 C4701 |

亲和性补丁仅应用到构建副本，不涉及 Skynet worker 迁移或 CPU group 扩展。
测试既调用真实 libuv API 并恢复原线程掩码，也从实际编译副本提取该函数，模拟进程/线程
掩码，逐位覆盖允许与不允许的 CPU，包括 31、32、63 位；不依赖本机 CPU 数量。
本机只有 6 个逻辑 CPU，单独的实际 API 测试在原版库上也通过，因此不能作为高位缺陷复现证据。
随后将生成目录测试夹具中的表达式暂时恢复为原版（仅夹具关闭 C4334，以便运行），
实际编译输入保持补丁版：原表达式构建成功，但高位测试失败，CTest 退出码 8。
恢复补丁表达式后构建通过，测试连续 100 次通过；没有提交复现用的生成文件或关闭警告设置。
MSVC Debug 全套 71/71 测试通过；本轮补丁消除两处 C4334，其余类别保留。
Clang Debug 配置、构建与 71/71 测试通过，本轮输出无 warning；clang-cl Debug
配置与新增测试目标构建通过，亲和性测试连续 20 次通过。Windows Release 与共享 libuv
组合本轮未复验；其余高风险诊断尚未全部修复，不能据此宣称 beta.4 完成。
提交 `8cbf15b` 的 [Linux CI](https://github.com/synchronized/skyuv/actions/runs/38348783693)
及 [macOS CI](https://github.com/synchronized/skyuv/actions/runs/38348787730) 全部成功。
