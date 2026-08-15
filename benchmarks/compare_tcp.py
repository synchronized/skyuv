"""使用相同客户端和参数依次运行 skyuv 与原版 Skynet TCP 基准。"""

from __future__ import annotations

import argparse
import json
import os
import socket
import subprocess
import sys
import time
from pathlib import Path

from process_metrics import ProcessMetrics


def wait_until_ready(process: subprocess.Popen[bytes], host: str, port: int, timeout: float) -> None:
	deadline = time.monotonic() + timeout
	while time.monotonic() < deadline:
		if process.poll() is not None:
			raise RuntimeError(f"服务端在就绪前退出，退出码 {process.returncode}")
		try:
			with socket.create_connection((host, port), timeout=0.2):
				return
		except OSError:
			time.sleep(0.05)
	raise TimeoutError(f"等待服务端 {host}:{port} 就绪超时")


def stop_process(process: subprocess.Popen[bytes]) -> None:
	if process.poll() is not None:
		return
	process.terminate()
	try:
		process.wait(timeout=5.0)
	except subprocess.TimeoutExpired:
		process.kill()
		process.wait(timeout=5.0)


def main() -> int:
	parser = argparse.ArgumentParser(description=__doc__)
	parser.add_argument("--skyuv-executable", type=Path, required=True)
	parser.add_argument("--skyuv-config", type=Path, required=True)
	parser.add_argument("--upstream-executable", type=Path, required=True)
	parser.add_argument("--upstream-config", type=Path, required=True)
	parser.add_argument("--build-type", required=True)
	parser.add_argument("--allocator", required=True)
	parser.add_argument("--compiler", required=True)
	parser.add_argument(
		"--scenario", choices=("tcp_echo", "tcp_short_connection", "tcp_backpressure"),
		default="tcp_echo",
		help="要运行的 TCP 客户端场景",
	)
	parser.add_argument("--host", default="127.0.0.1")
	parser.add_argument("--port", type=int, default=25281)
	parser.add_argument("--message-size", type=int, default=64)
	parser.add_argument("--initial-pause", type=float, default=0.2)
	parser.add_argument("--read-delay", type=float, default=0.005)
	parser.add_argument("--read-size", type=int, default=4096)
	parser.add_argument("--require-log-marker", help="双方服务端日志都必须包含的标记")
	parser.add_argument("--warmup", type=float, default=2.0)
	parser.add_argument("--duration", type=float, default=10.0)
	parser.add_argument("--iterations", type=int, default=5)
	parser.add_argument("--startup-timeout", type=float, default=10.0)
	parser.add_argument("--output-directory", type=Path, required=True)
	args = parser.parse_args()
	if (
		not 1 <= args.port <= 65535
		or args.message_size <= 0
		or args.initial_pause < 0
		or args.read_delay < 0
		or args.read_size <= 0
		or args.startup_timeout <= 0
	):
		parser.error("端口、消息尺寸和启动超时必须有效")

	args.output_directory.mkdir(parents=True, exist_ok=True)
	runner = Path(__file__).with_name(f"{args.scenario}.py")
	result_prefix = args.scenario.replace("_", "-")
	results = []
	for implementation, executable, config in (
		("skyuv", args.skyuv_executable, args.skyuv_config),
		("upstream", args.upstream_executable, args.upstream_config),
	):
		config = config.resolve()
		log_path = args.output_directory / f"{result_prefix}-{implementation}-server.log"
		output = args.output_directory / f"{result_prefix}-{implementation}.json"
		environment = os.environ.copy()
		environment["SKYUV_BENCHMARK_MESSAGE_SIZE"] = str(args.message_size)
		environment["SKYUV_BENCHMARK_PORT"] = str(args.port)
		try:
			with log_path.open("wb") as log:
				process = subprocess.Popen(
					[str(executable.resolve()), config.name], cwd=config.parent, env=environment,
					stdout=log, stderr=subprocess.STDOUT,
				)
				monitor = ProcessMetrics(process.pid)
				monitor.start()
				try:
					wait_until_ready(process, args.host, args.port, args.startup_timeout)
					command = [
						sys.executable, str(runner),
						"--implementation", implementation,
						"--build-type", args.build_type,
						"--allocator", args.allocator,
						"--compiler", args.compiler,
						"--host", args.host,
						"--port", str(args.port),
						"--warmup", str(args.warmup),
						"--duration", str(args.duration),
						"--iterations", str(args.iterations),
						"--output", str(output.resolve()),
					]
					if args.scenario == "tcp_backpressure":
						command.extend([
							"--initial-pause", str(args.initial_pause),
							"--read-delay", str(args.read_delay),
							"--read-size", str(args.read_size),
						])
					else:
						command.extend(["--message-size", str(args.message_size)])
					completed = subprocess.run(command, check=False)
					if completed.returncode != 0:
						return completed.returncode
				finally:
					metrics = monitor.stop()
					stop_process(process)
		except (OSError, RuntimeError, TimeoutError, subprocess.TimeoutExpired) as error:
			print(f"{implementation} {args.scenario} 配对运行失败：{error}", file=sys.stderr)
			return 3
		log_text = log_path.read_text(encoding="utf-8", errors="replace")
		marker_found = args.require_log_marker is None or args.require_log_marker in log_text
		if not marker_found:
			print(f"{implementation} 服务端日志缺少标记：{args.require_log_marker}", file=sys.stderr)
			return 3
		results.append({
			"implementation": implementation,
			"result": output.name,
			"server_log": log_path.name,
			"required_log_marker_found": marker_found,
			"process_metrics": metrics,
		})

	manifest = {
		"benchmark": args.scenario,
		"paired": True,
		"parameters": {
			"host": args.host,
			"port": args.port,
			"message_size_bytes": args.message_size,
			"warmup_seconds": args.warmup,
			"duration_seconds": args.duration,
			"iterations": args.iterations,
			"build_type": args.build_type,
			"allocator": args.allocator,
			"compiler": args.compiler,
		},
		"results": results,
	}
	if args.scenario == "tcp_backpressure":
		manifest["parameters"].update({
			"initial_pause_seconds": args.initial_pause,
			"read_delay_seconds": args.read_delay,
			"read_size_bytes": args.read_size,
			"required_log_marker": args.require_log_marker,
		})
	(args.output_directory / f"{result_prefix}-comparison.json").write_text(
		json.dumps(manifest, ensure_ascii=False, indent=2, sort_keys=True) + "\n", encoding="utf-8"
	)
	print(f"{args.scenario} 配对对照完成")
	return 0


if __name__ == "__main__":
	raise SystemExit(main())
