#!/usr/bin/env python3
"""生成发布验收记录草稿，不覆盖已有文件。"""

from __future__ import annotations

import argparse
import re
import sys
from datetime import date
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent


def main() -> int:
	parser = argparse.ArgumentParser(description=__doc__)
	parser.add_argument("--tag", required=True, help="发行标签，例如 v0.2.0-beta.2")
	parser.add_argument("--date", default=date.today().isoformat(), help="发布日期，默认本地今天")
	args = parser.parse_args()
	if not re.fullmatch(r"v[0-9]+\.[0-9]+\.[0-9]+(?:-[0-9A-Za-z.-]+)?", args.tag):
		parser.error("tag 必须使用 vX.Y.Z 或 vX.Y.Z-prerelease 格式")
	try:
		date.fromisoformat(args.date)
	except ValueError:
		parser.error("date 必须使用 YYYY-MM-DD 格式")
	version = args.tag[1:]
	output = ROOT / "docs" / "records" / f"{args.date}-{version}-release-acceptance.md"
	if output.exists():
		print(f"拒绝覆盖已有记录：{output.relative_to(ROOT)}", file=sys.stderr)
		return 1
	output.parent.mkdir(parents=True, exist_ok=True)
	kind = "GitHub Pre-release" if "-" in version else "GitHub Release"
	content = f"""# {args.date} skyuv {args.tag} 发布验收

## 结论

待填写：记录发布状态和三平台最终验收结论。

## 发布身份

- 版本：`{args.tag}`
- 提交：待填写
- 标签：[`{args.tag}`](https://github.com/synchronized/skyuv/releases/tag/{args.tag})
- Release workflow：待填写 Actions run 链接
- 发布类型：{kind}

## 验证结果

| 平台 | 构建 | 交付测试 | 归档与 SHA-256 |
|---|---|---|---|
| Windows x64 | 待填写 | 待填写 | 待填写 |
| Linux x64 | 待填写 | 待填写 | 待填写 |
| macOS | 待填写 | 待填写 | 待填写 |

## 本版本内容

- 待根据实际发布范围填写（以已发布版本文档和 Changelog 为准）。

## 已知限制与后续工作

- 待填写已知限制、未完成事项及后续版本安排。
	"""
	output.write_text(content, encoding="utf-8", newline="\n")
	print(f"已生成草稿：{output.relative_to(ROOT)}")
	print("发布后请人工核实内容，并将记录加入 docs/records/README.md、docs/README.md。")
	return 0


if __name__ == "__main__":
	raise SystemExit(main())
