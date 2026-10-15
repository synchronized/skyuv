# 固定 Linux 性能 Runner 配置指南

## 适用场景

本指南用于部署可产生 skyuv 权威性能基线的 GitHub Actions 自托管 Runner。共享 Hosted Runner
只验证采集流程，不能替代固定机器。

## 前置条件

- x86_64 Linux 专用主机或长期固定配置的虚拟机；
- 至少 8 GiB 内存和 10 GiB 工作区可用磁盘；
- 安装 `clang`、`cmake`、`ninja`、`make`、`git`、`python3` 和 `prlimit`；
- 各逻辑 CPU 使用相同调频策略，推荐固定为 `performance`；
- 测量期间避免同时运行构建、备份和其他高负载任务。

## 配置步骤

1. 按 GitHub 仓库的自托管 Runner 指引安装 Linux x64 Runner。
2. 除默认标签外，为 Runner 增加 `skyuv-benchmark` 标签。
3. 将 Runner 作为专用服务运行，记录主机、操作系统和工具链变更。
4. 在仓库根目录执行预检：

   ```sh
   python3 benchmarks/runner_preflight.py \
     --repository . \
     --output build/performance-preflight.json
   ```

5. 预检退出码为 0 且 JSON 中 `ready` 为 `true` 后，手动运行“Linux 性能基线”，选择
   `self-hosted-fixed`。默认采集原版配对；验证 skyuv 自身回归基线时选择 `skyuv_only=true`，
   此时不构建或运行原版，报告明确不含原版性能结论。
6. 使用完全相同的提交与参数，在另一个独立时段再次采集。
7. 解压两次 artifact，使用 `benchmarks/baseline_noise.py` 生成漂移比较结果。

## 验证结果

工作流会在构建前再次执行预检。失败时不会产生权威基线，但仍会上传
`preflight.json`，其中包含错误、警告及主机快照。

固定机结果还必须满足 [`performance-baseline.md`](../records/performance-baseline.md) 的权威性
规则；Runner 标签和单次绿色运行本身不足以证明结果可用。

## 常见问题

### 调频策略不是 performance

统一但非 `performance` 的策略只产生警告，因为部分虚拟化环境不暴露完整的频率控制。
两次采集必须保持相同策略；能够控制物理机时仍建议使用 `performance`。

### 无法读取调频策略

默认 `standard` 策略会失败。2026-10-15 经维护者确认，当前 Windows 宿主机上的固定 VMware
虚拟机采用 `approved-vmware` 策略验收，允许缺少 cpufreq，不记录宿主机调频或电源计划。
本地预检增加 `--environment-policy approved-vmware`，工作流也必须显式选择同名策略。
该策略仍检查 VMware 标识、机器身份、内存、磁盘、工具链和干净工作树，不适用于其他虚拟机。

两次采集必须保持机器身份、CPU 数、内存、工具链、提交和参数一致；结果只代表此固定虚拟机。
工作流通过 `prlimit` 为每个配对采集进程及其子进程设置 2 GiB 地址空间上限，防止背压负载耗尽
虚拟机内存；构建过程不受该上限限制。仍需两个独立时段评估噪声，不能以批准环境代替测量。

### 系统负载警告

等待其他任务结束后重新运行。预检只检查开始时负载，正式采集期间仍需保证机器专用。

## 跨版本自身回归检查

`baseline_noise.py` 保持同提交噪声比较语义。新增 `baseline_regression.py` 用于候选提交与两份
参考基线的跨版本检查；参考值和触线规则以[自身参考基线](../records/skyuv-self-baseline.md#首批人工复测参考线)
为准。它不运行基准，只读取已保存的完整五场景数据。

```sh
python3 benchmarks/baseline_regression.py \
  --reference-first /path/to/reference-first \
  --reference-second /path/to/reference-second \
  --candidate /path/to/candidate \
  --output-json build/regression/report.json \
  --output-markdown build/regression/report.md
```

参考窗口必须来自同一提交；候选允许不同提交，其他已记录的固定 Runner、工具链、子模块、
内存、环境策略和负载参数必须一致。每场景需要预热、至少五轮完整成功样本及服务端 CPU/RSS。
元数据缺失、脏工作树、Hosted Runner、参数不一致、缺失场景、非法指标和 UDP 丢包均拒绝比较。
工具核对清单和原始结果中的参数；未完整写入元数据的 worker 数、夹具语义仍需人工确认等价，
不能仅凭两个目录可比就断言任意代码改动的测量公平。

输出状态：

- `within_reference`：已检查的指标未触线，不证明长期稳定或正式版本已验收。
- `retest`：至少一个候选窗口触线，保留数据并在空闲环境复测。
- `investigate`：通过 `--candidate-repeat /path/to/repeat` 提供的另一候选窗口，在同一指标上也触线。
  两候选窗口须为同一候选提交且时间不同；两个窗口触发不同指标时仍仅提示复测。

有效报告无论是否触线均退出 0，且 JSON 标记 `blocking=false`；输入或数据校验失败退出 2。
这实现自动复测提示，不实现自动拒绝合并。JSON 记录参考/候选提交、窗口、参考策略版本、
已核对环境、参数、每指标参考值/参考线/候选值及逐轮波动；所有原始 artifact 仍须保留。
