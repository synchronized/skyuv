"""启动 Skynet 并采集多生产者单消费者 Actor 基准。"""

from __future__ import annotations

import argparse
import os
import re
import statistics
import subprocess
import sys
from pathlib import Path

from protocol import ExitCode, add_common_arguments, collect_environment, create_result, write_result


SAMPLE_PATTERN = re.compile(
	r"SKYUV_ACTOR_MULTI_SAMPLE\s+(\d+)\s+(\d+)\s+(\d+)\s+([0-9.]+)\s+(\d+)\s+(\d+)"
)


def positive_int(value: str) -> int:
	parsed = int(value)
	if parsed <= 0:
		raise argparse.ArgumentTypeError("必须大于 0")
	return parsed


def build_parser() -> argparse.ArgumentParser:
	parser = argparse.ArgumentParser(description=__doc__)
	add_common_arguments(parser)
	parser.add_argument("--executable", type=Path, required=True, help="Skynet 可执行文件")
	parser.add_argument("--config", type=Path, required=True, help="Actor 基准配置文件")
	parser.add_argument("--producers", type=positive_int, default=4, help="生产者 Actor 数量")
	parser.add_argument("--process-timeout", type=float, default=120.0, help="基准进程总超时秒数")
	return parser


def parse_samples(output: str) -> list[dict[str, object]]:
	samples: list[dict[str, object]] = []
	for match in SAMPLE_PATTERN.finditer(output):
		iteration, sent, received, elapsed, minimum, maximum = match.groups()
		duration = float(elapsed)
		sent_count = int(sent)
		received_count = int(received)
		minimum_count = int(minimum)
		maximum_count = int(maximum)
		samples.append({
			"iteration": int(iteration),
			"duration_seconds": duration,
			"operations": received_count,
			"messages_sent": sent_count,
			"messages_received": received_count,
			"throughput_ops_per_second": received_count / duration,
			"producer_messages_min": minimum_count,
			"producer_messages_max": maximum_count,
			"fairness_ratio": minimum_count / maximum_count if maximum_count else 0.0,
		})
	return samples


def main(arguments: list[str] | None = None) -> int:
	parser = build_parser()
	args = parser.parse_args(arguments)
	if args.process_timeout <= 0:
		parser.error("进程超时必须大于 0")

	result = create_result(
		"actor_multi_producer", "many_to_one_async_send", args, collect_environment(Path.cwd())
	)
	result["parameters"].update({
		"producer_count": args.producers,
		"consumer_count": 1,
		"executable": str(args.executable),
		"config": str(args.config),
	})
	environment = os.environ.copy()
	environment.update({
		"SKYUV_BENCHMARK_WARMUP": str(args.warmup),
		"SKYUV_BENCHMARK_DURATION": str(args.duration),
		"SKYUV_BENCHMARK_ITERATIONS": str(args.iterations),
		"SKYUV_BENCHMARK_PRODUCERS": str(args.producers),
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
		result["samples"] = parse_samples(combined_output)
		if len(result["samples"]) != args.iterations:
			raise ValueError(f"期望 {args.iterations} 轮，实际解析到 {len(result['samples'])} 轮")
		if any(sample["messages_sent"] != sample["messages_received"] for sample in result["samples"]):
			raise ValueError("生产与消费消息计数不一致")
	except (OSError, subprocess.TimeoutExpired, RuntimeError, ValueError) as error:
		result["status"] = "error"
		result["error"] = {"exit_code": int(ExitCode.EXECUTION_FAILED), "message": str(error)}
		write_result(args.output, result)
		print(f"Actor 多生产者基准失败：{error}", file=sys.stderr)
		return int(ExitCode.EXECUTION_FAILED)

	throughputs = [sample["throughput_ops_per_second"] for sample in result["samples"]]
	fairness = [sample["fairness_ratio"] for sample in result["samples"]]
	result["summary"] = {
		"throughput_ops_per_second_median": statistics.median(throughputs),
		"fairness_ratio_median": statistics.median(fairness),
		"fairness_ratio_min": min(fairness),
	}
	write_result(args.output, result)
	print(f"Actor 多生产者基准完成：中位吞吐 {result['summary']['throughput_ops_per_second_median']:.2f} msg/s")
	return int(ExitCode.SUCCESS)


if __name__ == "__main__":
	raise SystemExit(main())
