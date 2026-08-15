"""验证性能基线 Markdown 报告生成。"""

from __future__ import annotations

import json
import subprocess
import sys
import tempfile
from pathlib import Path


def write_json(path: Path, value: object) -> None:
	path.parent.mkdir(parents=True, exist_ok=True)
	path.write_text(json.dumps(value), encoding="utf-8")


def main() -> int:
	repository = Path(__file__).resolve().parents[2]
	with tempfile.TemporaryDirectory() as directory:
		root = Path(directory)
		write_json(root / "environment.json", {
			"authoritative": False,
			"runner_kind": "github-hosted",
			"operating_system": "Linux",
			"os_release": "test",
			"cpu": "Architecture: x86_64\nModel name: test-cpu | fixed",
			"cpu_count": 4,
			"commit": "0123456789abcdef",
		})
		scenario = root / "tcp-echo"
		results = []
		for implementation, rate, rss in (("skyuv", 900.0, 1024 * 1024), ("upstream", 1000.0, 2 * 1024 * 1024)):
			filename = f"tcp-echo-{implementation}.json"
			write_json(scenario / filename, {
				"summary": {
					"throughput_ops_per_second_median": rate,
					"latency_ms": {"p50": 0.1, "p95": 0.2, "p99": 0.3, "max": 0.4},
				},
			})
			results.append({
				"implementation": implementation,
				"result": filename,
				"process_metrics": {"total_cpu_seconds": 1.25, "peak_rss_bytes": rss},
				"client_process_metrics": {"total_cpu_seconds": 0.5, "peak_rss_bytes": 3 * 1024 * 1024},
			})
		write_json(scenario / "tcp-echo-comparison.json", {
			"benchmark": "tcp_echo",
			"paired": True,
			"results": results,
		})
		output = root / "report.md"
		subprocess.run([
			sys.executable,
			str(repository / "benchmarks" / "baseline_report.py"),
			"--input-directory", str(root),
			"--output", str(output),
		], check=True)
		report = output.read_text(encoding="utf-8")
		assert "非权威流程验证" in report
		assert "TCP 长连接 echo" in report
		assert "skyuv 相对原版吞吐差异：**-10.00%**" in report
		assert "1.00 MiB" in report
		assert "客户端 CPU(s)" in report
		assert "3.00 MiB" in report
		assert "test-cpu \\| fixed" in report
		assert "[tcp-echo-skyuv.json](tcp-echo/tcp-echo-skyuv.json)" in report
	return 0


if __name__ == "__main__":
	raise SystemExit(main())
