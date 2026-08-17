# Windows 静态 CRT 所有权审计

## 结论

当前不能把 Windows 发行目标直接从动态 CRT（`/MD`）切换为静态 CRT（`/MT`）。

Windows 使用 system allocator 时，上游 `skynet_malloc.h` 将 `skynet_malloc`、`skynet_realloc` 和
`skynet_free` 定义为 `malloc`、`realloc` 和 `free` 宏。使用 `/MT` 后，主程序与每个动态模块会
分别调用自己的 CRT 堆；现有代码包含动态模块分配消息、Skynet 主程序接管并释放的路径，会形成
跨 CRT 堆释放。

在引入进程级统一分配接口并迁移跨模块所有权路径前，Windows 发行版应继续使用 `/MD`。

## 实施进度

跨平台基础接口和首批语义测试已经实现：Windows 后端使用 process heap，Linux 根据构建配置
使用 jemalloc 或 system allocator，macOS 使用 system allocator。测试覆盖零尺寸、`calloc`
溢出、`realloc` 内容保持、对齐参数与统一 `skyuv_free()`。

当前尚未把 `skynet_*alloc` 和跨模块消息迁移到该接口，也没有切换 `/MT`。因此本记录的风险
结论仍然成立。

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

## 跨平台统一接口决策

进程级分配接口应在 Windows、Linux 和 macOS 使用相同签名与所有权语义，不设计 Windows 专用
公共接口。建议的内部接口包括：

```c
void *skyuv_malloc(size_t size);
void *skyuv_calloc(size_t count, size_t size);
void *skyuv_realloc(void *ptr, size_t size);
void skyuv_free(void *ptr);
void *skyuv_aligned_alloc(size_t alignment, size_t size);
int skyuv_posix_memalign(void **result, size_t alignment, size_t size);
```

底层后端保持平台可选：Windows 使用 process heap，Linux 默认使用主程序中的 jemalloc 并保留
system 回退，macOS 使用系统 allocator。统一的是 API、失败语义和跨模块所有权，不强制使用同一
第三方分配器。

接口先作为构建期内部边界维护，不随首版运行时包安装。Skynet 兼容层可以在底层返回 `NULL` 时
保留上游 OOM 终止策略，而底层 skyuv API 不直接决定调用方错误处理。

## 推荐改造

建议先建立真正的进程级 skyuv 分配接口，再启用 `/MT`：

1. 定义跨平台内部 `skyuv_malloc`、`skyuv_calloc`、`skyuv_realloc`、`skyuv_free` 和对齐
   分配语义，并为三个平台提供进程唯一实现。
2. Windows 底层使用 process heap，避免接口语义依赖某个 DLL 的 CRT；Linux 和 macOS 由当前
   allocator 配置选择后端。
3. 在 skyuv 维护的 Skynet 兼容头中，将 `skynet_*alloc` 映射到这些真实函数；不得修改
   `3rd/`，应通过补丁副本或兼容层完成。
4. 让 C 服务和 Lua C 模块调用进程唯一实现，不在各模块静态复制 allocator 实现。
5. 将所有跨模块所有权转移统一到该接口；模块私有临时对象可以继续使用本地 CRT，但应优先
   减少两套分配规则。
6. 增加“模块分配、核心释放”和“核心分配、模块释放”的专用测试，并覆盖失败路径。
7. 所有 Windows 发行目标统一设置 `MSVC_RUNTIME_LIBRARY` 为静态多线程 CRT，不能只修改 EXE。
8. 通过 PE 导入检查确认不再依赖动态 VCRUNTIME，并在 Windows Sandbox 中运行完整交付测试。

## 验收门槛

只有同时满足以下条件，才允许 Windows Release 使用 `/MT`：

- 所有跨模块缓冲区都有明确的创建者、释放者和统一分配接口；
- 源码扫描中不存在未经判定的跨边界 CRT 对象；
- Debug 和 Release 均通过跨模块分配测试、完整 CTest 和重复加载/关闭测试；
- Windows 候选 ZIP 在未安装 Visual C++ Redistributable 的全新环境中启动成功；
- PE 导入表和发行包内容证明没有遗漏的动态 CRT 依赖。

在这些条件完成前，保留 `/MD` 并声明 Visual C++ Redistributable 前置条件，比直接切换 `/MT`
更安全。

## 统一接口接入进度

截至 2026-08-17，推荐改造的第 1 至 5 项已完成第一轮接入：

- 补丁副本中的 `skynet_malloc.h` 已将普通、重分配和对齐分配入口映射到 `skyuv`；
- `skynet_lalloc` 已改由 `skyuv_realloc` 实现，Lua 状态不再回退到当前模块的 CRT；
- Skynet 核心、C 服务和依赖 Skynet 内存接口的 Lua C 模块均引用主程序导出的唯一实现；
- socket 适配层的收发缓冲区、事件与命令对象已使用同一接口，消除了网络边界上 CRT 与
  process heap 混合释放的问题；
- Windows 的 echo、gate、C gate、harbor、cluster、关闭流程及 socket 压力测试已覆盖这些
  跨边界路径。

本轮接入没有启用 `/MT`。后续仍需完成专用跨模块双向所有权测试、Release 重复加载测试和 PE
导入审计，达到验收门槛后再切换所有 Windows 发行目标的 CRT 模式。

专用跨模块测试已于 2026-08-17 补充到 `platform.dynamic`：测试模块从主程序导入唯一的
`skyuv` 分配符号，并连续执行 64 轮加载与卸载。每轮同时验证模块分配后由已卸载模块之外的
核心释放，以及核心分配后由模块重分配和释放。该测试覆盖 Debug 构建；Windows Release 重复
加载和 PE 导入审计仍是切换 `/MT` 前的未完成门槛。
