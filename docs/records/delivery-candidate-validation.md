# 阶段 6 候选发行包验收记录

## 结论

2026-08-16 的 `0.1.0` 候选发行包已通过 Windows x64、Linux x64 和 macOS arm64 的构建、
安装、归档及解压后运行验证。阶段 6 的工程实现和候选 artifact 验收已经完成。

当前不创建正式 GitHub Release。阶段 5 的固定 Linux Runner 双时段权威性能基线尚未采集，
Windows 全新环境中的 MSVC/UCRT 前置条件也待独立确认，因此阶段 6 保持“进行中”。

## 验收对象

- skyuv 提交：`657717ba267071b1c66b14fd4c2ef8dfa25e5c81`；
- 工作流：`Release`，仅手动触发；
- 工作流运行：[`31949273605`](https://github.com/synchronized/skyuv/actions/runs/31949273605)；
- 发布开关：`publish_release=false`，正式发布任务按预期跳过；
- Windows：`windows-2022`，静态 libuv；
- Linux：`ubuntu-24.04`，随包共享 libuv、jemalloc；
- macOS：`macos-14` arm64，随包共享 libuv、系统分配器。

后续提交 `2e0e759` 只增加运行时发行包使用与迁移指南，不改变候选二进制内容。

## 验收内容

三平台均完成：

1. 配置 Release 构建并构建主程序、4 个 C 服务和 7 个 Lua C 模块；
2. 安装 `Runtime` 组件到包含空格的临时目录；
3. 检查安装树不含测试程序、构建元数据和本机绝对路径；
4. 从安装树运行最小节点并正常退出；
5. 生成 ZIP 或 TGZ 及对应 SHA-256；
6. 在干净的含空格目录解压归档；
7. 从解压目录运行最小节点、TCP echo 和正常退出测试；
8. 上传仅包含归档和校验文件的三平台 artifact。

候选运行期间曾发现 Unix 主程序与动态模块分别静态嵌入 libuv 会在关闭阶段中止。最终发行
策略调整为 Windows 静态 libuv、Linux/macOS 随包共享 libuv，修复后验收通过。

## 发行文件

| 平台 | 文件 | SHA-256 |
| --- | --- | --- |
| Windows x64 | `skyuv-0.1.0-windows-amd64.zip` | `694a433375109931d41e4c2d233d7e74d83e27a1c4b09564d96c1e0e87b4b34b` |
| Linux x64 | `skyuv-0.1.0-linux-x86-64.tar.gz` | `09e0529f5217b29df6d8473f7d8863e1ded754c815151ad160bcd9da1c579d3a` |
| macOS arm64 | `skyuv-0.1.0-darwin-arm64.tar.gz` | `76f625f367f17ef0ab742ac83944964af2b89ccc39e683bddd1e504f848b9ea5` |

下载三个 artifact 后已在本地独立重新计算 SHA-256，结果与随包文件一致。候选 artifact 保留期
为 14 天，本表用于保存当次验收证据，不作为长期下载入口。

## 正式收口条件

正式创建 `v0.1.0` Release 前还需：

- 在带 `skyuv-benchmark` 标签的固定 Linux Runner 上，于两个独立时段采集阶段 5 权威基线；
- 记录噪声、性能差异和首批回归阈值，完成阶段 5 验收；
- 在全新 Windows 环境验证 MSVC/UCRT 加载条件，并决定声明前置条件还是随包分发运行库；
- 以拟发布提交再次运行 `Release` 工作流，使用 `publish_release=true` 和标签 `v0.1.0`。

前三项满足前不得把候选 artifact 描述为正式发布版本。
