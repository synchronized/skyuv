"""跨版本检查 skyuv 自身参考线；触线只提示复测，不自动拒绝合并。"""

from __future__ import annotations

import argparse
import json
import math
import statistics
from pathlib import Path
from typing import Any

from baseline_noise import environment_signature, load_run
from baseline_report import BENCHMARK_NAMES, load_json


SCENARIOS = {"actor_ping_pong", "tcp_echo", "tcp_short_connection", "tcp_backpressure", "udp_request_reply"}
POLICY = "skyuv-vmware-review-v1"


def number(value: Any, label: str, positive: bool = False) -> float:
	if isinstance(value, bool) or not isinstance(value, (int, float)) or not math.isfinite(value):
		raise ValueError(f"指标无效：{label}")
	if value < 0 or (positive and value == 0):
		raise ValueError(f"指标无效：{label}")
	return float(value)


def validated_run(directory: Path) -> tuple[dict[str, Any], dict[str, Any]]:
	environment, measurements = load_run(directory)
	signature = environment_signature(environment)
	if not signature["authoritative"] or environment.get("runner_kind") != "self-hosted-fixed":
		raise ValueError("回归检查仅接受固定 Runner 数据")
	if environment.get("dirty") is not False:
		raise ValueError("采集工作树必须干净")
	for key, value in signature.items():
		if value is None or value == "":
			raise ValueError(f"环境元数据缺失：{key}")
	if signature["machine_identity"] in ("unknown", "unavailable"):
		raise ValueError("缺少固定机器身份")
	if not environment.get("timestamp_utc"):
		raise ValueError("缺少窗口时间")
	items = {}
	for path in sorted(directory.glob("*/*-comparison.json")):
		manifest = load_json(path)
		for entry in manifest["results"]:
			if entry["implementation"] != "skyuv":
				continue
			name = manifest["benchmark"]
			if name in items:
				raise ValueError(f"场景重复：{name}")
			result = load_json(path.parent / entry["result"])
			if result.get("status") != "success":
				raise ValueError(f"场景执行未成功：{name}")
			if result.get("implementation") != "skyuv" or result.get("benchmark", {}).get("name") != name:
				raise ValueError(f"原始结果身份不一致：{name}")
			parameters = result["parameters"]
			result_environment = result["environment"]
			if result_environment.get("commit") != environment["commit"] or result_environment.get("dirty") is not False:
				raise ValueError(f"原始结果提交或工作树不一致：{name}")
			for key in ("allocator", "compiler", "build_type"):
				if not manifest["parameters"].get(key) or result_environment.get(key) != manifest["parameters"][key]:
					raise ValueError(f"构建参数不一致：{name}/{key}")
			if result_environment["build_type"] != "Release":
				raise ValueError("回归检查要求 Release")
			for key in ("duration_seconds", "iterations", "warmup_seconds"):
				if parameters.get(key) != manifest["parameters"].get(key):
					raise ValueError(f"清单与原始参数不一致：{name}/{key}")
			if number(parameters["warmup_seconds"], "warmup") <= 0:
				raise ValueError("需要预热")
			duration = number(parameters["duration_seconds"], "测量时长", True)
			samples = result["samples"]
			if len(samples) < 5 or parameters["iterations"] != len(samples):
				raise ValueError("每场景至少需要五轮完整样本")
			item = measurements[(name, "skyuv")]
			metrics = item["metrics"]
			if item["throughput_unit"] != ("B/s" if name == "tcp_backpressure" else "ops/s"):
				raise ValueError(f"吞吐单位无效：{name}")
			throughput_key = "throughput_bytes_per_second" if name == "tcp_backpressure" else "throughput_ops_per_second"
			values = []
			for index, sample in enumerate(samples, 1):
				if sample.get("iteration") != index:
					raise ValueError(f"样本序号不完整：{name}")
				if number(sample.get("duration_seconds"), "样本时长", True) < duration:
					raise ValueError(f"样本测量未完成：{name}")
				values.append(number(sample.get(throughput_key), "逐轮吞吐", True))
				if name != "tcp_backpressure":
					number(sample.get("latency_ms", {}).get("p99"), "逐轮 p99", True)
				if name == "udp_request_reply" and number(sample.get("packet_loss_ratio"), "逐轮丢包率") != 0:
					raise ValueError("UDP 丢包应按正确性问题处理")
			if not math.isclose(number(metrics["throughput"], "吞吐", True), statistics.median(values), rel_tol=1e-8):
				raise ValueError(f"中位吞吐与逐轮结果不一致：{name}")
			for key in ("peak_rss_bytes", "average_cpu_cores"):
				number(metrics[key], key, key == "peak_rss_bytes")
			if entry.get("process_metrics", {}).get("available") is not True:
				raise ValueError("缺少服务端资源采样")
			if name != "tcp_backpressure":
				number(metrics["latency_p99_ms"], "p99", True)
			if name == "udp_request_reply" and number(metrics["packet_loss_ratio"], "丢包率") != 0:
				raise ValueError("UDP 丢包应按正确性问题处理")
			item["result_parameters"] = parameters
			item["result_benchmark"] = result["benchmark"]
			items[name] = item
	if set(items) != SCENARIOS:
		raise ValueError("必须包含五类完整 skyuv 场景")
	return environment, items


