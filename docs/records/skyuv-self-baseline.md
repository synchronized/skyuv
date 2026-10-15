# skyuv 固定 VMware 自身参考基线

## 状态与范围

2026-10-15 完成同提交两个窗口的 skyuv 单实现采集。两份原始 artifact 均上传成功，
环境、机器身份、工具链、提交与参数一致性检查通过。可以作为当前固定虚拟机的自身参考基线；
网络逐轮波动仍明显，暂不启用自动合并硬门槛。该结果不包含原版 Skynet 性能结论，
原版波动定位经维护者确认已转为可选研究，不阻塞发布；skyuv 自身稳定性和回归验收仍按
[计划 006](../plans/006-performance-stability.md)的已验收策略持续执行。

## 环境与采集

- 提交：`a09db80`；Runner：`skyuv-vmware-linux`；环境策略：`approved-vmware`。
- Windows 宿主机上的固定 VMware，Arch Linux x86_64、4 个逻辑 CPU、约 16 GiB 内存；
  Clang 23.1.1、Release、jemalloc。按维护者确认不补录宿主调频或电源计划。
- 保持正式负载：2 worker、echo 64 字节、预热 2 秒、每场景 5 轮，每轮 10 秒；
  背压保留原有初始暂停、读取延迟及有界写入。无 CPU 绑定或单 worker 调参。
- 配对编排入口使用 `--skyuv-only`，不构建或运行原版；清单 `paired=false`，仅有 skyuv 条目。
- 进程及子进程地址空间限制为 2 GiB，采集期间不在虚拟机并行构建。
- 第一成功窗口：11:33:24–11:39:03 UTC；第二窗口：11:41:42–11:47:17 UTC。
  作业间空闲 2 分 39 秒；两个同日窗口不能代表跨日或长期分布。

首次尝试的测量和报告生成成功，但上传时 `ECONNRESET`；已先备份本机数据再重试。
本记录的正式表格采用重试后上传成功的窗口，不用上传失败的尝试代替第二份 artifact。

## 两窗口结果

吞吐为各窗口 5 轮的中位数，背压单位为 B/s，其余为次/秒。极差比是逐轮
`(最大值 − 最小值) / 中位数`，不能解释为置信区间。

| 场景 | 第一窗口吞吐 | 第二窗口吞吐 | 漂移 | 首窗极差比 | 次窗极差比 |
| --- | ---: | ---: | ---: | ---: | ---: |
| Actor | 203147.47 | 203239.09 | +0.05% | 0.58% | 0.76% |
| TCP 背压 | 56536.29 | 56414.35 | -0.22% | 0.44% | 0.82% |
| TCP 长连接 | 13972.77 | 13555.14 | -2.99% | 11.25% | 28.19% |
| TCP 短连接 | 6147.27 | 5838.73 | -5.02% | 30.34% | 18.12% |
| UDP | 14476.79 | 13891.94 | -4.04% | 22.68% | 14.51% |

| 场景 | p99 漂移 | 服务端 CPU 时间漂移 | 服务端峰值 RSS 漂移 |
| --- | ---: | ---: | ---: |
| Actor | +0.44% | +0.25% | +0.54% |
| TCP 背压 | — | +9.09% | -6.18% |
| TCP 长连接 | +1.26% | +2.08% | -8.07% |
| TCP 短连接 | +14.46% | +1.44% | -0.21% |
| UDP | +6.02% | -1.69% | +9.66% |

Actor 和背压吞吐在两窗口及窗口内部均较稳定；网络场景两窗口中位吞吐漂移不超过 5.02%，
但逐轮极差比最高约 30.34%，不能称为所有指标都低噪声。TCP 短连接汇总 p99 增加约 14.46%，
仍需观察。UDP 两窗口丢包率均为零，所有场景完成，无 OOM；背压吞吐受受控慢接收负载约束，
不是服务端最大带宽结论。客户端平均 CPU 均低于报告的 0.90 核瓶颈提示线。

## 首批人工复测参考线

以下是根据这批有限观测设立的保守调查触发线，不是统计置信界、发布承诺或自动拒绝合并规则。
只适用于相同固定虚拟机、工具链和负载参数；更换环境后重新采集，不套用这些数值。

