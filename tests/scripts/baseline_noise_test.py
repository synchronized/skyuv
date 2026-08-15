"""验证两时段性能基线比较及可比性检查。"""

from __future__ import annotations

import json
import sys
import tempfile
from pathlib import Path


REPOSITORY = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPOSITORY / "benchmarks"))

import baseline_noise


def write_json(path: Path, value: object) -> None:
	path.parent.mkdir(parents=True, exist_ok=True)
	path.write_text(json.dumps(value), encoding="utf-8")


def create_run(root: Path, throughput: float, cpu: str = "Model name: test-cpu") -> None:
	write_json(root / "environment.json", {
		"authoritative": True,
		"runner_kind": "self-hosted-fixed",
		"runner_name": "skyuv-benchmark-01",
		"operating_system": "Linux-test",
		"machine": "x86_64",
		"cpu": cpu,
		"cpu_count": 8,
		"clang": "clang 18",
		"frequency_governor": "performance",
		"commit": "0123456789abcdef",
	})
	scenario = root / "tcp-echo"
	write_json(scenario / "skyuv.json", {
		"summary": {
			"throughput_ops_per_second_median": throughput,
			"latency_ms": {"p50": 0.1, "p95": 0.2, "p99": 0.3, "max": 0.4},
		},
	})
	write_json(scenario / "tcp-echo-comparison.json", {
		"benchmark": "tcp_echo",
		"parameters": {"duration_seconds": 10, "iterations": 5},
		"results": [{
			"implementation": "skyuv",
			"result": "skyuv.json",
			"process_metrics": {"total_cpu_seconds": 2.0, "peak_rss_bytes": 1024},
		}],
	})


def main() -> int:
	with tempfile.TemporaryDirectory() as directory:
		root = Path(directory)
		first = root / "first"
		second = root / "second"
		create_run(first, 1000.0)
		create_run(second, 900.0)
		comparison = baseline_noise.compare_runs(first, second)
		assert comparison["comparable"] is True
		assert comparison["authoritative"] is True
		throughput = comparison["comparisons"][0]["metrics"]["throughput"]
		assert abs(throughput["change_percent"] - (-10.0)) < 0.0001
		report = baseline_noise.render_markdown(comparison)
		assert "权威固定机比较" in report
		assert "-10.00%" in report

		incompatible = root / "incompatible"
		create_run(incompatible, 900.0, cpu="Model name: another-cpu")
		try:
			baseline_noise.compare_runs(first, incompatible)
		except ValueError as error:
			assert "cpu_model" in str(error)
		else:
			raise AssertionError("不同 CPU 型号应被拒绝")
	return 0


if __name__ == "__main__":
	raise SystemExit(main())
