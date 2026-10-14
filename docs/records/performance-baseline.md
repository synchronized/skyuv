# 性能基线采集记录

## 当前状态

高优先级未完成。采集设施已就绪，权威首轮基线待固定 Linux 自托管 Runner 部署后执行。该事项
作为 `v0.2.0` 的高优先级收口任务跟踪，不阻塞当前 `v0.1.0` 的运行时发布或不依赖权威性能
数据的开发任务。

## 权威性规则

- `self-hosted-fixed` 使用标签 `self-hosted`、`linux`、`x64`、`skyuv-benchmark`，其结果可作为基线候选；
- `github-hosted` 仅用于验证构建、配对、结果结构和 artifact 上传，不用于性能结论；
- 正式基线必须使用 Release、相同 jemalloc 策略、相同 Clang 版本与相同负载参数；
- 至少在两个独立时段采集，确认噪声范围后才记录性能差异或设置回归阈值。

## 采集内容

手动工作流“Linux 性能基线”依次采集 Actor ping-pong、TCP 长连接、TCP 短连接、TCP 背压和
UDP request/reply。artifact 同时包含 CPU、内存、频率策略、工具链、提交与子模块版本信息，
每个场景保存 skyuv、原版 Skynet、服务端日志和配对清单。

两个独立时段的 artifact 解压后，使用 `benchmarks/baseline_noise.py` 生成结构化比较结果和
Markdown 报告。只有环境、提交、工具链与参数一致的两次采集才允许进入噪声评估。

## 首轮结果

尚未采集。固定 Runner 未部署前，共享 Runner 的缩短版验证不得填写到本节。

## 2026-10-14 本机探索性对照与背压负载修复

本次在 Arch Linux x86_64、4 个逻辑 CPU、约 16 GiB 内存上，以 Clang 23.1.1、Release 和
jemalloc 运行配对；预热 2 秒，测量 5 轮，每轮 10 秒，TCP/UDP echo 消息为 64 字节。
CPU 调频策略不可读取，未部署固定 Runner，也未完成双时段采集，因此本次结果不是权威基线，
不改变上述首轮验收状态，不用于设置回归阈值。

环境补充：用户确认宿主机为 Windows，Linux 运行在 VMware 虚拟机中；虚拟机的
`/sys/devices/system/cpu/cpufreq/` 目录为空。按本轮采集约定，不再补录宿主机电源计划、
物理 CPU 调频策略或频率控制信息，仅保留此环境限制备注。现有自动环境快照保留原样，
缺失信息不推定为固定频率；本轮仍作为探索性对照，不调整正式基线验收规则。

| 场景 | skyuv 中位吞吐 | 原版中位吞吐 | skyuv 相对差异 |
| --- | ---: | ---: | ---: |
| Actor ping-pong | 203390.67 ops/s | 200686.66 ops/s | +1.35% |
| TCP 长连接 | 13386.49 ops/s | 15022.43 ops/s | -10.89% |
| TCP 短连接 | 5763.58 ops/s | 5502.64 ops/s | +4.74% |
| TCP 背压 | 150861.38 B/s | 150498.84 B/s | +0.24% |
| UDP request/reply | 13084.22 ops/s | 11299.80 ops/s | +15.79% |

前四类中的 Actor、TCP 长/短连接与 UDP 采集基于 `c11ecf7`。旧背压负载持续发送，仅输出
warning；主机时间 14:36:41，内核记录全机 OOM 并杀死 skyuv，匿名 RSS 为 15349696 KiB。
这说明旧负载自身可造成无限队列积压，不能据此判定运行时存在泄漏。

修复 `dd96161` 只改变共同测试负载、回归测试与说明，未改变运行时源码。双方背压服务以
4 MiB 队列阈值暂停生产，每批 1 MiB 后等待网络线程并读取快照；修复后重测背压，并为双方
采集进程设置相同的 2 GiB 虚拟内存上限。双方五轮均完成且触发 warning，峰值 RSS 分别为
29.21 MiB 与 47.43 MiB。背压吞吐受慢客户端读取节奏限制，不代表网络层最大吞吐。

原始 JSON、配对清单、日志、环境快照、OOM 证据及生成报告保存在采集机的
`build/performance-local/results/`，未提交生成数据。不同场景的源码快照以各自 JSON 为准。
原版使用仓库现有流程，在隔离副本中以其上游 Makefile 构建；skyuv 使用 CMake Release preset。
本次仍需固定环境复测，不能将表中差异解释为跨环境的性能承诺。

验证：CMake 配置及构建通过，CTest 66/66 通过；新增模拟慢接收端、恢复读取和断开的
20 次重复测试通过。修复提交的 [Linux 完整 CI](https://github.com/synchronized/skyuv/actions/runs/38525266906)
与 [macOS Debug/Release CI](https://github.com/synchronized/skyuv/actions/runs/38525270851)
全部通过；Windows 未在本机验证。
