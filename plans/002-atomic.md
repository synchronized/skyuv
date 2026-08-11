# 002：原子操作抽象

## 状态

进行中。

## 背景

Skynet 当前原子接口依赖 GCC `__sync_*` 分支及不完整的 MSVC 条件实现，已经阻塞 Lua 和核心代码在 MSVC 下编译。消息队列、引用计数和 socket ID 都依赖其并发语义，因此需要独立设计、测试和迁移。

## 目标

- 提供整数与指针原子的统一接口；
- 支持 load、store、exchange、compare-exchange、fetch-add 和 fetch-sub；
- 明确 relaxed、acquire、release 与 acq_rel 内存序；
- 为 GCC、Clang/Apple Clang 和 MSVC 提供稳定后端；
- 在不改变 Skynet 并发语义的前提下迁移现有调用。

## 不在范围内

- 不使用 `volatile` 替代同步；
- 不依赖 MSVC C 模式下不完整的 `<stdatomic.h>`；
- 不在缺少测量的情况下改变消息队列算法；
- 不在本计划中重写 socket ID 生命周期。

## 设计决策

- API 位于 `include/skyuv/atomic.h`，编译器差异限制在 `src/platform/`；
- GCC 和 Clang 优先使用 `__atomic_*`；
- MSVC 使用 `_Interlocked*` intrinsic；
- compare-exchange 明确返回成功状态，并由调用方管理期望值；
- 每个操作在接口处声明内存序，不提供语义含糊的默认宏；
- 先逐项对照上游 `atomic.h` 的实际用途，再确定最弱但正确的内存序。

## 实施步骤

1. 盘点上游所有原子调用、共享状态和同步关系。
2. 定义整数、无符号整数和指针原子类型及操作。
3. 实现 GCC/Clang 后端和单线程语义测试。
4. 实现 MSVC 后端并通过严格警告构建。
5. 增加多生产者计数、CAS 竞争、发布/获取和指针交换压力测试。
6. 迁移 Lua 所依赖的原子接口，使定制 Lua 在 Windows 构建。
7. 迁移引用计数、消息队列和 socket ID 相关调用。
8. 在支持的平台运行 ThreadSanitizer，并回归阶段 0 行为测试。

## 测试方案

- 每个操作的返回值和状态转换；
- CAS 成功、失败和竞争路径；
- release/acquire 发布对象可见性；
- 多线程 fetch-add/fetch-sub 最终计数；
- 指针交换和空指针；
- 消息队列重复压力测试；
- Windows MSVC、clang-cl，Linux GCC/Clang 和 macOS Apple Clang。

## 验收标准

- 三个平台所有后端编译并通过测试；
- 原子接口没有平台类型泄漏；
- Skynet 目标范围内不再直接使用 `__sync_*`、`__atomic_*` 或 `_Interlocked*`；
- Lua 可在 MSVC 下构建并通过初始化测试；
- Linux 行为基线与迁移前一致；
- TSan 不报告已知数据竞争。

## 风险与回退

- 内存序判断不明确时先使用更强语义，待并发测试和审查后再收紧；
- MSVC intrinsic 的宽度和返回语义通过静态断言与单元测试固定；
- 消息队列迁移按独立提交进行，发现回归时可单独回退。

## 未决问题

是否把原子操作实现为纯头文件将在接口原型验证后确定；选择标准是类型安全、调试体验和跨编译器一致性，而不是减少一个源文件。

## 完成记录

已开始：

- 完成上游原子类型与操作盘点；
- 建立显式内存序的 32 位整数、机器字和指针基础接口；
- 增加基本语义、CAS 失败更新、多线程计数及 release/acquire 发布测试。
