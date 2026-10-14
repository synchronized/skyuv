# 三平台构建运行目录统一

## 变更范围

2026-10-14，顶层完整构建默认通过 `skyuv_stage_runtime` 组装
`build/<preset>/project/`。复用 Runtime 安装规则，目录包含 `bin/`、`cservice/`、
`luaclib/`、`lualib/`、`service/`、`examples/` 和 `licenses/`。原编译产物仍由 CMake 管理，
Linux/macOS 现有目标路径及上游基线不迁移，避免打断已有测试和基准命令。

Windows 保留用于 DLL 装载的程序相邻依赖安排，之后同样执行 Runtime 安装组装完整运行树。
独立 Lua 解释器启用时放入 `project/bin/` 作为构建工具；它和测试程序不因此加入 Runtime
发行包。CPack 继续按 Runtime 组件生成归档，不压缩整个 project 目录。

源码嵌入默认不启用组装；顶层可显式关闭 `SKYUV_STAGE_PROJECT`。完整构建或显式构建
`skyuv_stage_runtime` 会再次执行安装，重新部署缺失的生成文件。只构建单个源码目标不保证
部署同步，多配置目录按最后一次部署的配置运行。

## 验证

新增 `runtime.project` 检查完整目录、包外最小节点与 TCP echo、独立 Lua 加载客户端模块，
删除生成目录中的 logger 模块和示例配置后重新部署，再启动验证。该测试串行运行以避免
与归档 echo 测试争用端口；Windows 复用 DLL 预检夹具，缺失依赖时不启动程序。

本机 Linux Clang Release 配置、完整构建与 CTest 73/73 通过。Linux/macOS CI 最终结果在
提交后补充；Windows 本机验证不可用，不能以 CMake 配置与 Unix 结果宣告 Windows 验收完成。
