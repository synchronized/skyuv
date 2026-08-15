"""检查固定 Linux 性能 Runner 是否满足权威基线采集前提。"""

from __future__ import annotations

import argparse
import datetime
import json
import os
import platform
import shutil
import subprocess
from pathlib import Path
from typing import Any


REQUIRED_TOOLS = ("clang", "cmake", "ninja", "make", "git", "python3")
MINIMUM_MEMORY_BYTES = 8 * 1024**3
MINIMUM_DISK_BYTES = 10 * 1024**3


def command_version(command: str) -> str | None:
	path = shutil.which(command)
	if path is None:
		return None
	try:
		return subprocess.check_output(
			[path, "--version"], stderr=subprocess.STDOUT, text=True, timeout=10,
		).splitlines()[0]
	except (OSError, subprocess.CalledProcessError, subprocess.TimeoutExpired, IndexError):
		return "available-version-unknown"


def memory_bytes() -> int:
	try:
		pages = os.sysconf("SC_PHYS_PAGES")
		page_size = os.sysconf("SC_PAGE_SIZE")
		return int(pages * page_size)
	except (AttributeError, OSError, ValueError):
		return 0


def governors() -> list[str]:
	values = []
	for path in sorted(Path("/sys/devices/system/cpu").glob("cpu[0-9]*/cpufreq/scaling_governor")):
		try:
			values.append(path.read_text(encoding="utf-8").strip())
		except OSError:
			continue
	return values


def git_dirty(repository: Path) -> bool:
	try:
		return bool(subprocess.check_output(
			["git", "-C", str(repository), "status", "--porcelain"],
			stderr=subprocess.STDOUT, text=True, timeout=10,
		).strip())
	except (OSError, subprocess.CalledProcessError, subprocess.TimeoutExpired):
		return True


def collect_snapshot(repository: Path) -> dict[str, Any]:
	try:
		load_average = os.getloadavg()[0]
	except (AttributeError, OSError):
		load_average = None
	return {
		"operating_system": platform.system(),
		"architecture": platform.machine(),
		"cpu_count": os.cpu_count() or 0,
		"memory_bytes": memory_bytes(),
		"disk_free_bytes": shutil.disk_usage(repository).free,
		"load_average_1m": load_average,
		"governors": governors(),
		"tools": {tool: command_version(tool) for tool in REQUIRED_TOOLS},
		"git_dirty": git_dirty(repository),
	}


def evaluate(snapshot: dict[str, Any]) -> tuple[list[str], list[str]]:
	errors = []
	warnings = []
	if snapshot.get("operating_system") != "Linux":
		errors.append("固定性能基线只支持 Linux")
	if str(snapshot.get("architecture", "")).lower() not in ("x86_64", "amd64"):
		errors.append("固定性能基线当前只支持 x86_64")
	if snapshot.get("memory_bytes", 0) < MINIMUM_MEMORY_BYTES:
		errors.append("物理内存不得低于 8 GiB")
	if snapshot.get("disk_free_bytes", 0) < MINIMUM_DISK_BYTES:
		errors.append("工作区可用磁盘不得低于 10 GiB")
	missing_tools = [name for name, version in snapshot.get("tools", {}).items() if version is None]
	if missing_tools:
		errors.append(f"缺少工具：{', '.join(missing_tools)}")
	if snapshot.get("git_dirty"):
		errors.append("检出后的工作树不干净")
	governor_values = snapshot.get("governors", [])
	if not governor_values:
		errors.append("无法读取 CPU 调频策略")
	elif len(set(governor_values)) != 1:
		errors.append("各 CPU 核心的调频策略不一致")
	elif governor_values[0] != "performance":
		warnings.append(f"CPU 调频策略为 {governor_values[0]}，建议固定为 performance")
	cpu_count = snapshot.get("cpu_count", 0)
	load_average = snapshot.get("load_average_1m")
	if isinstance(load_average, (int, float)) and cpu_count and load_average / cpu_count > 0.5:
		warnings.append("一分钟平均负载超过逻辑 CPU 数的一半")
	return errors, warnings


def main() -> int:
	parser = argparse.ArgumentParser(description=__doc__)
	parser.add_argument("--repository", type=Path, default=Path.cwd(), help="skyuv 仓库目录")
	parser.add_argument("--output", type=Path, required=True, help="预检 JSON 输出路径")
	arguments = parser.parse_args()
	snapshot = collect_snapshot(arguments.repository.resolve())
	errors, warnings = evaluate(snapshot)
	result = {
		"schema_version": 1,
		"timestamp_utc": datetime.datetime.now(datetime.timezone.utc).isoformat(),
		"ready": not errors,
		"errors": errors,
		"warnings": warnings,
		"snapshot": snapshot,
	}
	arguments.output.parent.mkdir(parents=True, exist_ok=True)
	arguments.output.write_text(
		json.dumps(result, ensure_ascii=False, indent=2, sort_keys=True) + "\n",
		encoding="utf-8",
	)
	for warning in warnings:
		print(f"警告：{warning}")
	for error in errors:
		print(f"错误：{error}")
	return 0 if result["ready"] else 3


if __name__ == "__main__":
	raise SystemExit(main())
