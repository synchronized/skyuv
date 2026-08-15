"""验证进程资源监控协议及 Linux /proc 采样。"""

from __future__ import annotations

import platform
import subprocess
import sys
from pathlib import Path


def main() -> int:
	repository = Path(__file__).resolve().parents[2]
	sys.path.insert(0, str(repository / "benchmarks"))
	from process_metrics import ProcessMetrics

	process = subprocess.Popen([
		sys.executable, "-c",
		"import time; data = bytearray(4 * 1024 * 1024); sum(data); time.sleep(0.15)",
	])
	monitor = ProcessMetrics(process.pid)
	monitor.start()
	process.wait(timeout=5)
	metrics = monitor.stop()
	if platform.system() == "Linux":
		assert metrics["available"] is True
		assert metrics["peak_rss_bytes"] >= 4 * 1024 * 1024
		assert metrics["total_cpu_seconds"] >= 0
	else:
		assert metrics["available"] is False
	return 0


if __name__ == "__main__":
	raise SystemExit(main())