def compatible(first: tuple, second: tuple, same_commit: bool) -> None:
	a, b = environment_signature(first[0]), environment_signature(second[0])
	differences = [key for key in a if (same_commit or key != "commit") and a[key] != b[key]]
	if differences:
		raise ValueError(f"环境不可比：{', '.join(differences)}")
	for name in SCENARIOS:
		for key in ("parameters", "result_parameters", "result_benchmark", "throughput_unit"):
			if first[1][name][key] != second[1][name][key]:
				raise ValueError(f"参数不可比：{name}/{key}")


def evaluate(reference_first: Path, reference_second: Path, candidate: Path, repeat: Path | None = None) -> dict[str, Any]:
	first, second = validated_run(reference_first), validated_run(reference_second)
	compatible(first, second, True)
	if first[0]["timestamp_utc"] == second[0]["timestamp_utc"]:
		raise ValueError("参考基线需要两个不同窗口")
	candidates = [validated_run(candidate)]
	if repeat is not None:
		candidates.append(validated_run(repeat))
		compatible(candidates[0], candidates[1], True)
		if candidates[0][0]["timestamp_utc"] == candidates[1][0]["timestamp_utc"]:
			raise ValueError("复测需要不同窗口")
	for run in candidates:
		compatible(first, run, False)
	rows = []
	for name in sorted(SCENARIOS):
		a, b = first[1][name], second[1][name]
		for metric in ("throughput", "latency_p99_ms", "peak_rss_bytes", "average_cpu_cores"):
			if metric == "latency_p99_ms" and name == "tcp_backpressure":
				continue
			values = [a["metrics"][metric], b["metrics"][metric]]
			lower = metric == "throughput"
			reference = min(values) if lower else max(values)
			if lower:
				limit = reference * (0.95 if name in ("actor_ping_pong", "tcp_backpressure") else 0.85)
			elif metric == "latency_p99_ms":
				limit = reference * (1.15 if name == "actor_ping_pong" else 1.4)
			elif metric == "average_cpu_cores":
				limit = reference + max(reference * 0.3, 0.05)
			else:
				limit = reference * 1.3
			if not math.isfinite(limit):
				raise ValueError("参考线数值溢出")
			observations = [run[1][name]["metrics"][metric] for run in candidates]
			crossed = [value < limit if lower else value > limit for value in observations]
			rows.append({"benchmark": name, "metric": metric, "reference": reference, "limit": limit,
				"direction": "minimum" if lower else "maximum", "candidate_values": observations,
				"crossed": crossed, "status": "investigate" if len(crossed) == 2 and all(crossed)
				else "retest" if any(crossed) else "within_reference"})
	status = "investigate" if any(row["status"] == "investigate" for row in rows) else (
		"retest" if any(row["status"] == "retest" for row in rows) else "within_reference")
	return {"schema_version": 1, "policy": POLICY, "status": status, "blocking": False,
		"environment": environment_signature(first[0]),
		"parameters": {name: item["result_parameters"] for name, item in first[1].items()},
		"reference_commit": first[0]["commit"], "candidate_commit": candidates[0][0]["commit"], "workload_review_required": True,
		"windows": [{"directory": path.name, "timestamp_utc": run[0]["timestamp_utc"]}
			for path, run in zip([reference_first, reference_second, candidate, *([repeat] if repeat else [])],
				[first, second, *candidates])], "checks": rows,
		"sample_variability": [{name: item["sample_variability"] for name, item in run[1].items()}
			for run in [first, second, *candidates]]}


def render_markdown(report: dict[str, Any]) -> str:
	lines = ["# skyuv 跨版本回归复测提示", "", f"结论：`{report['status']}`；策略：`{report['policy']}`。",
		f"参考提交：`{report['reference_commit']}`；候选提交：`{report['candidate_commit']}`。", "",
		"触线仅提示复测；同一指标在两个不同候选窗口都触线才提示调查，不自动拒绝合并。", "",
		"| 场景 | 指标 | 保守参考值 | 参考线 | 候选值 | 状态 |", "| --- | --- | ---: | ---: | --- | --- |"]
	for row in report["checks"]:
		values = " / ".join(f"{v:.6g}" for v in row["candidate_values"])
		lines.append(f"| {BENCHMARK_NAMES[row['benchmark']]} | {row['metric']} | {row['reference']:.6g} | "
			f"{row['direction']} {row['limit']:.6g} | {values} | {row['status']} |")
	lines.extend(["", "逐轮波动保存在 JSON 的 sample_variability 中，需联合审阅，不能丢弃异常轮次。",
		"工具核对已记录的环境和负载参数；worker 数和夹具语义等未完整记录的信息仍需人工复核。",
		"within_reference 仅表示未触本策略参考线，不证明长期稳定或正式版已验收。", ""])
	return "\n".join(lines)


def main() -> int:
	parser = argparse.ArgumentParser(description=__doc__)
	parser.add_argument("--reference-first", type=Path, required=True)
	parser.add_argument("--reference-second", type=Path, required=True)
	parser.add_argument("--candidate", type=Path, required=True)
	parser.add_argument("--candidate-repeat", type=Path)
	parser.add_argument("--output-json", type=Path, required=True)
	parser.add_argument("--output-markdown", type=Path, required=True)
	args = parser.parse_args()
	try:
		report = evaluate(args.reference_first, args.reference_second, args.candidate, args.candidate_repeat)
	except (OSError, ValueError, KeyError, TypeError) as error:
		parser.error(str(error))
	for path, text in ((args.output_json, json.dumps(report, ensure_ascii=False, indent=2)),
		(args.output_markdown, render_markdown(report))):
		path.parent.mkdir(parents=True, exist_ok=True)
		path.write_text(text + "\n", encoding="utf-8")
	return 0


if __name__ == "__main__":
	raise SystemExit(main())
