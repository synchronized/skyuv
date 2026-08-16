"""验证发行归档解压后可独立启动并完成 TCP echo。"""

from __future__ import annotations

import argparse
import shutil
import socket
import subprocess
import tempfile
import time
from pathlib import Path


def parse_arguments() -> argparse.Namespace:
	parser = argparse.ArgumentParser()
	parser.add_argument("--archive-directory", type=Path, required=True)
	parser.add_argument("--archive-extension", required=True)
	parser.add_argument("--executable-name", required=True)
	return parser.parse_args()


def find_package_root(extract_root: Path) -> Path:
	entries = list(extract_root.iterdir())
	if len(entries) != 1 or not entries[0].is_dir():
		raise RuntimeError(f"发行归档顶层目录不唯一：{entries}")
	return entries[0]


def run_smoke(package_root: Path, executable: Path) -> None:
	completed = subprocess.run(
		[executable, "examples/skyuv.conf"],
		cwd=package_root,
		capture_output=True,
		check=False,
		timeout=15,
	)
	output = completed.stdout + completed.stderr
	if completed.returncode != 0 or b"SKYUV_RUNTIME_SMOKE_OK" not in output:
		raise RuntimeError(
			f"解压后的最小节点失败，返回码 {completed.returncode}：\n"
			+ output.decode("utf-8", errors="replace")
		)


def wait_until_ready(process: subprocess.Popen[bytes], log_path: Path) -> None:
	deadline = time.monotonic() + 10
	while time.monotonic() < deadline:
		output = log_path.read_bytes() if log_path.exists() else b""
		if b"SKYUV_RUNTIME_ECHO_READY" in output:
			return
		if process.poll() is not None:
			break
		time.sleep(0.05)
	output = log_path.read_text(encoding="utf-8", errors="replace") if log_path.exists() else ""
	raise RuntimeError(f"解压后的 TCP echo 未就绪：\n{output}")


def receive_exact(client: socket.socket, size: int) -> bytes:
	result = bytearray()
	while len(result) < size:
		chunk = client.recv(size - len(result))
		if not chunk:
			break
		result.extend(chunk)
	return bytes(result)


def run_echo(package_root: Path, executable: Path, log_path: Path) -> None:
	with log_path.open("wb") as log:
		process = subprocess.Popen(
			[executable, "examples/skyuv-echo.conf"],
			cwd=package_root,
			stdout=log,
			stderr=subprocess.STDOUT,
		)
	try:
		wait_until_ready(process, log_path)
		payload = b"skyuv-package-echo\n"
		with socket.create_connection(("127.0.0.1", 25490), timeout=5) as client:
			client.sendall(payload)
			received = receive_exact(client, len(payload))
		if received != payload:
			raise RuntimeError(f"TCP echo 内容不一致：期望 {payload!r}，实际 {received!r}")
		return_code = process.wait(timeout=15)
		output = log_path.read_bytes()
		if return_code != 0 or b"SKYUV_RUNTIME_ECHO_OK" not in output:
			raise RuntimeError(
				f"TCP echo 节点退出异常，返回码 {return_code}：\n"
				+ output.decode("utf-8", errors="replace")
			)
	finally:
		if process.poll() is None:
			process.kill()
			process.wait()


def main() -> int:
	arguments = parse_arguments()
	archives = list(arguments.archive_directory.glob(f"*{arguments.archive_extension}"))
	if len(archives) != 1:
		raise RuntimeError(f"预期一个发行归档，实际为：{archives}")
	with tempfile.TemporaryDirectory(prefix="skyuv extracted package ") as directory:
		extract_root = Path(directory)
		shutil.unpack_archive(archives[0], extract_root)
		package_root = find_package_root(extract_root)
		executable = package_root / "bin" / arguments.executable_name
		if not executable.is_file():
			raise RuntimeError(f"发行归档缺少主程序：{executable}")
		run_smoke(package_root, executable)
		run_echo(package_root, executable, extract_root / "echo.log")
	print("PACKAGE_EXTRACTED_RUNTIME_OK")
	return 0


if __name__ == "__main__":
	raise SystemExit(main())
