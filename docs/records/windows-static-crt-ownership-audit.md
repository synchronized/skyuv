# Windows 静态 CRT 所有权审计

## 结论

当前不能把 Windows 发行目标直接从动态 CRT（`/MD`）切换为静态 CRT（`/MT`）。

Windows 使用 system allocator 时，上游 `skynet_malloc.h` 将 `skynet_malloc`、`skynet_realloc` 和
`skynet_free` 定义为 `malloc`、`realloc` 和 `free` 宏。使用 `/MT` 后，主程序与每个动态模块会
分别调用自己的 CRT 堆；现有代码包含动态模块分配消息、Skynet 主程序接管并释放的路径，会形成
跨 CRT 堆释放。

在引入进程级统一分配接口并迁移跨模块所有权路径前，Windows 发行版应继续使用 `/MD`。

## 审计范围

本记录描述 2026-08-17 对以下 Windows 运行时目标的静态审计：

- `skyuv_skynet_portable.exe`；
- `cservice/` 下的 `snlua`、`logger`、`harbor` 和 `gate`；
- `luaclib/` 下的 `skynet`、`client/socket`、`bson`、`sproto/core`、`lpeg`、`md5/core` 和
  `skyuv/control`；
- Lua allocator、动态加载和线程启动兼容代码。

审计关注 `malloc/calloc/realloc/free`、`skynet_*alloc`、Lua userdata、`FILE *`、CRT 文件描述符
以及带 `PTYPE_TAG_DONTCOPY` 的消息所有权转移。

## 决定性风险

### system allocator 不是统一导出函数

当前 Windows 通过 `NOUSE_JEMALLOC` 使用系统分配器。`skynet_malloc.h` 中的接口实际展开为 CRT
函数宏，而不是调用主程序中唯一的分配器：

```c
#define skynet_malloc malloc
#define skynet_realloc realloc
#define skynet_free free
```

`/MD` 下，各目标最终使用同一个进程级动态 CRT，因此现有候选包能够正常工作。`/MT` 下，每个
PE 模块拥有独立 CRT 状态，上述宏会把相同源码映射到不同堆。

### 已确认的跨模块释放路径

`service_gate.c` 在 `gate.dll` 中通过 `skynet_malloc()` 分配 `temp` 或 `tmp`，随后使用
`PTYPE_TAG_DONTCOPY` 传给 `skynet_send()`。消息进入主程序的队列，最终由
`skynet_server.c` 中的 `skynet_free(msg->data)` 释放。

这条路径在 `/MT` 下等价于：

```text
gate.dll 的 malloc -> skyuv_skynet_portable.exe 的 free
```

`snlua`、`harbor` 和 `luaclib/skynet` 也存在 `PTYPE_TAG_DONTCOPY` 或 lightuserdata 所有权
转移。即使逐个修复 `gate`，也不能证明其余模块安全；必须统一跨模块分配协议。

## 当前可保留的模块内用法

以下模式创建和销毁均发生在同一动态模块内，本身不阻止 `/MT`：

- `logger.dll` 内部配对使用 `fopen()` 和 `fclose()`；
- `client/socket.dll` 的 stdin 队列在同一模块中 `malloc()` 和 `free()`；
- `bson.dll` 的临时编码缓冲区在同一模块中分配、扩容和销毁；
- `sproto/core.dll` 的内存池由同一模块创建和释放；
- `luaclib/skynet.dll` 内的 debug channel 和 sharedata 私有对象由同一模块回收；
- 线程启动包装对象的创建与线程入口释放位于同一目标；
- 动态加载兼容对象的创建和失败回收位于同一目标。

这些结论依赖“资源不离开所属模块”。后续接口变化仍需重新审计。

Lua userdata 由 `lua_State` 保存的 allocator 回调管理。调用代码可以位于不同模块，但实际释放
仍通过该状态的 allocator 回调执行。该机制不是本次发现的首要阻塞项，但切换 CRT 后必须纳入
动态加载、GC 和服务关闭测试。

## 推荐改造

建议先建立真正的进程级 skyuv 分配接口，再启用 `/MT`：

1. 在 Windows 主程序中实现并导出 `skyuv_malloc`、`skyuv_calloc`、`skyuv_realloc` 和
   `skyuv_free`；底层优先使用 Windows process heap，避免接口语义依赖某个 DLL 的 CRT。
2. 在 skyuv 维护的 Skynet 兼容头中，将 `skynet_*alloc` 映射到这些真实函数；不得修改
   `3rd/`，应通过补丁副本或兼容层完成。
3. 让 C 服务和 Lua C 模块通过主程序导入库调用唯一实现，不在各模块静态复制实现。
4. 将所有跨模块所有权转移统一到该接口；模块私有临时对象可以继续使用本地 CRT，但应优先
   减少两套分配规则。
5. 增加“模块分配、核心释放”和“核心分配、模块释放”的专用测试，并覆盖失败路径。
6. 所有 Windows 发行目标统一设置 `MSVC_RUNTIME_LIBRARY` 为静态多线程 CRT，不能只修改 EXE。
7. 通过 PE 导入检查确认不再依赖动态 VCRUNTIME，并在 Windows Sandbox 中运行完整交付测试。

## 验收门槛

只有同时满足以下条件，才允许 Windows Release 使用 `/MT`：

- 所有跨模块缓冲区都有明确的创建者、释放者和统一分配接口；
- 源码扫描中不存在未经判定的跨边界 CRT 对象；
- Debug 和 Release 均通过跨模块分配测试、完整 CTest 和重复加载/关闭测试；
- Windows 候选 ZIP 在未安装 Visual C++ Redistributable 的全新环境中启动成功；
- PE 导入表和发行包内容证明没有遗漏的动态 CRT 依赖。

在这些条件完成前，保留 `/MD` 并声明 Visual C++ Redistributable 前置条件，比直接切换 `/MT`
更安全。
