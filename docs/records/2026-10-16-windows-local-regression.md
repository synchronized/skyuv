# 2026-10-16 Windows 本机回归

## 范围与环境

对 Linux 收口与 v0.2.0 发布后的代码补充 Windows 宿主机验证，源码提交为 `3c85619`。
该提交相对 `v0.2.0` tag 的 src/include/patches/3rd 无差异。本记录描述本机运行，
不替代此前的 [Windows Actions 验收](windows-v0.2.0-actions-acceptance.md)。

- Windows x64，Visual Studio 2022，MSVC 19.41.34123.0。
- clang-cl 23.1.1，CMake 4.4.3；Python 测试设置 `PYTHONUTF8=1`。
- 自有目标保持默认严格警告和警告即错误；没有为本次验证调整实现、警告或断言。
- 本地两个原有 .gitkeep 删除未纳入本次提交；第三方版本未更改。

## 配置、构建与全套测试

| 构建组合 | 构建方式 | CTest | 测试耗时 |
|---|---|---|---:|
| MSVC Debug、静态 libuv | windows-vs2022-debug，clean-first | 82/82 通过 | 182.88 秒 |
| MSVC Release、静态 libuv | windows-vs2022-release，clean-first | 82/82 通过 | 186.76 秒 |
| clang-cl Release、共享 libuv | 独立共享构建目录，配置及完整构建 | 82/82 通过 | 152.04 秒 |

MSVC 两组使用 `scripts/msvc.ps1 -Action configure` 导入环境，执行对应 preset 的
`cmake --build --clean-first --parallel 3` 与 `ctest --output-on-failure`。
clang-cl 使用 windows-clang-cl-release preset，覆盖构建目录为
`build/beta4-clangcl-shared-release` 并设置 `SKYUV_USE_SHARED_LIBUV=ON`；目录名沿用早期
验证目录，本次实际配置和构建版本为 v0.2.0。测试期间各配置使用独立构建目录。

MSVC Debug 构建 0 warning；MSVC Release 保留三条 libuv C4189，均为既有断言变量提示。
clang-cl 共享组合本次构建输出 0 warning；该组合为已有目录的完整增量构建，不是全量
诊断采样，不据此声称所有 Release 第三方源码零警告。

## Linux 近期改动对应的 Windows 覆盖

- `skynet.portable.paths_legacy`、`skynet.portable.runtime_paths`：旧入口兼容、新目录根、
  配置 include 与覆盖顺序、含空格路径、不同调用目录、日志重开与错误路径。
- `package.extracted_runtime`：实际 Windows ACL 拒绝写入的程序树、包外配置与日志、
  解包启动与 TCP echo；Windows daemon 按不支持返回，不创建日志和运行目录。
- `platform.logger_reopen`：重开失败保留旧句柄、恢复后写入新文件；每次测试内部重复 20 次。
- `cmake.consumer_dynamic`：源码嵌入、移动 Development 安装前缀后的 find_package，
  编译真实 Lua MODULE 并通过独立解释器动态 require，同时验证搜索路径错误。
- `runtime.project`：完整 project 目录、目录外启动、echo、客户端模块加载，以及删除运行
  产物后通过 skyuv_stage_runtime 再次部署；本机静态和共享 libuv 均覆盖。
- 性能回归、噪声、预检与背压队列等工具协议测试均在全套通过；这些检查不属于 Windows
  权威性能采样，不能把 Linux 固定虚拟机的性能数据解释为 Windows 性能结果。

## 日志与复现

重点重复验证全部通过：

- MSVC Debug：paths_legacy、runtime_paths、logger_file、platform.logger_reopen、
  socket.stress、client.runtime、lua.client_libuv 各连续 10 次；连同部署前置检查，
  CTest 汇总 8/8，通过耗时 166.31 秒。日志重开测试内部重复累计 200 次。
- clang-cl 共享 Release：windows.project_client_modules、client.runtime、lua.client_libuv
  各连续 20 次；连同部署前置检查，CTest 汇总 4/4，通过耗时 39.56 秒。
- 使用 `ctest --repeat until-fail:<次数>`，保留原有断言，没有跳过失败测试。
- 验收期间 fetch 后确认 origin/main 与被测提交一致；本次没有发现需要修复的实现问题。

本机生成日志位于忽略目录，不提交二进制或原始日志：

- `build/windows-local-20261016-debug-{config,build,tests}.log`
- `build/windows-local-20261016-release-{config,build,tests}.log`
- `build/windows-local-20261016-shared-{config,build,tests}.log`
- `build/windows-local-20261016-debug-repeat.log`
- `build/windows-local-20261016-shared-repeat.log`

本次不重发版本、不移动 tag、不替换公开发行归档。未进行新的长时 soak、全部六种编译器
Debug/Release 组合或 Windows 固定机性能采样；如后续修改平台实现、网络生命周期或分配器，
再按对应风险补充这些验证。
