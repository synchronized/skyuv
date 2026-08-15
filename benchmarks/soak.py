"""在指定时长内重复运行命令，并保存长时稳定性汇总与逐轮日志。"""

from __future__ import annotations

import argparse
import json
import os
import platform
import subprocess
import sys
import time
from datetime import datetime, timezone
from pathlib import Path


def write_summary(path: Path, summary: dict[str, object]) -> None:
	temporary = path.with_suffix(path.suffix + ".tmp")
	temporary.write_text(
		json.dumps(summary, ensure_ascii=False, indent=2, sort_keys=True) + "\n", encoding="utf-8"
	)
	temporary.replace(path)


def configure_output_encoding() -> None:
	"""确保 Windows 非交互管道也能输出中文进度。"""

	for stream in (sys.stdout, sys.stderr):
		reconfigure = getattr(stream, "reconfigure", None)
		if reconfigure is not None:
			reconfigure(encoding="utf-8", errors="replace")


def main() -> int:
	configure_output_encoding()
	parser = argparse.ArgumentParser(description=__doc__)
	parser.add_argument("--duration-seconds", type=float, default=3600.0)
	parser.add_argument("--max-iterations", type=int, help="测试协议使用的最大轮数")
	parser.add_argument("--output-directory", type=Path, required=True)
	parser.add_argument("command", nargs=argparse.REMAINDER)
	args = parser.parse_args()
	command = args.command[1:] if args.command[:1] == ["--"] else args.command
	if args.duration_seconds <= 0 or args.max_iterations is not None and args.max_iterations <= 0:
		parser.error("运行时长和最大轮数必须大于零")
	if not command:
		parser.error("必须在 -- 后提供要重复运行的命令")

	args.output_directory.mkdir(parents=True, exist_ok=True)
	started_wall = datetime.now(timezone.utc)
	started = time.monotonic()
	summary: dict[str, object] = {
		"schema_version": 1,
		"status": "running",
		"started_at_utc": started_wall.isoformat(),
		"requested_duration_seconds": args.duration_seconds,
		"platform": platform.platform(),
		"python": sys.version,
		"command": command,
		"iterations": [],
	}
	write_summary(args.output_directory / "soak-summary.json", summary)

	iteration = 0
	command_environment = os.environ.copy()
	command_environment["PYTHONUTF8"] = "1"
	command_environment["PYTHONIOENCODING"] = "utf-8"
	while True:
		iteration += 1
		iteration_started = time.monotonic()
		completed = subprocess.run(
			command,
			capture_output=True,
			text=True,
			encoding="utf-8",
			errors="replace",
			env=command_environment,
		)
		elapsed = time.monotonic() - iteration_started
		log_name = f"iteration-{iteration:05d}.log"
		(args.output_directory / log_name).write_text(
			completed.stdout + completed.stderr, encoding="utf-8"
		)
		summary["iterations"].append({
			"iteration": iteration,
			"duration_seconds": elapsed,
			"return_code": completed.returncode,
			"log": log_name,
		})
		total_elapsed = time.monotonic() - started
		summary["elapsed_seconds"] = total_elapsed
		summary["completed_iterations"] = iteration
		if completed.returncode != 0:
			summary["status"] = "failed"
			summary["finished_at_utc"] = datetime.now(timezone.utc).isoformat()
			write_summary(args.output_directory / "soak-summary.json", summary)
			print(f"soak 第 {iteration} 轮失败，退出码 {completed.returncode}", file=sys.stderr)
			return completed.returncode
		write_summary(args.output_directory / "soak-summary.json", summary)
		print(f"soak 第 {iteration} 轮通过，累计 {total_elapsed:.2f} 秒", flush=True)
		if total_elapsed >= args.duration_seconds:
			break
		if args.max_iterations is not None and iteration >= args.max_iterations:
			break

	summary["status"] = "success"
	summary["finished_at_utc"] = datetime.now(timezone.utc).isoformat()
	write_summary(args.output_directory / "soak-summary.json", summary)
	return 0


if __name__ == "__main__":
	raise SystemExit(main())
