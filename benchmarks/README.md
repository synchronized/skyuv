# 基准工具

基准与功能测试分开：这里的程序采集数据，不在共享 CI Runner 上设置绝对性能门槛。
结果格式由 `result.schema.json` 定义，通用参数和环境元数据由 `protocol.py` 提供。

## 长时稳定性编排

`soak.py` 在指定时长内重复执行 `--` 后的命令，每轮保存独立日志，并持续原子更新
`soak-summary.json`。手动工作流“长时稳定性 Soak”可选择 Linux、Windows 或 macOS，默认运行
1 小时的 `stability` CTest 子集；即使某轮失败，已生成日志和汇总仍会作为 artifact 上传并保留 30 天。

手动工作流“Linux 性能基线”统一运行五类原版/skyuv 配对，并保存主机与工具链元数据。只有
带 `skyuv-benchmark` 标签的固定自托管 Runner 结果可作为权威基线；GitHub Hosted 模式仅验证流程。
配对清单还记录双方服务进程的用户态、内核态、总 CPU 时间和峰值 RSS；该资源采样当前只在
Linux 通过 `/proc` 提供，Windows 和 macOS 会明确写入 `available: false`。
TCP 与 UDP 清单还分别记录共同 Python 客户端的 CPU 时间和峰值 RSS，用于识别客户端先达到
瓶颈而污染服务端对照的情况；Actor 负载运行在被测 Skynet 进程内，不另设客户端指标。
工作流最后使用 `baseline_report.py` 从环境、配对清单和原始结果生成 `report.md`，报告展示双方
吞吐、延迟、CPU、峰值 RSS 及相对差异；JSON 始终是权威数据源，不手工维护报告数据。
固定机完成两个独立时段采集后，使用 `baseline_noise.py` 比较两个 artifact 目录；工具会先检查
Runner、CPU、工具链、频率策略、提交和场景参数一致，再输出结构化 JSON 与 Markdown 漂移报告。
固定 Runner 在采集前由 `runner_preflight.py` 检查平台、架构、工具、内存、磁盘、工作树和调频
策略；部署步骤与故障处理见 [`固定 Linux 性能 Runner 配置指南`](../docs/guides/performance-runner.md)。

## TCP 长连接 echo

先启动 skyuv 或原版 Skynet 的固定长度 echo 服务，再运行同一个客户端：

```powershell
python benchmarks/tcp_echo.py `
  --implementation skyuv `
  --build-type Release `
  --allocator system `
  --compiler "MSVC 19.x" `
  --message-size 64 `
  --output build/benchmarks/tcp-echo.json
```

默认连接 `127.0.0.1:25281`，预热 2 秒并测量 5 轮，每轮 10 秒。每次往返都会验证
响应内容；结果保存每轮吞吐和延迟 p50、p95、p99、最大值，并汇总中位吞吐。

`compare_tcp.py` 会依次启动 skyuv 与原版 Skynet 服务端，以相同参数调用上述客户端，保存两份
原始结果、两份服务端日志及 `tcp-echo-comparison.json` 配对清单。服务端日志用于诊断启动、
连接和关闭异常；共享 CI 的短时数据只验证配对流程，不作为正式性能结论。

## TCP 短连接

短连接基准复用相同的固定长度 echo 服务，但每次操作都会重新建立连接、完成一次 echo 并关闭：

```powershell
python benchmarks/tcp_short_connection.py `
  --implementation skyuv `
  --build-type Release `
  --allocator system `
  --compiler "MSVC 19.x" `
  --message-size 64 `
  --output build/benchmarks/tcp-short-connection.json
```

该场景用于观察 connect、accept、close 和 socket ID 回收的综合成本。正式对照时必须与长连接
基准使用相同主机、编译模式、分配器、消息尺寸和测量轮数。

使用 `compare_tcp.py --scenario tcp_short_connection` 可复用长连接场景的服务端启动、就绪等待、
日志保存与清理流程，生成 `tcp-short-connection-comparison.json` 及双方原始结果。

## TCP 慢接收端与背压

背压服务持续写入固定内容，客户端先暂停读取，再按指定间隔小块读取：