| 场景 | 中位吞吐下降 | 汇总 p99 增加 |
| --- | ---: | ---: |
| Actor | 超过 5% | 超过 15% |
| TCP 背压 | 超过 5% | 无此指标 |
| TCP 长连接、短连接、UDP | 超过 15% | 超过 40% |

服务端峰值 RSS 增加超过 30% 触发复测；CPU 使用监控时长归一化后的平均占用核数，增加超过
`max(参考核数 × 30%, 0.05 核)` 触发复测，避免背压等低 CPU 场景的计数粒度被百分比放大。
这些幅度高于本次对应指标的已观察漂移，仍需更多独立时段检验其误报率。

吞吐参考取两窗口中位数的较小值；p99、RSS、平均 CPU 参考取较大值。后续候选至少预热并采
5 轮，检查逐轮分布；触线先在空闲环境重复采集。两个独立窗口均触线才升级为性能问题调查，
一次异常也要保留数据，不通过丢弃异常轮次获得通过结果。错误、丢包、数据损坏和 OOM
仍按正确性要求处理，不依赖性能参考线。

`baseline_noise.py` 继续只比较同提交窗口以量化噪声。跨版本工具 `baseline_regression.py`
已实现上述策略的自动复测提示：候选提交可变化，其他已记录环境与负载参数须一致；同一指标
在两个候选窗口都触线才提示调查。它不自动拒绝合并，后续硬门槛须另行评估长期误报率。
使用方法和输入限制见[Runner 指南](../guides/performance-runner.md#跨版本自身回归检查)。

## 证据与复现

- [第一成功窗口](https://github.com/synchronized/skyuv/actions/runs/38727257251)（第 2 次尝试），
  artifact：`performance-baseline-self-hosted-fixed-38727257251`。
- [第二窗口](https://github.com/synchronized/skyuv/actions/runs/38729138051)，
  artifact：`performance-baseline-self-hosted-fixed-38729138051`。
- 两份 artifact 保留 90 天，包含预检、环境、每轮 JSON、日志、资源清单和自身报告。
- [Linux 完整矩阵](https://github.com/synchronized/skyuv/actions/runs/38727256798)、
  [macOS Debug/Release](https://github.com/synchronized/skyuv/actions/runs/38727257737)、
  [三平台发行演练](https://github.com/synchronized/skyuv/actions/runs/38727257234) 全部通过，
  未创建 tag 或 Release。
- 本地 CMake 配置、严格构建及八个相关测试通过；新增单实现测试覆盖 Actor/TCP/UDP、
  原版参数缺失诊断、`paired=false` 和结果集合；原有配对协议测试继续通过。

下载解压两份 artifact 到 `first/`、`second/` 后：

```sh
python3 benchmarks/baseline_noise.py --first first --second second \
  --output-json comparison.json --output-markdown comparison.md
```

详细原版噪声与后续诊断见[配对性能记录](performance-baseline.md)。

## 跨版本工具本地验证

2026-10-15：新增 `benchmark.baseline_regression`，验证保守参考值、各指标触线规则、边界相等、
低 CPU 绝对余量、同指标重复触线升级、不同指标不误升级、环境/提交/参数变化及不完整或无效数据拒绝。
有效触线报告退出 0，输入拒绝退出 2；JSON 明确 `blocking=false`，不将提示伪装为合并硬门槛。

使用本记录已保存的五场景数据，把第二参考窗口作为候选进行自检，结果为 `within_reference`。
复制数据并将 TCP 长连接吞吐注入降低 30%、标为合成候选提交后，结果为 `retest`；原始数据
未修改。这用于验证工具读取和触线行为，不是新增的跨版本性能测量。
本地 CMake 配置、严格构建及三个基准报告/比较测试通过。提交 `bcb5027` 的
[Linux 完整矩阵](https://github.com/synchronized/skyuv/actions/runs/38750441100)、
[macOS Debug/Release](https://github.com/synchronized/skyuv/actions/runs/38750441010) 和
[三平台发行演练](https://github.com/synchronized/skyuv/actions/runs/38750441248) 全部成功。
计划 006 按维护者确认的非阻断复测提示范围完成；长期采样、误报率评估和将来可能的硬门槛
继续作为后续改进，不表示已经证明长期分布或启用自动拒绝合并。
