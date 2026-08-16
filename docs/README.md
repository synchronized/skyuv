# skyuv 文档中心

这里是 skyuv 项目文档的统一入口。项目概览和快速构建方式见根目录
[`README.md`](../README.md)，文档职责与维护规则见
[`DOCUMENTATION_GUIDE.md`](../DOCUMENTATION_GUIDE.md)。

## 推荐阅读顺序

1. [`ROADMAP.md`](ROADMAP.md)：了解总体目标、架构边界和当前阶段。
2. [`DEPENDENCIES.md`](DEPENDENCIES.md)：了解第三方依赖的选择、用途和引入条件。
3. [`plans/README.md`](plans/README.md)：查看当前阶段的实施计划和验收项。
4. [`LUA_RUNTIME_MATRIX.md`](LUA_RUNTIME_MATRIX.md)：查看 Lua C 模块与运行环境的真实接入状态。

## 路线图与计划

- [`ROADMAP.md`](ROADMAP.md)：全局阶段、依赖关系和状态摘要。
- [`plans/README.md`](plans/README.md)：计划索引与状态定义。
- [`plans/005-lua-runtime.md`](plans/005-lua-runtime.md)：已完成的 Lua 模块与运行环境计划及验收摘要。
- [`plans/006-performance-stability.md`](plans/006-performance-stability.md)：性能基准、压力测试和长时稳定性计划。
- [`plans/007-cross-platform-delivery.md`](plans/007-cross-platform-delivery.md)：安装、打包和发布的跨平台交付计划草案。
- [`plans/variable-buffer-audit.md`](plans/variable-buffer-audit.md)：Skynet 可变缓冲区专项审计。

## 参考资料

- [`DEPENDENCIES.md`](DEPENDENCIES.md)：依赖库策略、平台边界与启用条件。
- [`LUA_RUNTIME_MATRIX.md`](LUA_RUNTIME_MATRIX.md)：Lua 模块接入状态、风险与验证等级。

## 操作指南

- [`guides/runtime-distribution.md`](guides/runtime-distribution.md)：校验、解压和启动三平台运行时
  发行包，并迁移已有 Skynet 服务。
- [`guides/performance-runner.md`](guides/performance-runner.md)：部署并预检固定 Linux 性能 Runner。

## 实施记录

- [`records/lua-runtime-stage-4.md`](records/lua-runtime-stage-4.md)：阶段 4 的模块接入、
  跨平台验证过程和已知差异。
- [`records/performance-baseline.md`](records/performance-baseline.md)：固定 Linux 环境的性能基线
  权威性规则、采集内容和首轮结果状态。
- [`records/stability-soak-stage-5.md`](records/stability-soak-stage-5.md)：阶段 5 三平台正式一小时
  Soak 的方法、结果和基础设施修复记录。
- [`records/delivery-runtime-closure-audit.md`](records/delivery-runtime-closure-audit.md)：阶段 6 的
  最小启动闭包、发行运行时边界和平台依赖审计。

## 尚待建立的分类

以下分类按需渐进建立，不为填充目录而创建空文档：

- Tutorial：从零完成可运行目标的连续教程。
- Guide：构建、运行、调试和平台操作指南。
- Concept：Actor、网络线程、句柄所有权等架构说明。
- ADR：长期有效的架构与兼容性决策。

新增文档时应先确认权威来源，并将入口加入本页。
