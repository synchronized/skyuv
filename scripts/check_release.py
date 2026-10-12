#!/usr/bin/env python3
"""检查 skyuv 版本文件与 Git 发布前置条件，不修改仓库状态。"""

from __future__ import annotations

import argparse
import re
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent


def run_git(*args: str) -> str:
	result = subprocess.run(
		["git", *args], cwd=ROOT, check=True, capture_output=True, text=True
	)
	return result.stdout.strip()


def release_version() -> str:
	cmake = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
	version_match = re.search(r"^project\(skyuv VERSION ([0-9]+\.[0-9]+\.[0-9]+)", cmake, re.M)
	prerelease_match = re.search(r'^set\(SKYUV_VERSION_PRERELEASE "([^"]*)"\)', cmake, re.M)
	if not version_match or not prerelease_match:
		raise ValueError("无法从 CMakeLists.txt 读取项目版本或预发布标识")
	version = version_match.group(1)
	prerelease = prerelease_match.group(1)
	if prerelease and not re.fullmatch(r"[0-9A-Za-z-]+(?:\.[0-9A-Za-z-]+)*", prerelease):
		raise ValueError(f"预发布标识格式无效：{prerelease}")
	return f"{version}-{prerelease}" if prerelease else version


def check() -> list[str]:
	errors: list[str] = []
	try:
		version = release_version()
	except (OSError, ValueError) as error:
		return [str(error)]
	tag = f"v{version}"
	version_doc = ROOT / "docs" / "versions" / f"v{version}.md"
	if not version_doc.is_file():
		errors.append(f"缺少版本范围文档：{version_doc.relative_to(ROOT)}")
	else:
		content = version_doc.read_text(encoding="utf-8")
		if not re.search(rf"^# {re.escape(tag)}(?:[:： ]|$)", content, re.M):
			errors.append(f"版本文档标题与 {tag} 不一致")

	changelog = (ROOT / "CHANGELOG.md").read_text(encoding="utf-8")
	if not re.search(rf"^## {re.escape(version)}(?:\s|$)", changelog, re.M):
		errors.append(f"CHANGELOG.md 缺少 `## {version}` 章节")
	version_index = (ROOT / "docs" / "versions" / "README.md").read_text(encoding="utf-8")
	if f"](v{version}.md)" not in version_index:
		errors.append("docs/versions/README.md 未链接当前版本文档")
	if version_doc.exists() and f"versions/v{version}.md" not in (ROOT / "docs" / "README.md").read_text(encoding="utf-8"):
		errors.append("docs/README.md 未登记当前版本文档")

	try:
		status = run_git("status", "--porcelain")
		branch = run_git("branch", "--show-current")
		if status:
			errors.append("工作区不干净；请先提交或清理变更")
		if branch != "main":
			errors.append(f"当前分支为 {branch or '<detached>'}，发布准备应从 main 执行")
		local = run_git("tag", "--list", tag)
		if local:
			errors.append(f"本地已存在标签 {tag}；版本号不可复用")
		remote = run_git("ls-remote", "--tags", "origin", f"refs/tags/{tag}", f"refs/tags/{tag}^{{}}")
		if remote:
			errors.append(f"origin 已存在标签 {tag}；版本号不可复用")
		origin_main = run_git("rev-parse", "origin/main")
		head = run_git("rev-parse", "HEAD")
		if head != origin_main:
			errors.append("HEAD 与 origin/main 不一致；请先同步并合入发布准备提交")
	except (subprocess.CalledProcessError, FileNotFoundError) as error:
		detail = getattr(error, "stderr", "") or str(error)
		errors.append(f"Git 前置检查失败（需要 origin/main 和可用的 origin）：{detail.strip()}")
	return errors


def main() -> int:
	parser = argparse.ArgumentParser(description=__doc__)
	parser.parse_args()
	version = release_version()
	errors = check()
	if errors:
		print(f"发布前检查未通过：v{version}", file=sys.stderr)
		for error in errors:
			print(f"- {error}", file=sys.stderr)
		return 1
	print(f"发布前静态检查通过：v{version}（未执行构建、打标签或发布）")
	return 0


if __name__ == "__main__":
	try:
		raise SystemExit(main())
	except (OSError, ValueError, subprocess.CalledProcessError) as error:
		print(f"发布前检查无法完成：{error}", file=sys.stderr)
		raise SystemExit(2)
