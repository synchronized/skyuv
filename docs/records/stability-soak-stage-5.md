# 阶段 5 三平台长时稳定性记录

## 结论

提交 `059ce689d1b814e818ae30ac325831591094870c` 已在 Windows、Linux 和 macOS 分别完成一次
不少于 3600 秒的正式 Soak。三次运行共完成 957 轮 `stability` CTest，失败轮次为 0，逐轮日志
无缺失。阶段 5 的三平台长时稳定性验收已完成。

## 统一方法

- 通过手动工作流“长时稳定性 Soak”运行；
- 每个平台使用 Release Preset 构建；
- 每轮运行带 `stability` 标签的 9 个 CTest；
- 请求时长均为 3600 秒；
- 每轮保存独立日志，持续原子更新 `soak-summary.json`；
- 最终验收同时检查工作流结论、累计时长、轮次退出码和日志文件完整性。

## 结果

| 平台 | GitHub Actions 运行 | 实际时长 | 完成轮数 | 失败轮数 | 缺失日志 |
| --- | --- | ---: | ---: | ---: | ---: |
| Windows 2022 x64 | [31894154179](https://github.com/synchronized/skyuv/actions/runs/31894154179) | 3602.88 秒 | 307 | 0 | 0 |
| Ubuntu 24.04 x64 | [31913581575](https://github.com/synchronized/skyuv/actions/runs/31913581575) | 3600.70 秒 | 354 | 0 | 0 |
| macOS 15 arm64 | [31913582944](https://github.com/synchronized/skyuv/actions/runs/31913582944) | 3606.95 秒 | 296 | 0 | 0 |

三份 artifact 分别包含 308、355 和 297 个文件，即一份汇总加每轮一份日志。

## 运行中发现并修复的问题

Windows 首次正式运行暴露了非交互管道编码问题：基准成功后输出中文进度时，Hosted Runner 的
cp1252 编码触发 `UnicodeEncodeError`。第一次修复统一了 Soak 子进程链的 Python UTF-8 模式；
第二次修复统一了编排器自身的 stdout/stderr 编码，并增加以 cp1252 启动编排器的回归测试。

两次失败均发生在编排基础设施而非 skyuv 生命周期逻辑。最终 Windows 正式运行使用完整修复后的
`059ce68`，307 轮全部通过。

## 未覆盖范围

- Hosted Runner 结果证明三平台长时正确性，不作为 Linux 权威性能结论；
- 本记录不设置吞吐、延迟、CPU 或内存回归阈值；
- 固定 Linux 主机的两个独立时段性能基线仍待采集。
