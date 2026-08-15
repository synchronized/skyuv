"""启动 Skynet 并采集双 Actor ping-pong 调度基准。"""

from __future__ import annotations

import argparse
import os
import re
import statistics
import subprocess
import sys
from pathlib import Path

from protocol import ExitCode, add_common_arguments, collect_environment, create_result, write_result


def build_parser() -> argparse.ArgumentParser:
	parser = argparse.ArgumentParser(description=__doc__)
	add_common_arguments(parser)
	parser.add_argument("--executable", type=Path, required=True, help="Skynet 可执行文件")
	parser.add_argument("--config", type=Path, required=True, help="Actor 基准配置文件")
	parser.add_argument("--process-timeout", type=float, default=120.0, help="基准进程总超时秒数")
	parser.add_argument("--actors", type=int, default=2, help="参与的 Actor 数量")
	parser.add_argument("--benchmark-name", default="actor_ping_pong", help="结果中的基准名称")
	parser.add_argument("--scenario", default="two_service_call_return", help="结果中的场景名称")
	parser.add_argument("--sample-marker", default="SKYUV_ACTOR_SAMPLE", help="Lua 样本日志标记")
	return parser


def parse_samples(output: str, marker: str) -> list[dict[str, object]]:
	pattern = re.compile(
		re.escape(marker)
		+ r"\s+(\d+)\s+(\d+)\s+([0-9.]+)\s+([0-9.]+)\s+([0-9.]+)\s+([0-9.]+)\s+([0-9.]+)"
	)
	samples: list[dict[str, object]] = []
	for match in pattern.finditer(output):
		iteration, operations, elapsed, p50, p95, p99, maximum = match.groups()
		duration = float(elapsed)
		count = int(operations)
		samples.append({
			"iteration": int(iteration),
			"duration_seconds": duration,
			"operations": count,
			"throughput_ops_per_second": count / duration,
			"latency_ms": {
				"p50": float(p50),
				"p95": float(p95),
				"p99": float(p99),
				"max": float(maximum),
			},
		})
	return samples


def main(arguments: list[str] | None = None) -> int:
	parser = build_parser()
	args = parser.parse_args(arguments)
	if args.process_timeout <= 0 or args.actors <= 0:
		parser.error("进程超时和 Actor 数量必须大于 0")

	result = create_result(args.benchmark_name, args.scenario, args, collect_environment(Path.cwd()))
	result["parameters"].update({
		"actor_count": args.actors,
		"executable": str(args.executable),
		"config": str(args.config),
	})
	environment = os.environ.copy()
	environment.update({
		"SKYUV_BENCHMARK_WARMUP": str(args.warmup),
		"SKYUV_BENCHMARK_DURATION": str(args.duration),
		"SKYUV_BENCHMARK_ITERATIONS": str(args.iterations),
		"SKYUV_BENCHMARK_ACTORS": str(args.actors),
	})

	try:
		config = args.config.resolve()
		completed = subprocess.run(
			[str(args.executable.resolve()), config.name],
			cwd=config.parent,
			env=environment,
			capture_output=True,
			text=True,
			encoding="utf-8",
			errors="replace",
			timeout=args.process_timeout,
			check=False,
		)
		combined_output = completed.stdout + "\n" + completed.stderr
		if completed.returncode != 0:
			raise RuntimeError(f"Skynet 退出码为 {completed.returncode}：{combined_output[-2000:]}")
		result["samples"] = parse_samples(combined_output, args.sample_marker)
		if len(result["samples"]) != args.iterations:
			raise ValueError(f"期望 {args.iterations} 轮，实际解析到 {len(result['samples'])} 轮")
	except (OSError, subprocess.TimeoutExpired, RuntimeError, ValueError) as error:
		result["status"] = "error"
		result["error"] = {"exit_code": int(ExitCode.EXECUTION_FAILED), "message": str(error)}
		write_result(args.output, result)
		print(f"Actor ping-pong 基准失败：{error}", file=sys.stderr)
		return int(ExitCode.EXECUTION_FAILED)

	throughputs = [sample["throughput_ops_per_second"] for sample in result["samples"]]
	result["summary"] = {
		"throughput_ops_per_second_median": statistics.median(throughputs),
		"latency_ms": {
			name: statistics.median(sample["latency_ms"][name] for sample in result["samples"])
			for name in ("p50", "p95", "p99", "max")
		},
	}
	write_result(args.output, result)
	print(f"Actor ping-pong 基准完成：中位吞吐 {result['summary']['throughput_ops_per_second_median']:.2f} ops/s")
	return int(ExitCode.SUCCESS)


if __name__ == "__main__":
	raise SystemExit(main())
