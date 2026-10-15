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
			"process_metrics": {"total_cpu_seconds": 2.0, "average_cpu_cores": 0.5, "peak_rss_bytes": 1024},
			"client_process_metrics": {
				"total_cpu_seconds": 1.0, "average_cpu_cores": 0.4, "peak_rss_bytes": 2048,
			},
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
		assert "client_total_cpu_seconds" in comparison["comparisons"][0]["metrics"]
		assert "client_average_cpu_cores" in comparison["comparisons"][0]["metrics"]
		report = baseline_noise.render_markdown(comparison)
		assert "权威固定机比较" in report
		assert "-10.00%" in report
		assert "无逐轮数据" in report
		# 两时段中位数完全相同，仍需暴露单轮吞吐骤降与尾延迟突增。
		for directory, slow in ((first, False), (second, True)):
			path = directory / "tcp-echo/skyuv.json"
			data = json.loads(path.read_text())
			data["summary"]["throughput_ops_per_second_median"] = 1000.0
			data["samples"] = [
				{"throughput_ops_per_second": 100.0 if slow and index == 0 else 1000.0,
				 "latency_ms": {"p99": 60.0 if slow and index == 0 else 0.3}}
				for index in range(5)
			]
			write_json(path, data)
		variation = baseline_noise.compare_runs(first, second)["comparisons"][0]
		assert variation["metrics"]["throughput"]["change_percent"] == 0.0
		assert variation["sample_variability"]["second"]["throughput"]["range_over_median_percent"] == 90.0
		assert variation["sample_variability"]["second"]["latency_p99_ms"]["maximum"] == 60.0
		assert "60.0000" in baseline_noise.render_markdown(baseline_noise.compare_runs(first, second))
		assert baseline_noise.sample_variability({"samples": [{"throughput_bytes_per_second": 0}]})["throughput"]["range_over_median_percent"] is None
		for invalid in (-1, float("nan"), float("inf"), True):
			try:
				baseline_noise.sample_variability({"samples": [{"throughput_ops_per_second": invalid}]})
			except ValueError:
				pass
			else:
				raise AssertionError("无效逐轮指标应被拒绝")

		for key in ("machine_identity", "environment_policy", "memory_total_bytes"):
			environment_path = second / "environment.json"
			environment = json.loads(environment_path.read_text())
			environment[key] = "changed"
			write_json(environment_path, environment)
			try:
				baseline_noise.compare_runs(first, second)
			except ValueError as error:
				assert key in str(error)
			else:
				raise AssertionError(f"环境字段 {key} 变化应被拒绝")
			del environment[key]
			write_json(environment_path, environment)

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
