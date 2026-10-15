# 性能基线采集记录

## 当前状态

固定 VMware 的五场景原版配对已完成采集，原版网络指标存在明显波动，相关测量与诊断结果
保留在下文。2026-10-15 经维护者确认，继续定位原版波动转为可选研究，不作为发布阻塞项；
本记录不据不稳定比值给出加速结论。

skyuv 自身双窗口参考基线及人工复测参考线见[自身基线](skyuv-self-baseline.md)。
当前验收范围以[计划 006](../plans/006-performance-stability.md)及
[v0.2.0 门槛](../versions/v0.2.0.md)为准；下文阶段结论描述当时状态，不覆盖后续范围调整。

## 权威性规则

- `self-hosted-fixed` 使用标签 `self-hosted`、`linux`、`x64`、`skyuv-benchmark`，其结果可作为基线候选；
- 当前固定 VMware 经维护者于 2026-10-15 批准，可显式使用 `approved-vmware` 策略；
  调频信息缺失作为环境限制记录，结论仅适用于该虚拟机。配置见[Runner 指南](../guides/performance-runner.md)。
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

2026-10-15 已完成当前固定 VMware 的两轮候选采集，详见下一节。采集成功表示结果完整，
不表示网络性能已稳定，也不表示 `v0.2.0` 性能发布门槛已通过。

## 2026-10-15 固定 VMware 双时段验收

维护者确认使用当前 Windows 宿主机上的 VMware 虚拟机验收。本次采用显式
`approved-vmware` 策略，保留调频信息缺失备注，不记录宿主机电源计划。结论仅适用于此虚拟机。

### 环境与复现

- 提交：`9fd822ce911f7157a20188899842eecc9d1b3dd8`；Runner：`skyuv-vmware-linux`。
- Arch Linux x86_64，Linux 7.2.9，4 个逻辑 CPU，约 15.57 GiB 内存；宿主 CPU 为 i5-13600KF。
- Clang 23.1.1，Release，jemalloc；各实现预热 2 秒，测量 5 轮，每轮 10 秒，TCP/UDP echo 为 64 字节。
- 配对采集进程及子进程各受 2 GiB 地址空间上限约束；构建并行数为 2。
- 第一窗口：02:32:54–02:44:09 UTC；第二窗口：02:46:46–02:57:22 UTC。
  两轮作业间隔 2 分 37 秒，第二轮重新构建后采样；这是同日分开的两个窗口，不代表跨日或长期分布。
- 两轮机器身份、CPU、内存、提交、子模块、工具链、策略和负载参数一致性检查通过。

首次 Runner 启动曾在检出阶段失败：服务缺少终端代理配置，且 checkout 复制用户相对符号链接
`.gitconfig` 后得到失效链接。已配置服务代理及独立 HOME，重试成功；失败尝试没有性能样本。
Runner 以用户 systemd 服务运行；服务状态、磁盘和网络配置应由后续维护持续检查。

### 观测漂移

下表为第二窗口相对第一窗口的变化。CPU 为服务端总时间，RSS 为服务端峰值。

| 场景 | 实现 | 吞吐 | p99 | CPU | RSS |
| --- | --- | ---: | ---: | ---: | ---: |
| Actor | skyuv | +0.35% | -1.03% | -0.12% | +9.44% |
| Actor | upstream | +0.27% | -2.42% | -0.19% | +2.94% |
| TCP 背压 | skyuv | -0.55% | — | +7.69% | +7.22% |
| TCP 背压 | upstream | -0.58% | — | +0.00% | -0.44% |
| TCP 长连接 | skyuv | +4.55% | -5.93% | +5.41% | +0.78% |
| TCP 长连接 | upstream | +14.88% | -2.67% | +12.64% | +0.60% |
| TCP 短连接 | skyuv | +2.89% | +3.17% | -1.20% | -0.10% |
| TCP 短连接 | upstream | -7.42% | +1.44% | -12.78% | +5.34% |
| UDP | skyuv | +4.52% | -8.50% | +5.68% | +9.56% |
| UDP | upstream | +550.40% | +10.40% | +72.49% | -0.30% |

Actor 与背压吞吐漂移均小于 1%；TCP 长连接 skyuv 两轮吞吐均低于原版，分别约低 16.41%
和 23.92%，需要作为当前观测差异保留。短连接与 UDP 不可据中位数比值宣称加速：
原版短连接首轮单轮吞吐为约 326–5510 次/秒，原版 UDP 两轮中位吞吐为约 1787 和
11621 次/秒，后者漂移约 550%。UDP 两轮丢包率均为零，不能用丢包解释吞吐波动。

背压 skyuv 服务端峰值 RSS 两轮约为 27.71/29.71 MiB，原版约为 44.72/44.52 MiB；
两轮均无此前不受控增长导致的 OOM。低 CPU 场景的相对变化会放大小额绝对差异。

### 当时的验收结论与后续门槛

固定环境部署、例外预检、完整采集和可重复比较已验证。性能稳定性尚未通过，
不启用自动回归硬门槛，也不将本次结果当作任意 Windows 宿主机或物理 Linux 的保证。
不能据这些数据给全部场景设置可信阈值；计划 006 保持进行中。

后续先复现原版 UDP/短连接吞吐塌陷，联合观察虚拟机调度与被测进程等待；目前没有足够证据
确定来自宿主机、虚拟机调度还是原版事件循环。若调整采集方法，需用新提交重新采两轮，
不与本次直接混合。基线稳定后再按各核心场景确定允许回归范围。

### 验证与原始证据

