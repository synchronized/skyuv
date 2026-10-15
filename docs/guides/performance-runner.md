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
