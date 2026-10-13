"""验证进程资源监控协议及 Linux /proc 采样。"""

from __future__ import annotations

import platform
import subprocess
import sys
import time
from pathlib import Path
from unittest.mock import patch


def main() -> int:
	repository = Path(__file__).resolve().parents[2]
	sys.path.insert(0, str(repository / "benchmarks"))
	from process_metrics import ProcessMetrics, wait_with_metrics

	process = subprocess.Popen([
		sys.executable, "-c",
		"import sys, time; sys.stdin.buffer.readline(); "
		"data = bytearray(4 * 1024 * 1024); sum(data); time.sleep(0.15)",
	], stdin=subprocess.PIPE)
	# 模拟父进程晚调度；子进程必须等待监控启动后才进入计时工作阶段。
	time.sleep(0.2)
	monitor = ProcessMetrics(process.pid)
	monitor.start()
	assert process.stdin is not None
	process.stdin.write(b"start\n")
	process.stdin.close()
	process.wait(timeout=5)
	metrics = monitor.stop()
	if platform.system() == "Linux":
		assert metrics["available"] is True
		assert metrics["peak_rss_bytes"] >= 4 * 1024 * 1024
		assert metrics["total_cpu_seconds"] >= 0
		assert metrics["average_cpu_cores"] is not None
	else:
		assert metrics["available"] is False
		assert metrics["average_cpu_cores"] is None
	assert metrics["monitored_wall_seconds"] >= 0.1

	waited = subprocess.Popen([sys.executable, "-c",
		"import sys, time; sys.stdin.buffer.readline(); time.sleep(0.05)"], stdin=subprocess.PIPE)
	original_start = ProcessMetrics.start
	def start_and_release(instance: ProcessMetrics) -> None:
		original_start(instance)
		assert waited.stdin is not None
		waited.stdin.write(b"start\n")
		waited.stdin.close()
	# 保持生产等待逻辑不变，只在真实监控启动后允许测试子进程执行。
	with patch.object(ProcessMetrics, "start", start_and_release):
		return_code, waited_metrics = wait_with_metrics(waited)
	assert return_code == 0
	assert waited_metrics["available"] is (platform.system() == "Linux")
	assert waited_metrics["monitored_wall_seconds"] >= 0.04
	return 0


if __name__ == "__main__":
	raise SystemExit(main())
