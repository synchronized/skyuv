"""TCP 短连接 connect/echo/close 基准客户端。"""

from __future__ import annotations

import argparse
import socket
import statistics
import sys
import time
from pathlib import Path

from protocol import ExitCode, add_common_arguments, collect_environment, create_result, write_result
from tcp_echo import latency_summary, receive_exact


def run_for(
	host: str,
	port: int,
	payload: bytes,
	duration: float,
	connect_timeout: float,
	measure: bool,
) -> tuple[int, float, list[float]]:
	operations = 0
	latencies: list[float] = []
	started = time.perf_counter()
	deadline = started + duration
	while time.perf_counter() < deadline:
		operation_started = time.perf_counter_ns()
		with socket.create_connection((host, port), timeout=connect_timeout) as client:
			client.settimeout(connect_timeout)
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
	parser.add_argument("--message-size", type=int, default=64, help="每条连接的 echo 字节数")
	parser.add_argument("--connect-timeout", type=float, default=5.0, help="单次连接超时秒数")
	return parser


def main(arguments: list[str] | None = None) -> int:
	parser = build_parser()
	args = parser.parse_args(arguments)
	if not 1 <= args.port <= 65535 or args.message_size <= 0 or args.connect_timeout <= 0:
		parser.error("端口、消息尺寸或连接超时无效")

	result = create_result("tcp_short_connection", "connect_echo_close", args, collect_environment(Path.cwd()))
	result["parameters"].update({
		"host": args.host,
		"port": args.port,
		"message_size_bytes": args.message_size,
		"requests_per_connection": 1,
	})
	payload = bytes((index + args.seed) % 251 for index in range(args.message_size))
	all_latencies: list[float] = []

	try:
		if args.warmup > 0:
			run_for(args.host, args.port, payload, args.warmup, args.connect_timeout, False)
		for iteration in range(1, args.iterations + 1):
			operations, elapsed, latencies = run_for(
				args.host,
				args.port,
				payload,
				args.duration,
				args.connect_timeout,
				True,
			)
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
		print(f"TCP 短连接基准失败：{error}", file=sys.stderr)
		return int(ExitCode.EXECUTION_FAILED)

	throughputs = [sample["throughput_ops_per_second"] for sample in result["samples"]]
	result["summary"] = {
		"throughput_ops_per_second_median": statistics.median(throughputs),
		"latency_ms": latency_summary(all_latencies),
	}
	write_result(args.output, result)
	print(f"TCP 短连接基准完成：中位吞吐 {result['summary']['throughput_ops_per_second_median']:.2f} ops/s")
	return int(ExitCode.SUCCESS)


if __name__ == "__main__":
	raise SystemExit(main())
