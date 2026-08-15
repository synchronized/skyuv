"""UDP request/reply 包速率、丢包与延迟基准。"""

from __future__ import annotations

import argparse
import socket
import statistics
import sys
import time
from pathlib import Path

from protocol import ExitCode, add_common_arguments, collect_environment, create_result, write_result
from tcp_echo import latency_summary


def run_for(
	client: socket.socket,
	endpoint: tuple[str, int],
	payload: bytes,
	duration: float,
	measure: bool,
) -> tuple[int, int, float, list[float]]:
	sent = 0
	received = 0
	latencies: list[float] = []
	started = time.perf_counter()
	deadline = started + duration
	while time.perf_counter() < deadline:
		operation_started = time.perf_counter_ns()
		client.sendto(payload, endpoint)
		sent += 1
		try:
			response, source = client.recvfrom(65535)
		except TimeoutError:
			continue
		if source[0] != endpoint[0] or source[1] != endpoint[1]:
			raise ValueError("UDP 响应来源不一致")
		if response != payload:
			raise ValueError("UDP 响应内容不一致")
		received += 1
		if measure:
			latencies.append((time.perf_counter_ns() - operation_started) / 1_000_000.0)
	return sent, received, time.perf_counter() - started, latencies


def build_parser() -> argparse.ArgumentParser:
	parser = argparse.ArgumentParser(description=__doc__)
	add_common_arguments(parser)
	parser.add_argument("--host", default="127.0.0.1", help="UDP echo 服务端地址")
	parser.add_argument("--port", type=int, default=25285, help="UDP echo 服务端端口")
	parser.add_argument("--message-size", type=int, default=64, help="数据报字节数")
	parser.add_argument("--reply-timeout", type=float, default=0.1, help="单个响应超时秒数")
	return parser


def main(arguments: list[str] | None = None) -> int:
	parser = build_parser()
	args = parser.parse_args(arguments)
	if not 1 <= args.port <= 65535 or not 1 <= args.message_size <= 65507 or args.reply_timeout <= 0:
		parser.error("UDP 端口、消息尺寸或响应超时无效")

	result = create_result("udp_request_reply", "sequential_round_trip", args, collect_environment(Path.cwd()))
	result["parameters"].update({
		"host": args.host,
		"port": args.port,
		"message_size_bytes": args.message_size,
		"reply_timeout_seconds": args.reply_timeout,
		"inflight_requests": 1,
	})
	payload = bytes((index + args.seed) % 251 for index in range(args.message_size))
	all_latencies: list[float] = []

	try:
		with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as client:
			client.settimeout(args.reply_timeout)
			endpoint = (socket.gethostbyname(args.host), args.port)
			if args.warmup > 0:
				run_for(client, endpoint, payload, args.warmup, False)
			for iteration in range(1, args.iterations + 1):
				sent, received, elapsed, latencies = run_for(
					client, endpoint, payload, args.duration, True
				)
				all_latencies.extend(latencies)
				result["samples"].append({
					"iteration": iteration,
					"duration_seconds": elapsed,
					"operations": received,
					"packets_sent": sent,
					"packets_received": received,
					"packets_lost": sent - received,
					"packet_loss_ratio": (sent - received) / sent if sent else 0.0,
					"throughput_ops_per_second": received / elapsed,
					"latency_ms": latency_summary(latencies),
				})
	except (OSError, ValueError) as error:
		result["status"] = "error"
		result["error"] = {"exit_code": int(ExitCode.EXECUTION_FAILED), "message": str(error)}
		write_result(args.output, result)
		print(f"UDP request/reply 基准失败：{error}", file=sys.stderr)
		return int(ExitCode.EXECUTION_FAILED)

	throughputs = [sample["throughput_ops_per_second"] for sample in result["samples"]]
	total_sent = sum(sample["packets_sent"] for sample in result["samples"])
	total_lost = sum(sample["packets_lost"] for sample in result["samples"])
	result["summary"] = {
		"throughput_ops_per_second_median": statistics.median(throughputs),
		"packet_loss_ratio": total_lost / total_sent if total_sent else 0.0,
		"latency_ms": latency_summary(all_latencies),
	}
	write_result(args.output, result)
	print(f"UDP request/reply 基准完成：中位包速率 {result['summary']['throughput_ops_per_second_median']:.2f} pkt/s")
	return int(ExitCode.SUCCESS)


if __name__ == "__main__":
	raise SystemExit(main())
