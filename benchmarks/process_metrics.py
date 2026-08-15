"""采集被测服务进程的 CPU 时间和峰值常驻内存。"""

from __future__ import annotations

import os
import platform
import subprocess
import threading
import time
from pathlib import Path


class ProcessMetrics:
	"""在 Linux 上轮询 /proc；其他平台明确返回不可用。"""

	def __init__(self, process_id: int, interval: float = 0.01) -> None:
		self.process_id = process_id
		self.interval = interval
		self.available = platform.system() == "Linux"
		self.user_seconds = 0.0
		self.system_seconds = 0.0
		self.peak_rss_bytes = 0
		self._stop = threading.Event()
		self._thread: threading.Thread | None = None
		self._started_at: float | None = None

	def start(self) -> None:
		self._started_at = time.monotonic()
		if not self.available:
			return
		self._sample()
		self._thread = threading.Thread(target=self._run, name="skyuv-process-metrics", daemon=True)
		self._thread.start()

	def stop(self) -> dict[str, object]:
		if self._thread is not None:
			self._stop.set()
			self._thread.join()
			self._sample()
		wall_seconds = 0.0 if self._started_at is None else time.monotonic() - self._started_at
		total_cpu_seconds = self.user_seconds + self.system_seconds
		return {
			"available": self.available,
			"source": "linux-procfs" if self.available else "unsupported-platform",
			"user_cpu_seconds": self.user_seconds,
			"system_cpu_seconds": self.system_seconds,
			"total_cpu_seconds": total_cpu_seconds,
			"peak_rss_bytes": self.peak_rss_bytes,
			"monitored_wall_seconds": wall_seconds,
			"average_cpu_cores": total_cpu_seconds / wall_seconds if self.available and wall_seconds > 0 else None,
		}

	def _run(self) -> None:
		while not self._stop.wait(self.interval):
			self._sample()

	def _sample(self) -> None:
		if not self.available:
			return
		try:
			stat = Path(f"/proc/{self.process_id}/stat").read_text(encoding="utf-8")
			fields = stat[stat.rfind(")") + 2:].split()
			clock_ticks = os.sysconf("SC_CLK_TCK")
			self.user_seconds = max(self.user_seconds, int(fields[11]) / clock_ticks)
			self.system_seconds = max(self.system_seconds, int(fields[12]) / clock_ticks)
			status = Path(f"/proc/{self.process_id}/status").read_text(encoding="utf-8")
		except (OSError, ValueError, IndexError):
			return
		for line in status.splitlines():
			if line.startswith(("VmHWM:", "VmRSS:")):
				parts = line.split()
				if len(parts) >= 2:
					self.peak_rss_bytes = max(self.peak_rss_bytes, int(parts[1]) * 1024)


def wait_with_metrics(process: subprocess.Popen[bytes]) -> tuple[int, dict[str, object]]:
	"""等待子进程退出，并返回退出码及其资源指标。"""

	monitor = ProcessMetrics(process.pid)
	monitor.start()
	try:
		return_code = process.wait()
	except BaseException:
		if process.poll() is None:
			process.kill()
			process.wait()
		raise
	finally:
		metrics = monitor.stop()
	return return_code, metrics
