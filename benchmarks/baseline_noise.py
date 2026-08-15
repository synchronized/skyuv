"""比较两个独立时段的性能基线，量化已观测漂移。"""

from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Any

from baseline_report import BENCHMARK_NAMES, cpu_summary, load_json


ENVIRONMENT_KEYS = (
	"runner_kind",
	"runner_name",
	"operating_system",
	"machine",
	"cpu_count",
	"clang",
	"frequency_governor",
	"commit",
)
METRIC_NAMES = {
	"throughput": "中位吞吐",
	"latency_p50_ms": "p50 延迟(ms)",
	"latency_p95_ms": "p95 延迟(ms)",
	"latency_p99_ms": "p99 延迟(ms)",
	"latency_max_ms": "最大延迟(ms)",
	"packet_loss_ratio": "丢包率",
	"total_cpu_seconds": "CPU(s)",
	"peak_rss_bytes": "峰值 RSS(bytes)",
}


def percent_change(first: float, second: float) -> float | None:
	if first == 0:
		return 0.0 if second == 0 else None
	return (second / first - 1.0) * 100.0


def environment_signature(environment: dict[str, Any]) -> dict[str, Any]:
	return {
		**{key: environment.get(key) for key in ENVIRONMENT_KEYS},
		"cpu_model": cpu_summary(environment.get("cpu", "unknown")),
		"authoritative": environment.get("authoritative") is True,
	}


def load_run(directory: Path) -> tuple[dict[str, Any], dict[tuple[str, str], dict[str, Any]]]:
	environment = load_json(directory / "environment.json")
	measurements: dict[tuple[str, str], dict[str, Any]] = {}
	for manifest_path in sorted(directory.glob("*/*-comparison.json")):
		manifest = load_json(manifest_path)
		benchmark = manifest["benchmark"]
		for entry in manifest["results"]:
			result = load_json(manifest_path.parent / entry["result"])
			summary = result.get("summary", {})
			latency = summary.get("latency_ms", {})
			metrics = entry.get("process_metrics", {})
			throughput = summary.get(
				"throughput_ops_per_second_median",
				summary.get("throughput_bytes_per_second_median"),
			)
			measurements[(benchmark, entry["implementation"])] = {
				"parameters": manifest.get("parameters", {}),
				"throughput_unit": "ops/s" if "throughput_ops_per_second_median" in summary else "B/s",
				"metrics": {
					"throughput": throughput,
					"latency_p50_ms": latency.get("p50"),
					"latency_p95_ms": latency.get("p95"),
					"latency_p99_ms": latency.get("p99"),
					"latency_max_ms": latency.get("max"),
					"packet_loss_ratio": summary.get("packet_loss_ratio"),
					"total_cpu_seconds": metrics.get("total_cpu_seconds"),
					"peak_rss_bytes": metrics.get("peak_rss_bytes"),
				},
			}
	if not measurements:
		raise ValueError(f"{directory} 未找到配对结果")
	return environment, measurements


