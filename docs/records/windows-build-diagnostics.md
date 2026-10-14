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

## 2026-10-13 Windows stat 回退路径恢复修复

固定 libuv 版本的 `fs__stat_directory()` 仅在拆分目录与文件名时赋值 splitchar，
退出时却以 split 非零判断是否恢复。因此根路径、盘符、末尾分隔符及纯点号路径可能
用未初始化字符覆盖原路径；当 split 为零且 includes_name 为假时，分隔符判断还会读
path[-1]。这些控制流问题不限于编译器误报，但实际触发还依赖 stat 兼容回退及访问失败条件。

独立补丁 `0003-Fix-Windows-stat-path-restoration.patch` 将 splitchar 的零值表示为未拆分，
只在保存了实际分隔符后恢复；检查分隔符前增加 split 非零条件。仅替换构建目录 fs.c，
静态/共享目标都接入，不修改第三方子模块，不关闭 C4701。

新增 `libuv.stat_path_restoration` 从实际编译副本提取完整回退函数，模拟系统边界，
覆盖 11 类路径与成功、打开失败、Unicode 转换失败、目录查询失败、两类卷信息查询失败，
验证缓冲区全量保持、错误码、句柄关闭次数和成功结果分配次数。模拟系统调用允许空路径，
仅用于验证函数内部边界，不表示公共 stat API 接受空路径。
测试首次构建因私有头目录中的 winsock.h 遮蔽 Windows SDK 头失败；改为相对路径包含
winapi.h，并移除该私有目录搜索项后构建通过，未降低严格警告。
复现时将生成目录夹具暂时恢复为原版控制流，并把原来未初始化的 splitchar 填为 `L'Z'`，
使错误写入可确定观察而不依赖栈内容或触发运行时未初始化检查。该夹具构建成功，路径保持
断言失败，CTest 退出码 8；随后恢复实际补丁函数。此证据验证原恢复条件错误，不宣称已在
真实旧 Windows/ACL 环境重现整条公共 API 调用链；复现夹具不提交。
MSVC Debug 配置、构建与 72/72 测试通过（59.16 秒），新增回退测试连续 100 次通过；
Clang Debug 配置、构建与 72/72 测试通过（61.80 秒），本轮输出无 warning。
clang-cl Debug 新增测试目标构建通过，测试连续 20 次通过。fs.c 的 splitchar C4701
已消除，另外两处 original_position C4701 保留；Windows Release、共享 libuv 及真实
旧版 stat 回退环境本轮未覆盖，beta.4 完整验收仍待完成。
提交 `394e138` 的 [Linux CI](https://github.com/synchronized/skyuv/actions/runs/38356058512)
及 [macOS CI](https://github.com/synchronized/skyuv/actions/runs/38356061845) 全部成功。

## 2026-10-13 libuv 转换诊断分层策略

用户确认日常构建局部屏蔽转换提示，保留诊断审查入口。新增 CMake 开关
`SKYUV_LIBUV_CONVERSION_DIAGNOSTICS`，默认 OFF；Windows MSVC/clang-cl 仅对 uv、uv_a
PRIVATE 设置 `/wd4244 /wd4245 /wd4267`，ON 时不设置，保持上游 `/W4`。
Clang GNU 前端和非 Windows 平台不受影响；不修改第三方源码、不向消费者传播，
不降低 skyuv 的严格警告，不关闭其他诊断，不代表 155 处转换已证明安全。

MSVC Debug 全量重建成功，三类转换提示为零，剩余 13 条：C4701 6、C4152 2、
C4702 2、C4819 3。全套 72/72 测试通过（115.25 秒）。生成工程检查确认 uv_a
带局部选项，64 个自有目标均不带。clang-cl 相关构建与三项 libuv 测试通过；首次
测试缺少初始化测试产物，补齐目标后复验成功，不修改测试来绕过失败。
原始 155 条转换基线继续保留，后续依赖升级与定期审查通过诊断开关采样。
MSVC 设为 ON 后重新配置并构建 uv_a，恢复 C4244 107、C4245 28、C4267 20 条，
构建成功；随后恢复 OFF，重新配置与 uv_a 构建成功。开关控制输出，不改变源码语义。
clang-cl 编译数据库确认 37 条 libuv 条目带屏蔽选项，没有扩散到其他来源。
提交 `d17fc35` 的 [Linux CI](https://github.com/synchronized/skyuv/actions/runs/38359917692)
及 [macOS CI](https://github.com/synchronized/skyuv/actions/runs/38359929436) 全部成功。

## 2026-10-13 合并 libuv 总诊断开关与 cmocka 编码

用户确认合并为 `SKYUV_LIBUV_DIAGNOSTICS`，默认 OFF，仅对 Windows MSVC/clang-cl 的
uv、uv_a PRIVATE 关闭 C4244/C4245/C4267/C4701/C4152/C4702；ON 恢复这些提示。
清单外的新诊断继续保留，不全局关闭警告。旧转换开关仅在新开关尚未设置时迁移初始值，
新开关显式值与后续缓存值优先。未初始化诊断被屏蔽不表示全部系统调用错误组合已验证安全。

cmocka、cmocka-static 存在时在上述前端使用 PRIVATE `/utf-8`，按头文件真实编码处理
C4819，不关闭该警告；自有目标继续沿用严格警告策略，第三方源码工作树保持干净。
MSVC Debug 全量重建 0 条 warning，72/72 测试通过（61.44 秒）；clang-cl Debug
构建与 72/72 测试通过（92.05 秒），仍有 8 条 libuv 未使用函数提示，本轮不扩大屏蔽范围。
MSVC 的 64 个自有目标未带屏蔽选项；clang-cl 编译数据库验证选项仅在 libuv 范围内，
cmocka 编译条目带 `/utf-8`。Clang GNU 前端与非 Windows 不受该编号策略影响；
Windows Release 与共享 libuv 本轮未复验，beta.4 发布验收仍待完成。
删除新缓存并以旧转换开关 ON 重新配置时，新开关迁移为 ON；uv_a 构建成功，恢复
C4244 107、C4245 28、C4267 20、C4701 6、C4152 2、C4702 2，共 165 条。
随后显式设置新开关 OFF，在旧值仍为 ON 时也正确生效，重新配置与 uv_a 构建成功。
提交 `f1cfca1` 的 [Linux CI](https://github.com/synchronized/skyuv/actions/runs/38363773110)
及 [macOS CI](https://github.com/synchronized/skyuv/actions/runs/38363785961) 全部成功。

## 2026-10-13 clang-cl 未使用函数提示

将 Windows Clang GNU 前端已审查的 unused 策略扩展到 clang-cl，统一受
`SKYUV_LIBUV_DIAGNOSTICS` 控制。OFF 时 libuv 目标 PRIVATE 定义 UV__UNUSED 属性，
只对 timer.c/win/core.c 设置未使用函数例外；clang-cl 使用 `/clang:-Wno-unused-function`，
GNU 前端仍使用 `-Wno-unused-function`。ON 时两种前端均撤销这些设置，恢复原始提示。
CRT 独立策略不变，不关闭所有警告，不修改第三方源码或自有目标严格检查。

clang-cl Debug 配置、构建及 72/72 测试通过（75.64 秒），本轮构建 0 warning。
Clang GNU Debug 配置、相关目标构建和三项 libuv 测试通过；编译命令确认例外只在
libuv 范围内，函数抑制只作用于两个已审查的翻译单元。Windows Release 和共享 libuv
矩阵仍待集中验收，不能据此宣称 beta.4 已结束。
clang-cl 将总开关设为 ON 并重建 uv_a 成功，恢复 8 条未使用函数提示；随后恢复 OFF，
重新配置与 uv_a 构建成功，输出 0 warning。例外可恢复，原始诊断未被永久删除。
提交 `2092a42` 的 [Linux CI](https://github.com/synchronized/skyuv/actions/runs/38366738054)
及 [macOS CI](https://github.com/synchronized/skyuv/actions/runs/38366744354) 全部成功。

## 2026-10-13 Release 验收中的进程指标测试同步

clang-cl Release 首轮构建成功，但 72 项测试中 benchmark.process_metrics 失败。
该测试先启动睡眠 0.15 秒的子进程，再启动监控；并发构建时父进程可能晚调度，
监控只覆盖子进程末段，导致 monitored_wall_seconds >= 0.1 的断言失败。
这是测试启动时序假设，不是编译器 Release 代码失败。

测试子进程改为等待 stdin 启动消息，再进行分配与睡眠；父进程在实际监控启动后释放。
首条路径特意延迟父进程 0.2 秒，验证不依赖父进程及时调度；wait_with_metrics 路径仅在
真实 ProcessMetrics.start 后释放子进程，不修改生产监控协议或放宽时长断言。
clang-cl Release 对应测试连续 20 次通过，全套 72/72 复验通过。
提交 `874c6c5` 的 [Linux CI](https://github.com/synchronized/skyuv/actions/runs/38369312404)
及 [macOS CI](https://github.com/synchronized/skyuv/actions/runs/38369318294) 全部成功。

## 2026-10-13 Windows Release 与构建运行目录

运行目录调整前，静态 MSVC、clang-cl、Clang Release 均构建成功且全套 72/72 测试
通过（clang-cl 在上述测试同步修复后复验）。Release 新出现 libuv 断言变量提示：
MSVC C4189 三处，Clang 两种前端五处，按源位置去重；保留，不扩大日常屏蔽清单。
getaddrinfo.c 的 r、pipe.c 的 r、signal.c 的 r 仅供 assert 检查返回结果；signal.c 的
removed_handle 仅供 assert 检查树删除结果，util.c 的 len 仅供 assert 检查格式化结果。
Release 删除断言后变量未使用，相关操作调用仍执行。此审查不宣称被断言保护的所有
错误组合已验证安全，不通过删除调用消除提示。

既有 C4152 两处为 thread.c 通过 GetProcAddress 动态取得线程命名函数地址时的
函数/数据指针扩展转换；C4702 两处为 uv_sem_trywait 的 abort 后返回及 tty 的无限循环
后返回。保留源码依据，由统一诊断开关控制原有编号，不通过改变线程或 tty 行为消除警告。

共享 Windows 构建初轮发现 uv.dll 只在第三方输出目录，测试程序目录无法加载依赖，
产生系统模态弹窗；停止对应共享 CTest 后处理部署。用户确认统一 bin 运行布局：
可执行程序及 uv.dll 直接输出到 build/<preset>/bin；其他 DLL 由独立部署目标复制，
portable 模块保留 cservice/luaclib 子目录，源模块输出供现有目标引用，不改安装包布局。
多配置生成器不额外叠加 Debug/Release，要求每种配置使用独立 preset 构建目录。

新增 windows.runtime_layout 检查运行程序、DLL 存在性及复制 DLL 的 SHA-256 一致性，
作为其他 Windows 测试的前置 fixture；缺少依赖时跳过程序测试，避免重复弹窗。
部署目标原先与反向依赖测试程序的 DLL 构成环，已改为程序仅依赖 uv，部署目标独立
依赖 DLL。暂时移动共享构建的 uv.dll 后，检查失败、lua.interpreter 未运行，CTest
退出码 8；finally 恢复 DLL 后全套通过。未提交生成文件或复现用移动。

调整后 MSVC、clang-cl、Clang 的共享 Release 全套各 73/73 通过，分别 57.64、84.74、
55.50 秒，涵盖安装、发行包内容、解包运行及 Lua 客户端测试。共享构建同时编译 uv/uv_a
时同一第三方诊断可能重复输出，仍按源位置识别，不承诺所有 Release 第三方提示归零。
默认静态 MSVC Debug 新布局配置、构建及 73/73 测试通过（60.54 秒）。原始 Debug
其他前端采样和静态 Release 结果见前述记录，新的 bin 布局本轮额外覆盖三种共享 Release
和默认静态 Debug；未将剩余版本发布门槛写成已完成，不修改当前 beta.3 版本号。
提交 `722c8c8` 的 [Linux CI](https://github.com/synchronized/skyuv/actions/runs/38370895011)
及 [macOS CI](https://github.com/synchronized/skyuv/actions/runs/38370902365) 全部成功。

## 2026-10-13 独立 project 运行根目录

用户明确运行根目录应为 build/<preset>/project，而不是 build/<preset>/bin。
Windows 程序及 uv.dll 改为 project/bin，服务 DLL 为同级 project/cservice，Lua DLL
为同级 project/luaclib，保留 Lua 模块子目录。部署脚本与存在性/哈希检查均使用新根目录。
不修改安装包布局，不把 project 写成已包含完整 Lua 源码与配置的发行包。

新增 windows.project_client_modules 从明确的 project/bin/lua.exe 加载
project/luaclib 中 client.socket 与 client.crypt，验证新布局可运行，而不只检查文件存在。
共享 MSVC Release 先完成原全套 73/73 测试（58.71 秒），随后新目录加载与前置部署
检查两项通过。静态 MSVC Debug 首轮日志测试只留下 bootstrap 记录，未见预期标记；
同一测试连续 20 次复验通过，未放宽断言或修改日志实现，首次失败原因未确定。
随后静态 MSVC Debug 全套 74/74 复验通过（55.86 秒），包含新目录客户端加载测试。
提交 `0613cb0` 的 [Linux CI](https://github.com/synchronized/skyuv/actions/runs/38373080374)
及 [macOS CI](https://github.com/synchronized/skyuv/actions/runs/38373087474) 全部成功。

## 2026-10-14 最终矩阵中的路径测试复制范围

从 `3446696` 初始化子模块并建立新的 Windows 构建目录。MSVC 使用 19.42.34436.0，
clang-cl 使用 Clang 22.1.8；Python 为 3.9.13。MSVC Debug 与 clang-cl Debug 首轮各有
`skynet.portable.paths` 超过 15 秒，其余 73 项通过；单项复验通过。MSVC 单项连续 20 次
和随后全套 74 项复验通过，但 clang-cl 冷构建再次出现同样超时，不能仅以重试作为处置。

超时遗留的临时目录仅完成部分 Lua 模块复制，尚未复制服务 DLL。原脚本递归复制整个
`3rd/portable`，包含 PDB；clang-cl Debug 输入共 24 个文件、97,948,706 字节。
此证据把本次超时定位在测试准备阶段，不证明 Skynet 启动或 DLL 路径语义发生失败，
也不能据此确定磁盘或防病毒软件是唯一原因。

路径测试改为仅递归复制 DLL，保留模块目录和含空格的临时根目录，启动标志断言和
15 秒 CTest 超时不变。现有真实模块加载测试继续验证复制内容足以启动服务并加载
`client.socket`；clang-cl Debug 修改后连续 20 次通过，总计 20.63 秒（含 fixture 重复）。
本机原始日志位于 `build/beta4-final-*.log` 和 `build/beta4-paths-dll-only.log`，不提交。

随后重新生成 MSVC Debug DLL 后，整个运行 DLL 树的递归复制仍超时，临时目录同样仅有
部分 Lua DLL。因此只排除 PDB 不能作为最终修复。夹具进一步只复制 logger、snlua、harbor、
skynet 与 client 五个启动链模块，增加逐模块准备日志，继续保留原目录含空格、真实启动
断言和 15 秒超时。遗漏 harbor 的初次最小夹具明确报服务加载失败，补齐后正常启动；
成功运行的复制阶段耗时 0.034 秒。这一失败确认夹具仍检查实际服务依赖，没有绕过启动。

## 2026-10-14 集中本地验收与发布阻塞

六种静态组合完成 CMake 配置、构建和全套 CTest，三种编译器的共享 Release 使用独立
构建目录完成同样验证。各组合均有 74 项测试；最终最小路径夹具另在九种组合中重复验证，
避免把早期递归复制夹具的成功结果作为最终修复证据。版本标识切换前的构建仍为 beta.3；
这批结果用于验收 beta.4 的实现，不代表 beta.4 发行包已经发布。

| 编译器 | 静态 Debug | 静态 Release | 共享 Release |
|---|---|---|---|
| MSVC 19.42.34436.0 | 74/74 | 74/74 | 74/74 |
| clang-cl 22.1.8 | 74/74 | 74/74 | 74/74 |
| Clang GNU 前端 22.1.8 | 74/74 | 74/74 | 74/74 |

默认 Debug 诊断输出为零；Release 保留此前审查的 libuv 断言变量提示，MSVC 按位置去重
为 3 处，Clang 两种前端为 5 处。共享构建同时编译 uv/uv_a 会重复打印相同源位置，
不把重复输出计为新的缺陷。MSVC 静态 Release 的首次完整构建警告与共享采样一致，
随后增量构建日志不能用于声称其原始完整构建零警告。

MSVC Debug 的总诊断开关 ON 重建成功，恢复 C4244 107、C4245 28、C4267 20、
C4701 6、C4152 2、C4702 2，共 165 条；恢复 OFF 后完整构建成功。
六种静态组合各 34 个自有编译条目均启用警告即错误，不含第三方 CRT 宏、libuv unused
属性或局部警告抑制。原始 JSON 位于 `build/beta4-compile-audit.json`，不提交。
所有第三方子模块工作树干净，未升级依赖。

路径测试首批修复提交 `aa5a9b0` 已推送。随后手动触发 Linux/macOS CI 均收到 HTTP 401，
`gh api user` 同样确认本机认证失效；这些工作流未运行，不能记作通过或测试失败。
恢复 GitHub CLI 认证后，必须对最终准备提交触发 Linux/macOS CI 并查看最终结果，再执行
三平台 Release、SHA-256 和公开资产核验。当前尚无 beta.4 tag 或 Release。
