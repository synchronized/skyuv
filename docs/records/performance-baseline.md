# 性能基线采集记录

## 当前状态

采集设施已就绪，权威首轮基线待固定 Linux 自托管 Runner 部署后执行。

## 权威性规则

- `self-hosted-fixed` 使用标签 `self-hosted`、`linux`、`x64`、`skyuv-benchmark`，其结果可作为基线候选；
- `github-hosted` 仅用于验证构建、配对、结果结构和 artifact 上传，不用于性能结论；
- 正式基线必须使用 Release、相同 jemalloc 策略、相同 Clang 版本与相同负载参数；
- 至少在两个独立时段采集，确认噪声范围后才记录性能差异或设置回归阈值。

## 采集内容

手动工作流“Linux 性能基线”依次采集 Actor ping-pong、TCP 长连接、TCP 短连接、TCP 背压和
UDP request/reply。artifact 同时包含 CPU、内存、频率策略、工具链、提交与子模块版本信息，
每个场景保存 skyuv、原版 Skynet、服务端日志和配对清单。

## 首轮结果

尚未采集。固定 Runner 未部署前，共享 Runner 的缩短版验证不得填写到本节。
