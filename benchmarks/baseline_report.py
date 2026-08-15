"""从性能基线原始结果生成便于审阅的 Markdown 报告。"""

from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Any


BENCHMARK_NAMES = {
	"actor_ping_pong": "Actor ping-pong",
	"tcp_echo": "TCP 长连接 echo",
	"tcp_short_connection": "TCP 短连接",
	"tcp_backpressure": "TCP 背压",
	"udp_request_reply": "UDP request/reply",
}


def load_json(path: Path) -> dict[str, Any]:
	return json.loads(path.read_text(encoding="utf-8"))


def format_number(value: Any, digits: int = 2) -> str:
	if not isinstance(value, (int, float)):
		return "—"
	return f"{value:.{digits}f}"


def format_bytes(value: Any) -> str:
	if not isinstance(value, (int, float)):
		return "—"
	return f"{value / (1024 * 1024):.2f} MiB"


def markdown_cell(value: Any) -> str:
	"""将环境元数据压缩为不会破坏 Markdown 表格的单行文本。"""

	text = str(value).replace("|", "\\|")
	return "<br>".join(part.strip() for part in text.splitlines() if part.strip()) or "unknown"


def cpu_summary(value: Any) -> str:
	"""从 lscpu 输出提取型号，完整信息仍保留在 environment.json。"""

	lines = str(value).splitlines()
	for line in lines:
		if line.startswith("Model name:"):
			return markdown_cell(line.split(":", 1)[1].strip())
	return markdown_cell(lines[0] if lines else "unknown")


def throughput(summary: dict[str, Any]) -> tuple[Any, str]:
	if "throughput_ops_per_second_median" in summary:
		return summary["throughput_ops_per_second_median"], "ops/s"
	return summary.get("throughput_bytes_per_second_median"), "B/s"


def generate_report(input_directory: Path) -> str:
	environment_path = input_directory / "environment.json"
	if not environment_path.is_file():
		raise ValueError("缺少 environment.json")
	environment = load_json(environment_path)
	manifests = sorted(input_directory.glob("*/*-comparison.json"))
	if not manifests:
		raise ValueError("未找到配对结果清单")

	authoritative = environment.get("authoritative") is True
	lines = [
		"# skyuv 性能基线报告",
		"",
		f"> 结论属性：{'权威固定机基线' if authoritative else '非权威流程验证'}。",
		"",
		"## 环境",
		"",
		"| 项目 | 值 |",
		"| --- | --- |",
		f"| Runner 类型 | {markdown_cell(environment.get('runner_kind', 'unknown'))} |",
		f"| 操作系统 | {markdown_cell(environment.get('operating_system', 'unknown'))} |",
		f"| CPU | {cpu_summary(environment.get('cpu', 'unknown'))} |",
		f"| 逻辑 CPU | {environment.get('cpu_count', 'unknown')} |",
		f"| 提交 | `{environment.get('commit', 'unknown')}` |",
		f"| 权威结果 | {'是' if authoritative else '否'} |",
		"",
		"## 配对结果",
	]

	for manifest_path in manifests:
		manifest = load_json(manifest_path)
		name = manifest.get("benchmark", manifest_path.parent.name)
		rows = []
		client_saturation = []
		throughputs: dict[str, float] = {}
		unit = ""
		for entry in manifest.get("results", []):
			result_path = manifest_path.parent / entry["result"]
			result = load_json(result_path)
			summary = result.get("summary", {})
			value, unit = throughput(summary)
			implementation = entry.get("implementation", "unknown")
			if isinstance(value, (int, float)):
				throughputs[implementation] = float(value)
			latency = summary.get("latency_ms", {})
			metrics = entry.get("process_metrics", {})
			client_metrics = entry.get("client_process_metrics", {})
			client_average_cores = client_metrics.get("average_cpu_cores")
			if isinstance(client_average_cores, (int, float)) and client_average_cores >= 0.9:
				client_saturation.append(f"{implementation} 客户端平均占用 {client_average_cores:.2f} 核")
			rows.append(
				f"| {implementation} | {format_number(value)} | {format_number(latency.get('p50'), 3)} | "
				f"{format_number(latency.get('p95'), 3)} | {format_number(latency.get('p99'), 3)} | "
				f"{format_number(summary.get('packet_loss_ratio'), 4)} | "
				f"{format_number(metrics.get('total_cpu_seconds'), 3)} | "
				f"{format_number(metrics.get('average_cpu_cores'), 3)} | "
				f"{format_bytes(metrics.get('peak_rss_bytes'))} | "
				f"{format_number(client_metrics.get('total_cpu_seconds'), 3)} | "
				f"{format_number(client_average_cores, 3)} | "
				f"{format_bytes(client_metrics.get('peak_rss_bytes'))} | "
				f"[{entry['result']}]({manifest_path.parent.name}/{entry['result']}) |"
			)

		lines.extend([
			"",
			f"### {BENCHMARK_NAMES.get(name, name)}",
			"",
			f"吞吐单位：`{unit or 'unknown'}`。",
			"",
			"| 实现 | 中位吞吐 | p50 延迟(ms) | p95 延迟(ms) | p99 延迟(ms) | 丢包率 | 服务端 CPU(s) | 服务端平均核 | 服务端峰值 RSS | 客户端 CPU(s) | 客户端平均核 | 客户端峰值 RSS | 原始结果 |",
			"| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | --- |",
			*rows,
		])
		if client_saturation:
			lines.extend(["", f"> 客户端瓶颈提示：{'；'.join(client_saturation)}。"])
		upstream = throughputs.get("upstream")
		skyuv = throughputs.get("skyuv")
		if upstream and skyuv is not None:
			difference = (skyuv / upstream - 1.0) * 100.0
			lines.extend(["", f"skyuv 相对原版吞吐差异：**{difference:+.2f}%**。"])

	lines.extend([
		"",
		"## 解读约束",
		"",
		"- 非权威 Runner 的结果只用于验证采集和报告流程，不用于设置性能回归阈值。",
		"- 单次结果不能代表噪声范围；权威基线必须在固定主机的两个独立时段采集。",
		"- 优化判断应同时检查吞吐、尾延迟、CPU 和峰值 RSS，不能只比较单一指标。",
		"",
	])
	return "\n".join(lines)


def main() -> int:
	parser = argparse.ArgumentParser(description="生成 skyuv 性能基线 Markdown 报告")
	parser.add_argument("--input-directory", type=Path, required=True, help="基线原始结果目录")
	parser.add_argument("--output", type=Path, required=True, help="Markdown 输出路径")
	arguments = parser.parse_args()
	try:
		report = generate_report(arguments.input_directory.resolve())
	except (OSError, ValueError, KeyError, json.JSONDecodeError) as error:
		parser.error(str(error))
	arguments.output.parent.mkdir(parents=True, exist_ok=True)
	arguments.output.write_text(report, encoding="utf-8")
	return 0


if __name__ == "__main__":
	raise SystemExit(main())
