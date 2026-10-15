# 实施记录

`docs/records/` 保存已经发生的实施、迁移、测试和测量结果。记录描述当时的提交、环境、验证
结论和已知差异；当前行为和版本范围分别以对应的 Guide、Reference 和版本文档为准。

## 交付与运行时

- [`windows-v0.2.0-actions-acceptance.md`](windows-v0.2.0-actions-acceptance.md)：Windows 新路径、真实只读 ACL、独立 Lua 动态加载与三平台发行演练。
- [`build-project-layout.md`](build-project-layout.md)：三平台构建运行目录统一。

- [`linux-v0.2.0-closure.md`](linux-v0.2.0-closure.md)：v0.2.0 Linux 功能和交付收口与性能基线外部条件。

- [`2026-10-14-v0.2.0-beta.4-release-acceptance.md`](2026-10-14-v0.2.0-beta.4-release-acceptance.md)：beta.4 三平台发布与公开资产复核。
- [`2026-10-12-v0.2.0-beta.3-release-acceptance.md`](2026-10-12-v0.2.0-beta.3-release-acceptance.md)：beta.3 三平台发布与公开资产复核。
- [`client-libuv-beta.3.md`](client-libuv-beta.3.md)：beta.3 客户端迁移的三平台开发验收与生命周期结论。
- [`windows-build-diagnostics.md`](windows-build-diagnostics.md)：Windows 诊断治理、缺陷复现与 beta.4 本地构建矩阵。
- [`2026-10-12-v0.2.0-beta.1-release-acceptance.md`](2026-10-12-v0.2.0-beta.1-release-acceptance.md)：`v0.2.0-beta.1` 三平台预览版发布验收。
- [`2026-09-29-v0.1.1-release-acceptance.md`](2026-09-29-v0.1.1-release-acceptance.md)：`v0.1.1` 正式发布验收。
- [`2026-09-29-v0.1.0-release-acceptance.md`](2026-09-29-v0.1.0-release-acceptance.md)：`v0.1.0` 正式发布验收。
- [`delivery-candidate-validation.md`](delivery-candidate-validation.md)：三平台候选发行包验收。
- [`delivery-runtime-closure-audit.md`](delivery-runtime-closure-audit.md)：发行运行时闭包和平台依赖审计。
- [`windows-static-crt-ownership-audit.md`](windows-static-crt-ownership-audit.md)：Windows 静态 CRT 所有权审计。

## Lua、性能与稳定性

- [`lua-runtime-stage-4.md`](lua-runtime-stage-4.md)：Lua 模块接入和跨平台验证。
- [`performance-baseline.md`](performance-baseline.md)：固定 Linux 性能基线状态。
- [`stability-soak-stage-5.md`](stability-soak-stage-5.md)：三平台稳定性 Soak 验证。
