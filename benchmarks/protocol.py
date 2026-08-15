"""skyuv 基准程序共享的参数、环境元数据和结果输出协议。"""

from __future__ import annotations

import argparse
import datetime
import enum
import json
import os
import platform
import subprocess
from pathlib import Path
from typing import Any


SCHEMA_VERSION = 1


class ExitCode(enum.IntEnum):
	"""基准程序使用的稳定退出码。"""

	SUCCESS = 0
	INVALID_ARGUMENT = 2
	SETUP_FAILED = 3
	EXECUTION_FAILED = 4
	VALIDATION_FAILED = 5


def positive_float(value: str) -> float:
	parsed = float(value)
	if parsed <= 0:
		raise argparse.ArgumentTypeError("必须大于 0")
	return parsed


def nonnegative_float(value: str) -> float:
	parsed = float(value)
	if parsed < 0:
		raise argparse.ArgumentTypeError("不得小于 0")
	return parsed


def positive_int(value: str) -> int:
	parsed = int(value)
	if parsed <= 0:
		raise argparse.ArgumentTypeError("必须大于 0")
	return parsed


def nonnegative_int(value: str) -> int:
	parsed = int(value)
	if parsed < 0:
		raise argparse.ArgumentTypeError("不得小于 0")
	return parsed


def add_common_arguments(parser: argparse.ArgumentParser) -> None:
	"""为具体基准增加统一参数。"""

	parser.add_argument("--duration", type=positive_float, default=10.0, help="每轮测量秒数")
	parser.add_argument("--warmup", type=nonnegative_float, default=2.0, help="预热秒数")
	parser.add_argument("--iterations", type=positive_int, default=5, help="测量轮数")
	parser.add_argument("--seed", type=nonnegative_int, default=1, help="可复现随机种子")
	parser.add_argument(
		"--implementation",
		choices=("upstream", "skyuv"),
		required=True,
		help="被测实现",
	)
	parser.add_argument("--build-type", required=True, help="构建类型，例如 Release")
	parser.add_argument("--allocator", required=True, help="分配器，例如 jemalloc 或 system")
	parser.add_argument("--compiler", required=True, help="编译器及版本摘要")
	parser.add_argument("--output", type=Path, required=True, help="JSON 结果路径")


def _git_value(repository: Path, *arguments: str) -> str:
	try:
		return subprocess.check_output(
			["git", "-C", str(repository), *arguments],
			stderr=subprocess.DEVNULL,
			text=True,
		).strip()
	except (OSError, subprocess.CalledProcessError):
		return "unknown"


def collect_environment(repository: Path) -> dict[str, Any]:
	"""采集可复现结果所需、且无需额外依赖的环境信息。"""

	status = _git_value(repository, "status", "--porcelain")
	return {
		"timestamp_utc": datetime.datetime.now(datetime.timezone.utc).isoformat(),
		"commit": _git_value(repository, "rev-parse", "HEAD"),
		"dirty": status not in ("", "unknown"),
		"operating_system": platform.system(),
		"os_release": platform.release(),
		"architecture": platform.machine(),
		"cpu": platform.processor() or "unknown",
		"cpu_count": os.cpu_count() or 0,
		"python": platform.python_version(),
	}


def create_result(
	benchmark_name: str,
	scenario: str,
	arguments: argparse.Namespace,
	environment: dict[str, Any],
) -> dict[str, Any]:
	"""创建尚未填充样本的标准结果对象。"""

	return {
		"schema_version": SCHEMA_VERSION,
		"benchmark": {"name": benchmark_name, "scenario": scenario},
		"implementation": arguments.implementation,
		"environment": {
			**environment,
			"compiler": arguments.compiler,
			"build_type": arguments.build_type,
			"allocator": arguments.allocator,
		},
		"parameters": {
			"duration_seconds": arguments.duration,
			"warmup_seconds": arguments.warmup,
			"iterations": arguments.iterations,
			"seed": arguments.seed,
		},
		"samples": [],
		"summary": {},
		"status": "success",
	}


def write_result(path: Path, result: dict[str, Any]) -> None:
	"""以 UTF-8 和稳定键顺序原子写入结果。"""

	path.parent.mkdir(parents=True, exist_ok=True)
	temporary = path.with_name(f"{path.name}.tmp")
	temporary.write_text(
		json.dumps(result, ensure_ascii=False, indent=2, sort_keys=True) + "\n",
		encoding="utf-8",
	)
	temporary.replace(path)
