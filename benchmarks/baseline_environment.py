"""采集性能基线所需的固定 Linux 主机、工具链和源码版本信息。"""

from __future__ import annotations

import argparse
import json
import os
import platform
import subprocess
from datetime import datetime, timezone
from pathlib import Path


def command_output(*command: str) -> str:
	try:
		return subprocess.check_output(command, text=True, stderr=subprocess.STDOUT).strip()
	except (OSError, subprocess.CalledProcessError) as error:
		return f"unavailable: {error}"


def read_text(path: Path) -> str:
	try:
		return path.read_text(encoding="utf-8", errors="replace").strip()
	except OSError:
		return "unavailable"


def main() -> int:
	parser = argparse.ArgumentParser(description=__doc__)
	parser.add_argument("--runner-kind", choices=("github-hosted", "self-hosted-fixed"), required=True)
	parser.add_argument("--authoritative", choices=("true", "false"), required=True)
	parser.add_argument("--output", type=Path, required=True)
	args = parser.parse_args()

	repository = Path.cwd()
	data = {
		"schema_version": 1,
		"timestamp_utc": datetime.now(timezone.utc).isoformat(),
		"authoritative": args.authoritative == "true",
		"runner_kind": args.runner_kind,
		"runner_name": os.environ.get("RUNNER_NAME", "local"),
		"operating_system": platform.platform(),
		"machine": platform.machine(),
		"cpu_count": os.cpu_count() or 0,
		"cpu": command_output("lscpu"),
		"memory": command_output("free", "--bytes"),
		"frequency_governor": read_text(Path("/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor")),
		"clang": command_output("clang", "--version"),
		"cmake": command_output("cmake", "--version"),
		"ninja": command_output("ninja", "--version"),
		"python": platform.python_version(),
		"commit": command_output("git", "rev-parse", "HEAD"),
		"dirty": bool(command_output("git", "status", "--porcelain")),
		"submodules": command_output("git", "submodule", "status", "--recursive"),
	}
	args.output.parent.mkdir(parents=True, exist_ok=True)
	temporary = args.output.with_suffix(args.output.suffix + ".tmp")
	temporary.write_text(json.dumps(data, ensure_ascii=False, indent=2, sort_keys=True) + "\n", encoding="utf-8")
	temporary.replace(args.output)
	return 0


if __name__ == "__main__":
	raise SystemExit(main())
