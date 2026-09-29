# CMake 外部 Lua C 模块示例

这个示例演示外部工程如何通过安装后的 skyuv CMake package 编译一个最小 Lua C 模块。
模块只使用 `skyuv::lua` 和 `skyuv::platform`，不依赖 Skynet 内部头文件。

先构建并安装 skyuv 的 Development 组件，再从本目录配置：

```shell
cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/skyuv-install
cmake --build build
```

示例还会构建 `skyuv_consumer_smoke`，创建 Lua 状态并通过
`luaL_requiref` 注册模块，检查模块导出的 `version` 函数；启用 CTest
时可运行 `ctest --test-dir build` 验证。

Windows 的外部工程必须选择与 skyuv 一致的 MSVC CRT。模块输出为无前缀的
`skyuv_consumer` 动态库；将其放入 Lua 的模块搜索路径后，可通过
`require("skyuv_consumer")` 加载。

如果需要验证源码嵌入方式，可以额外传入：

```shell
cmake -S . -B build -DSKYUV_SOURCE_DIR=/path/to/skyuv
```

该方式用于验证 `add_subdirectory`，实际分发优先使用安装 package。
