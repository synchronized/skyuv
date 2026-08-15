"""TCP 长连接 echo 基准客户端。"""

from __future__ import annotations

import argparse
import math
import socket
import statistics
import sys
import time
from pathlib import Path

from protocol import ExitCode, add_common_arguments, collect_environment, create_result, write_result


def percentile(values: list[float], fraction: float) -> float:
	"""使用最近秩法计算延迟分位数。"""

	if not values:
		return 0.0
	ordered = sorted(values)
	index = max(0, math.ceil(len(ordered) * fraction) - 1)
	return ordered[index]


def latency_summary(values: list[float]) -> dict[str, float]:
	return {
		"p50": percentile(values, 0.50),
		"p95": percentile(values, 0.95),
		"p99": percentile(values, 0.99),
		"max": max(values, default=0.0),
	}


def receive_exact(client: socket.socket, size: int) -> bytes:
	data = bytearray()
	while len(data) < size:
		chunk = client.recv(size - len(data))
		if not chunk:
			raise ConnectionError("服务端在响应完整前关闭连接")
		data.extend(chunk)
	return bytes(data)


def run_for(client: socket.socket, payload: bytes, duration: float, measure: bool) -> tuple[int, float, list[float]]:
	operations = 0
	latencies: list[float] = []
	started = time.perf_counter()
	deadline = started + duration
	while time.perf_counter() < deadline:
		operation_started = time.perf_counter_ns()
		client.sendall(payload)
		response = receive_exact(client, len(payload))
		if response != payload:
			raise ValueError("echo 响应内容不一致")
		operations += 1
		if measure:
			latencies.append((time.perf_counter_ns() - operation_started) / 1_000_000.0)
	return operations, time.perf_counter() - started, latencies


def build_parser() -> argparse.ArgumentParser:
	parser = argparse.ArgumentParser(description=__doc__)
	add_common_arguments(parser)
	parser.add_argument("--host", default="127.0.0.1", help="echo 服务端地址")
	parser.add_argument("--port", type=int, default=25281, help="echo 服务端端口")
	parser.add_argument("--message-size", type=int, default=64, help="每次 echo 的字节数")
	parser.add_argument("--connect-timeout", type=float, default=5.0, help="连接超时秒数")
	return parser


def main(arguments: list[str] | None = None) -> int:
	parser = build_parser()
	args = parser.parse_args(arguments)
	if not 1 <= args.port <= 65535 or args.message_size <= 0 or args.connect_timeout <= 0:
		parser.error("端口、消息尺寸或连接超时无效")

	result = create_result("tcp_echo", "persistent_round_trip", args, collect_environment(Path.cwd()))
	result["parameters"].update({
		"host": args.host,
		"port": args.port,
		"message_size_bytes": args.message_size,
		"connections": 1,
	})
	payload = bytes((index + args.seed) % 251 for index in range(args.message_size))
	all_latencies: list[float] = []

	try:
		with socket.create_connection((args.host, args.port), timeout=args.connect_timeout) as client:
			client.settimeout(max(args.connect_timeout, args.duration + args.warmup + 1.0))
			if args.warmup > 0:
				run_for(client, payload, args.warmup, False)
			for iteration in range(1, args.iterations + 1):
				operations, elapsed, latencies = run_for(client, payload, args.duration, True)
				all_latencies.extend(latencies)
				result["samples"].append({
					"iteration": iteration,
					"duration_seconds": elapsed,
					"operations": operations,
					"throughput_ops_per_second": operations / elapsed,
					"latency_ms": latency_summary(latencies),
				})
	except (OSError, ValueError) as error:
		result["status"] = "error"
		result["error"] = {"exit_code": int(ExitCode.EXECUTION_FAILED), "message": str(error)}
		write_result(args.output, result)
		print(f"TCP echo 基准失败：{error}", file=sys.stderr)
		return int(ExitCode.EXECUTION_FAILED)

	throughputs = [sample["throughput_ops_per_second"] for sample in result["samples"]]
	result["summary"] = {
		"throughput_ops_per_second_median": statistics.median(throughputs),
		"latency_ms": latency_summary(all_latencies),
	}
	write_result(args.output, result)
	print(f"TCP echo 基准完成：中位吞吐 {result['summary']['throughput_ops_per_second_median']:.2f} ops/s")
	return int(ExitCode.SUCCESS)


if __name__ == "__main__":
	raise SystemExit(main())
