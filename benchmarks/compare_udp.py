"""使用相同客户端和参数依次运行 skyuv 与原版 Skynet UDP 基准。"""

from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys
import time
from pathlib import Path


def wait_for_marker(
	process: subprocess.Popen[bytes], log_path: Path, marker: str, timeout: float
) -> None:
	deadline = time.monotonic() + timeout
	while time.monotonic() < deadline:
		if process.poll() is not None:
			raise RuntimeError(f"服务端在就绪前退出，退出码 {process.returncode}")
		if log_path.exists() and marker in log_path.read_text(encoding="utf-8", errors="replace"):
			return
		time.sleep(0.05)
	raise TimeoutError(f"等待服务端就绪标记超时：{marker}")


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
	parser.add_argument("--host", default="127.0.0.1")
	parser.add_argument("--port", type=int, default=25285)
	parser.add_argument("--message-size", type=int, default=64)
	parser.add_argument("--reply-timeout", type=float, default=0.1)
	parser.add_argument("--warmup", type=float, default=2.0)
	parser.add_argument("--duration", type=float, default=10.0)
	parser.add_argument("--iterations", type=int, default=5)
	parser.add_argument("--startup-timeout", type=float, default=10.0)
	parser.add_argument("--ready-marker", default="SKYUV_UDP_BENCHMARK_READY")
	parser.add_argument("--output-directory", type=Path, required=True)
	args = parser.parse_args()
	if (
		not 1 <= args.port <= 65535
		or not 1 <= args.message_size <= 65507
		or args.reply_timeout <= 0
		or args.startup_timeout <= 0
	):
		parser.error("UDP 端口、消息尺寸和超时必须有效")

	args.output_directory.mkdir(parents=True, exist_ok=True)
	runner = Path(__file__).with_name("udp_request_reply.py")
	results = []
	for implementation, executable, config in (
		("skyuv", args.skyuv_executable, args.skyuv_config),
		("upstream", args.upstream_executable, args.upstream_config),
	):
		config = config.resolve()
		log_path = args.output_directory / f"udp-request-reply-{implementation}-server.log"
		output = args.output_directory / f"udp-request-reply-{implementation}.json"
		environment = os.environ.copy()
		environment["SKYUV_BENCHMARK_PORT"] = str(args.port)
		try:
			with log_path.open("wb") as log:
				process = subprocess.Popen(
					[str(executable.resolve()), config.name], cwd=config.parent, env=environment,
					stdout=log, stderr=subprocess.STDOUT,
				)
				try:
					wait_for_marker(process, log_path, args.ready_marker, args.startup_timeout)
					completed = subprocess.run([
						sys.executable, str(runner),
						"--implementation", implementation,
						"--build-type", args.build_type,
						"--allocator", args.allocator,
						"--compiler", args.compiler,
						"--host", args.host,
						"--port", str(args.port),
						"--message-size", str(args.message_size),
						"--reply-timeout", str(args.reply_timeout),
						"--warmup", str(args.warmup),
						"--duration", str(args.duration),
						"--iterations", str(args.iterations),
						"--output", str(output.resolve()),
					], check=False)
					if completed.returncode != 0:
						return completed.returncode
				finally:
					stop_process(process)
		except (OSError, RuntimeError, TimeoutError, subprocess.TimeoutExpired) as error:
			print(f"{implementation} UDP 配对运行失败：{error}", file=sys.stderr)
			return 3
		results.append({"implementation": implementation, "result": output.name, "server_log": log_path.name})

	manifest = {
		"benchmark": "udp_request_reply",
		"paired": True,
		"parameters": {
			"host": args.host,
			"port": args.port,
			"message_size_bytes": args.message_size,
			"reply_timeout_seconds": args.reply_timeout,
			"warmup_seconds": args.warmup,
			"duration_seconds": args.duration,
			"iterations": args.iterations,
			"build_type": args.build_type,
			"allocator": args.allocator,
			"compiler": args.compiler,
		},
		"results": results,
	}
	(args.output_directory / "udp-request-reply-comparison.json").write_text(
		json.dumps(manifest, ensure_ascii=False, indent=2, sort_keys=True) + "\n", encoding="utf-8"
	)
	print("UDP request/reply 配对对照完成")
	return 0


if __name__ == "__main__":
	raise SystemExit(main())
