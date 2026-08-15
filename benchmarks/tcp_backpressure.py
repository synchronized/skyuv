"""TCP 慢接收端与服务端写队列背压基准。"""

from __future__ import annotations

import argparse
import socket
import statistics
import sys
import time
from pathlib import Path

from protocol import ExitCode, add_common_arguments, collect_environment, create_result, write_result


def run_for(
	host: str,
	port: int,
	duration: float,
	initial_pause: float,
	read_delay: float,
	read_size: int,
	connect_timeout: float,
) -> tuple[int, int, float]:
	reads = 0
	bytes_received = 0
	with socket.create_connection((host, port), timeout=connect_timeout) as client:
		client.settimeout(connect_timeout)
		client.sendall(b"start\n")
		if initial_pause > 0:
			time.sleep(initial_pause)
		started = time.perf_counter()
		deadline = started + duration
		while time.perf_counter() < deadline:
			data = client.recv(read_size)
			if not data:
				raise ConnectionError("服务端在测量结束前关闭连接")
			if data.count(data[:1]) != len(data) or data[:1] != b"b":
				raise ValueError("背压数据内容不一致")
			reads += 1
			bytes_received += len(data)
			if read_delay > 0:
				time.sleep(read_delay)
	return reads, bytes_received, time.perf_counter() - started


def build_parser() -> argparse.ArgumentParser:
	parser = argparse.ArgumentParser(description=__doc__)
	add_common_arguments(parser)
	parser.add_argument("--host", default="127.0.0.1", help="背压服务端地址")
	parser.add_argument("--port", type=int, default=25284, help="背压服务端端口")
	parser.add_argument("--initial-pause", type=float, default=0.2, help="连接后暂停读取秒数")
	parser.add_argument("--read-delay", type=float, default=0.005, help="每次读取后的等待秒数")
	parser.add_argument("--read-size", type=int, default=4096, help="每次读取的最大字节数")
	parser.add_argument("--connect-timeout", type=float, default=5.0, help="连接和读取超时秒数")
	return parser


def main(arguments: list[str] | None = None) -> int:
	parser = build_parser()
	args = parser.parse_args(arguments)
	if (
		not 1 <= args.port <= 65535
		or args.initial_pause < 0
		or args.read_delay < 0
		or args.read_size <= 0
		or args.connect_timeout <= 0
	):
		parser.error("背压参数无效")

	result = create_result("tcp_backpressure", "slow_receiver", args, collect_environment(Path.cwd()))
	result["parameters"].update({
		"host": args.host,
		"port": args.port,
		"initial_pause_seconds": args.initial_pause,
		"read_delay_seconds": args.read_delay,
		"read_size_bytes": args.read_size,
	})

	try:
		if args.warmup > 0:
			run_for(
				args.host, args.port, args.warmup, 0.0, 0.0, args.read_size, args.connect_timeout
			)
		for iteration in range(1, args.iterations + 1):
			reads, bytes_received, elapsed = run_for(
				args.host,
				args.port,
				args.duration,
				args.initial_pause,
				args.read_delay,
				args.read_size,
				args.connect_timeout,
			)
			result["samples"].append({
				"iteration": iteration,
				"duration_seconds": elapsed,
				"operations": reads,
				"bytes_received": bytes_received,
				"throughput_bytes_per_second": bytes_received / elapsed,
			})
	except (OSError, ValueError) as error:
		result["status"] = "error"
		result["error"] = {"exit_code": int(ExitCode.EXECUTION_FAILED), "message": str(error)}
		write_result(args.output, result)
		print(f"TCP 背压基准失败：{error}", file=sys.stderr)
		return int(ExitCode.EXECUTION_FAILED)

	throughputs = [sample["throughput_bytes_per_second"] for sample in result["samples"]]
	result["summary"] = {
		"throughput_bytes_per_second_median": statistics.median(throughputs),
		"bytes_received_total": sum(sample["bytes_received"] for sample in result["samples"]),
	}
	write_result(args.output, result)
	print(f"TCP 背压基准完成：中位接收吞吐 {result['summary']['throughput_bytes_per_second_median']:.2f} B/s")
	return int(ExitCode.SUCCESS)


if __name__ == "__main__":
	raise SystemExit(main())
