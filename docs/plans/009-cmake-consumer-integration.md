# 009：CMake 第三方消费与 Lua C 模块接入

## 状态

进行中。嵌入式构建、安装导出和外部模块示例已完成基础实现，剩余独立 Lua 解释器动态
`require` 验证和完成记录。

## 背景

当前 skyuv 的 CMake 目标主要服务于仓库自身构建。需要编译 Lua C 模块的外部项目仍需手动指定
生成的库文件、Lua 头文件、Skynet 内部头文件和平台库，容易因配置、平台或构建类型不同而失效。

同时，skyuv 的部分 CMake 逻辑默认自己是顶层工程。通过 `add_subdirectory` 或 `FetchContent`
嵌入时，宿主工程的源码目录、二进制目录、测试和安装规则可能被错误使用。

## 目标

- 支持 skyuv 作为 `add_subdirectory` 或 `FetchContent` 子项目构建；
- 使路径解析、补丁副本和生成文件只依赖 skyuv 自身的源码及二进制目录；
- 提供可安装、可导出的 CMake package，支持 `find_package(skyuv CONFIG REQUIRED)`；
- 为可公开消费的 Lua C 模块场景提供稳定且有边界说明的 target；
- 提供最小外部项目示例，验证编译、链接、安装和运行时加载链路。

## 不在范围内

- 不承诺 Skynet 内部结构体或未安装头文件的长期 ABI 稳定性；
- 不直接修改 `3rd/` 中的第三方源码；
- 不把所有 Skynet 内部 Lua C 模块都包装成独立公共 SDK；
- 不改变 Actor 调度、Lua API、网络语义或平台层所有权规则；
- 不引入新的包管理器或强制下载外部依赖。

## 已确认决策

### 嵌入式构建

- 使用 `CMAKE_CURRENT_SOURCE_DIR`、`CMAKE_CURRENT_BINARY_DIR` 和显式保存的 skyuv 根目录，
  不使用宿主工程的 `CMAKE_SOURCE_DIR` 推导 skyuv 文件路径；
- `PROJECT_IS_TOP_LEVEL` 只控制默认测试、CTest、CPack 和完整运行时安装，不控制核心库目标；
- 补丁只应用到构建目录中的 Skynet 副本，保留第三方工作树干净；
- 嵌入式构建不得覆盖宿主项目的通用缓存变量或无关安装规则。

### 安装与导出

- 导出名称使用 `skyuv::` 命名空间；
- 只导出具有明确消费边界的 skyuv 目标，内部构建目标保持非公共；
- 导出文件不得引用构建机源码目录；
- 安装包同时提供公共 skyuv 头文件、Lua 头文件、CMake package 配置和版本文件；
- 传递依赖必须随包提供或由 package 配置明确查找，不能依赖调用方手写 `.lib` 路径。

### 外部 Lua C 模块

- 示例优先验证 `skyuv::lua` 与 `skyuv::platform` 的最小模块；
- 示例必须覆盖 Debug/Release 中至少一种配置、模块输出命名和 `require` 加载；
- 示例明确说明 Lua ABI、allocator、Skynet 内部头文件和运行时目录的限制；
- 示例属于开发验证夹具，不自动安装到运行时发行包。

## 影响范围

- `CMakeLists.txt`、`src/CMakeLists.txt`、`3rd/CMakeLists.txt` 和 `cmake/`；
- `include/skyuv/` 及需要安装的 Lua 头文件；
- `examples/` 或独立的 `examples/cmake-consumer/`；
- `tests/` 下的嵌入式宿主、安装树和消费验证；
- `docs/README.md`、本计划和完成后的实施记录。

## 实施顺序

### 1. 修复子项目嵌入

- 审计所有 `CMAKE_SOURCE_DIR`、`PROJECT_SOURCE_DIR` 和输出目录假设；
- 将 skyuv 自有路径改为当前目录或明确的 skyuv 根变量；
- 将顶层专属测试、打包和安装行为加以隔离；
- 增加最小宿主工程，使用 `add_subdirectory` 构建一个 skyuv target；
- 在干净第三方工作树下验证补丁副本和重复配置。