```powershell
python benchmarks/tcp_backpressure.py `
  --implementation skyuv `
  --build-type Release `
  --allocator system `
  --compiler "MSVC 19.x" `
  --initial-pause 0.2 `
  --read-delay 0.005 `
  --read-size 4096 `
  --output build/benchmarks/tcp-backpressure.json
```

结果记录每轮读取次数、接收字节数和接收吞吐。服务端日志中的
`SKYUV_BACKPRESSURE_WARNING` 表明 Skynet 写队列已跨过 warning 阈值；正式运行需同时保存服务端日志。
使用 `compare_tcp.py --scenario tcp_backpressure` 可依次运行双方服务端；传入
`--require-log-marker SKYUV_BACKPRESSURE_WARNING` 后，任一实现未触发 warning 都会使本轮失败。

## UDP request/reply

UDP 基准使用顺序 request/reply 建立可重复的基础数据：

```powershell
python benchmarks/udp_request_reply.py `
  --implementation skyuv `
  --build-type Release `
  --allocator system `
  --compiler "MSVC 19.x" `
  --message-size 64 `
  --output build/benchmarks/udp-request-reply.json
```

每个响应都会校验来源地址和完整内容。结果记录发送、接收、超时丢包、包速率以及成功响应的
p50、p95、p99 和最大延迟；首期固定单个在途请求，后续再用独立场景扩展并发窗口。

`compare_udp.py` 通过双方共同输出的就绪标记判断 UDP 服务可用，再以相同数据报尺寸、响应超时
和测量参数依次运行客户端，保存原始结果、服务端日志及 `udp-request-reply-comparison.json`。

## Actor ping-pong

Actor 基准由 Python 启动器运行两个独立 Skynet Lua 服务，测量跨服务 `call/return` 往返：

```powershell
python benchmarks/actor_ping_pong.py `
  --implementation skyuv `
  --build-type Release `
  --allocator system `
  --compiler "MSVC 19.x" `
  --executable build/windows-vs2022-release/src/Release/skyuv_skynet_portable.exe `
  --config build/windows-vs2022-release/3rd/skyuv-benchmark-actor.conf `
  --output build/benchmarks/actor-ping-pong.json
```

Lua 服务使用纳秒级 `skynet.hpc()` 记录每次往返延迟；Python 汇总每轮吞吐，以及各轮延迟
分位数的中位数。测试原版 Skynet 时只需替换可执行文件和等价配置，启动器与 Lua 服务保持不变。

`compare_actor.py` 接收两套可执行文件和配置，以完全相同参数依次运行二者，并生成两个原始结果
和 `actor-ping-pong-comparison.json` 配对清单。共享 CI 仅验证协议可运行，输出不作为性能结论。

## Actor 多生产者

多生产者基准默认启动 4 个生产者 Actor，向单个消费者 Actor 异步发送消息：

```powershell
python benchmarks/actor_multi_producer.py `
  --implementation skyuv `
  --build-type Release `
  --allocator system `
  --compiler "MSVC 19.x" `
  --producers 4 `
  --executable build/windows-vs2022-release/src/Release/skyuv_skynet_portable.exe `
  --config build/windows-vs2022-release/3rd/skyuv-benchmark-actor-multi.conf `
  --output build/benchmarks/actor-multi-producer.json
```

每个生产者发送完成后通过同源 barrier 确认此前消息均已消费。结果校验发送/接收总数一致，
并记录各生产者消息数的最小值、最大值和公平性比例 `min/max`。

## Actor 环形传递

环形基准默认让令牌依次经过 8 个 Actor。完整一圈计为一次延迟样本，每次跨 Actor 转发计为
一个操作；使用 `actor_ping_pong.py` 并指定 `--actors 8`、环形配置和
`SKYUV_ACTOR_RING_SAMPLE` 标记即可运行。结果记录 hop 吞吐和完整环路的延迟分位数。

## 定时器集中触发

定时器基准在每轮注册指定数量的同截止时间 `skynet.timeout`，默认正式参数为 10,000 个。
`--duration` 表示注册到截止时间的秒数，`--timer-count` 表示每轮数量。结果校验所有回调均触发，
并记录从计划截止时间到实际回调执行时刻的 p50、p95、p99 和最大偏差。