- [第一窗口](https://github.com/synchronized/skyuv/actions/runs/38671220715)（第 3 次尝试成功），
  artifact：`performance-baseline-self-hosted-fixed-38671220715`。
- [第二窗口](https://github.com/synchronized/skyuv/actions/runs/38672777755)，
  artifact：`performance-baseline-self-hosted-fixed-38672777755`。
- artifact 包含预检、环境快照、每轮 JSON、服务端日志、CPU/RSS 配对清单与报告，保留 90 天。
- 同提交 [Linux 完整 CI](https://github.com/synchronized/skyuv/actions/runs/38671216497) 与
  [macOS Debug/Release CI](https://github.com/synchronized/skyuv/actions/runs/38671216082) 均成功。
- 本机 CMake 配置、严格构建及三个相关基准设施 CTest 全部通过；本轮未执行 Windows 本机验收。

下载并解压两份 artifact 到 `first/`、`second/` 后可复现比较：

```sh
python3 benchmarks/baseline_noise.py --first first --second second \
  --output-json comparison.json --output-markdown comparison.md
```

## 2026-10-15 噪声诊断与逐轮报告补强

在同一虚拟机使用原版 Skynet Release/jemalloc 做额外短时诊断，每组预热 1 秒、3 轮、每轮
5 秒、64 字节，UDP 与短连接各测三组；不作为正式配对基线。对照默认 2 worker、将服务端
固定 CPU 0/客户端固定 CPU 1，以及保持默认亲和性但改为 1 worker。每次重新启动服务端，
端口为 25585，配置只在本地构建副本生成，第三方源码未修改；不与原基线混合。

| 场景 | 诊断变量 | 三轮吞吐（次/秒） | 三轮 p99（毫秒） |
| --- | --- | --- | --- |
| UDP | 默认 2 worker | 12915 / 17700 / 13125 | 0.192 / 0.133 / 0.170 |
| UDP | 双端分别固定 CPU，2 worker | 14078 / 16384 / 15786 | 0.165 / 0.165 / 0.165 |
| UDP | 1 worker | 12443 / 13898 / 11528 | 0.190 / 0.073 / 0.151 |
| 短连接 | 默认 2 worker | 2932 / 4274 / 5237 | 0.403 / 0.382 / 0.348 |
| 短连接 | 双端分别固定 CPU，2 worker | 831 / 699 / 649 | 59.998 / 63.260 / 67.914 |
| 短连接 | 1 worker | 6107 / 7457 / 6995 | 0.360 / 0.300 / 0.316 |

固定 CPU 并未一致改善两类场景，短连接反而出现明显长尾，因此没有把亲和性设置加入正式
工作流。单 worker 的这组短连接较好，但只改变了负载条件，不能据此认定修复噪声；UDP
本次也没有复现原先所有低吞吐轮次。各组最大请求延迟仍约 70 毫秒，个别短连接约 95 毫秒。

诊断期间每 50 毫秒读取服务端各线程的 `/proc/<pid>/task/*/{schedstat,wchan}`，此观测本身
可能扰动时序；内核 `sched_schedstats=0`，不能依赖这些读数准确归因调度等待。
检查到用户服务共同祖先 cgroup 的 CPU 配额为无限，限流计数为零，Runner 服务报告
`CPUQuotaPerSecUSec=infinity`；这只排除了已观察到的该类限流证据，不排除 Windows/VMware
宿主调度影响。未修改宿主机电源策略、内核参数或 CPU 配额。

可按以下方式复现短时负载（使用已构建的原版副本和对应配置，分别运行服务端和客户端）：

```sh
# 配置采用 examples/upstream-benchmark-udp-echo.conf；短连接改用 upstream-benchmark-tcp-echo.conf。
# 固定亲和性诊断时分别在服务端与客户端命令前加 taskset -c 0、taskset -c 1。
SKYUV_BENCHMARK_PORT=25585 SKYUV_BENCHMARK_MESSAGE_SIZE=64 ./skynet diagnostic.conf
python3 benchmarks/udp_request_reply.py --implementation upstream --build-type Release \
  --allocator jemalloc --compiler 'clang 23.1.1' --port 25585 \
  --message-size 64 --warmup 1 --duration 5 --iterations 3 --output diagnostic.json
# 短连接客户端改用 benchmarks/tcp_short_connection.py；单 worker 修改副本配置 thread = 1。
```

原始诊断 JSON、日志与线程快照保存在本机忽略目录 `build/noise-diagnosis/`，不是 Actions
artifact，也未提交生成目录。诊断服务/客户端继承 2 GiB 地址空间限制。

本轮同时修复比较报告的观测盲区：保留两个时段各自逐轮吞吐和 p99 的轮数、最小值、最大值、
中位数及极差比，不再仅展示两时段总体统计。历史原版短连接首轮吞吐极差比为 176.03%，
UDP 首轮为 705.01%；短连接首轮逐轮 p99 最大 56.762 毫秒，汇总 p99 却只有约 0.459 毫秒，
说明必须联合检查逐轮尾延迟。新测试覆盖中位数不变但单轮骤降/长尾、缺少旧逐轮数据、零值
以及非法非有限指标。该补强不引入性能硬阈值。

报告补强提交 `46e81c1` 已通过本地 CMake 配置、严格构建和 `benchmark.baseline_noise`，
以及 [Linux 完整矩阵](https://github.com/synchronized/skyuv/actions/runs/38725518477) 与
[macOS Debug/Release](https://github.com/synchronized/skyuv/actions/runs/38725518576)。
本轮未重新执行 Windows；未修改平台运行时代码。

结论仍为噪声未收敛；下一步需要在保持正式 2 worker 参数的条件下重复观测，并补足可以归因
等待时间的调度证据，不能用本次单 worker 或亲和性结果替换正式基线。

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