### 2. 建立安装与导出

- 为公共目标补齐安装规则和 export set；
- 安装公共头文件、Lua 头文件和必要的运行时元数据；
- 生成 `skyuvConfig.cmake` 与版本文件；
- 验证全新构建目录中的外部项目可以 `find_package`、编译和链接；
- 验证静态/动态 libuv、Windows 多配置和 Unix 配置的依赖表达。

### 3. 增加外部 Lua C 模块示例

- 创建最小 C 模块和外部 CMake 工程；
- 使用安装包和子项目两种方式分别验证；
- 运行 Lua `require`，确认模块搜索路径、符号和运行时库一致；
- 记录不属于公共 ABI 的 Skynet 内部模块接入方式和限制。

## 测试与验收

- 顶层 CMake 配置、构建和现有相关 CTest 全部通过；
- 宿主工程从非 skyuv 源码目录执行 `add_subdirectory` 配置并成功构建；
- 安装树不包含构建机绝对源码路径；
- 外部工程仅通过 `find_package(skyuv CONFIG REQUIRED)` 和导出 target 完成编译链接；
- Lua C 模块在至少 Linux 和 Windows 之一完成真实 `require` 加载，另一平台完成配置/构建；
- `3rd/skynet` 和 `3rd/libuv` 工作树保持干净；
- 重复配置、不同构建目录和 Debug/Release 不产生路径串用或目标冲突。

## 风险与回退

- 若某些 Skynet 模块依赖内部符号，先将其降级为内部目标，不扩大公共 SDK；
- 若静态 libuv 的传递依赖无法稳定导出，优先提供共享 libuv 或显式 package 依赖，并记录平台限制；
- 若安装包与子项目的目标边界不同，保留两套明确入口，不用隐式变量兼容；
- 所有导出接口均可通过关闭安装导出选项回退，不影响仓库内部构建。

## 未决问题

- `skyuv::lua` 是否作为完整 Lua runtime 公共目标，还是仅作为 skyuv 内部/受限开发目标；
- 外部模块是否需要稳定的 Skynet 兼容头文件集合；
- Windows 静态 CRT 下外部 Lua C 模块的 allocator 和运行时库分发边界；
- 安装包是否需要同时提供可执行 Skynet runtime target。

## 当前进度

- 第一阶段已完成最小宿主工程夹具，使用外部源码目录通过 `add_subdirectory` 配置 skyuv；
- 已验证 `skyuv::platform` 和 `skyuv::lua` 在嵌入场景创建成功；
- 已验证配置阶段不会因宿主工程的源码目录不同而找不到 skyuv 的构建文件；
- 第二阶段已导出 `skyuv::platform`、`skyuv::lua` 和 `skyuv::libuv`，并安装公共 skyuv/Lua 头文件；
- 已生成 `skyuvConfig.cmake`、版本文件和可重定位的 target export；
- 第三阶段已加入最小 Lua C 模块示例，并验证源码嵌入和安装 package 两种方式均可构建；
- 示例已增加 Lua 宿主 smoke test：通过 `luaL_requiref` 注册模块并调用导出的函数；
- 源码嵌入和安装 package 两种方式的构建及 smoke test 均已在 Windows 本地通过；
- Linux CI 和 macOS CI 已在 `ee82490` 上通过，包含嵌入式 CMake 测试和第三方源码状态检查；
- 动态模块搜索路径下由独立 Lua 解释器执行的真实 `require`，以及完整跨平台矩阵仍待补充自动验收。

## 完成摘要

基础实现已完成；完成独立 Lua 解释器动态 `require` 验证并补齐记录后，再将本计划标记为已完成。
详细构建结果和平台差异记录在 `docs/records/`，本计划只保留最终状态和遗留限制。