def compare_runs(first_directory: Path, second_directory: Path) -> dict[str, Any]:
	first_environment, first = load_run(first_directory)
	second_environment, second = load_run(second_directory)
	first_signature = environment_signature(first_environment)
	second_signature = environment_signature(second_environment)
	differences = [
		key for key in first_signature
		if key != "authoritative" and first_signature[key] != second_signature.get(key)
	]
	if differences:
		raise ValueError(f"环境不可比：{', '.join(differences)}")
	if set(first) != set(second):
		raise ValueError("两次采集的场景或实现集合不同")

	comparisons = []
	for benchmark, implementation in sorted(first):
		first_item = first[(benchmark, implementation)]
		second_item = second[(benchmark, implementation)]
		if first_item["parameters"] != second_item["parameters"]:
			raise ValueError(f"参数不可比：{benchmark}/{implementation}")
		if first_item["throughput_unit"] != second_item["throughput_unit"]:
			raise ValueError(f"吞吐单位不可比：{benchmark}/{implementation}")
		metrics = {}
		for name in METRIC_NAMES:
			first_value = first_item["metrics"].get(name)
			second_value = second_item["metrics"].get(name)
			if not isinstance(first_value, (int, float)) or not isinstance(second_value, (int, float)):
				continue
			metrics[name] = {
				"first": first_value,
				"second": second_value,
				"change_percent": percent_change(float(first_value), float(second_value)),
			}
		comparisons.append({
			"benchmark": benchmark,
			"implementation": implementation,
			"throughput_unit": first_item["throughput_unit"],
			"metrics": metrics,
		})

	return {
		"schema_version": 1,
		"comparable": True,
		"authoritative": first_signature["authoritative"] and second_signature["authoritative"],
		"first": {"directory": first_directory.name, "timestamp_utc": first_environment.get("timestamp_utc")},
		"second": {"directory": second_directory.name, "timestamp_utc": second_environment.get("timestamp_utc")},
		"environment": first_signature,
		"comparisons": comparisons,
	}


def format_value(value: float) -> str:
	return f"{value:.4f}"


def format_change(value: float | None) -> str:
	return "不可计算" if value is None else f"{value:+.2f}%"


def render_markdown(comparison: dict[str, Any]) -> str:
	lines = [
		"# skyuv 性能基线两时段比较",
		"",
		f"> 结论属性：{'权威固定机比较' if comparison['authoritative'] else '非权威流程验证'}。",
		"",
		"两次采集的环境、提交、工具链和场景参数已通过一致性检查。下表变化率为第二时段相对第一时段的观测漂移。",
	]
	for item in comparison["comparisons"]:
		lines.extend([
			"",
			f"## {BENCHMARK_NAMES.get(item['benchmark'], item['benchmark'])} / {item['implementation']}",
			"",
			"| 指标 | 第一时段 | 第二时段 | 变化率 |",
			"| --- | ---: | ---: | ---: |",
		])
		for name, metric in item["metrics"].items():
			label = METRIC_NAMES[name]
			if name == "throughput":
				label = f"{label}({item['throughput_unit']})"
			lines.append(
				f"| {label} | {format_value(metric['first'])} | "
				f"{format_value(metric['second'])} | {format_change(metric['change_percent'])} |"
			)
	lines.extend([
		"",
		"## 解读约束",
		"",
		"- 两个时段只能给出已观测漂移，不能单独证明长期统计分布。",
		"- 非权威 Runner 的比较结果不得用于设置回归阈值。",
		"- 确定阈值前应结合更多重复采集，并同时检查吞吐、尾延迟、CPU 和峰值 RSS。",
		"",
	])
	return "\n".join(lines)


def main() -> int:
	parser = argparse.ArgumentParser(description="比较两次 skyuv 性能基线")
	parser.add_argument("--first", type=Path, required=True, help="第一时段基线目录")
	parser.add_argument("--second", type=Path, required=True, help="第二时段基线目录")
	parser.add_argument("--output-json", type=Path, required=True, help="结构化比较结果")
	parser.add_argument("--output-markdown", type=Path, required=True, help="Markdown 比较报告")
	arguments = parser.parse_args()
	try:
		comparison = compare_runs(arguments.first.resolve(), arguments.second.resolve())
	except (OSError, ValueError, KeyError, json.JSONDecodeError) as error:
		parser.error(str(error))
	arguments.output_json.parent.mkdir(parents=True, exist_ok=True)
	arguments.output_markdown.parent.mkdir(parents=True, exist_ok=True)
	arguments.output_json.write_text(
		json.dumps(comparison, ensure_ascii=False, indent=2, sort_keys=True) + "\n",
		encoding="utf-8",
	)
	arguments.output_markdown.write_text(render_markdown(comparison), encoding="utf-8")
	return 0


if __name__ == "__main__":
	raise SystemExit(main())
