"""验证发行归档解压后可独立启动并完成 TCP echo。"""

from __future__ import annotations

import argparse
import hashlib
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
	parser.add_argument("--allocator", choices=("jemalloc", "system"), required=True)
	parser.add_argument("--executable-name", required=True)
	parser.add_argument("--project-version", required=True)
	return parser.parse_args()


def find_package_root(extract_root: Path) -> Path:
	entries = list(extract_root.iterdir())
	if len(entries) != 1 or not entries[0].is_dir():
		raise RuntimeError(f"发行归档顶层目录不唯一：{entries}")
	return entries[0]


def verify_checksum(archive: Path) -> None:
	checksum_path = archive.with_name(f"{archive.name}.sha256")
	if not checksum_path.is_file():
		raise RuntimeError(f"发行归档缺少 SHA-256 文件：{checksum_path}")
	fields = checksum_path.read_text(encoding="utf-8").strip().split()
	if len(fields) != 2 or fields[1] != archive.name:
		raise RuntimeError(f"SHA-256 文件格式或文件名错误：{fields}")
	actual = hashlib.sha256(archive.read_bytes()).hexdigest()
	if fields[0].lower() != actual:
		raise RuntimeError(f"SHA-256 不一致：期望 {fields[0]}，实际 {actual}")


def verify_manifest(package_root: Path, project_version: str, allocator: str) -> None:
	license_root = package_root / "licenses"
	required_licenses = {
		"LPeg-LICENSE",
		"Lua-LICENSE",
		"libuv-LICENSE",
		"lua-md5-LICENSE",
		"skynet-LICENSE",
		"skyuv-LICENSE",
	}
	if allocator == "jemalloc":
		required_licenses.add("jemalloc-COPYING")
	for name in required_licenses:
		if not (license_root / name).is_file():
			raise RuntimeError(f"发行归档缺少许可证：{name}")
	if allocator == "system" and (license_root / "jemalloc-COPYING").exists():
		raise RuntimeError("system allocator 发行包错误包含 jemalloc 许可证")

	manifest_path = license_root / "versions.txt"
	manifest = dict(
		line.split("=", maxsplit=1)
		for line in manifest_path.read_text(encoding="utf-8").splitlines()
		if line
	)
	required_versions = {
		"allocator",
		"jemalloc",
		"libuv",
		"libuv_commit",
		"lpeg",
		"lua",
		"lua-md5",
		"skynet",
		"skynet_commit",
		"skyuv",
	}
	if not required_versions.issubset(manifest) or any(
		not manifest[key] for key in required_versions
	):
		raise RuntimeError(f"第三方版本清单不完整：{manifest}")
	if manifest["skyuv"] != project_version:
		raise RuntimeError(f"skyuv 版本不一致：{manifest['skyuv']} != {project_version}")
	if manifest["allocator"] != allocator:
		raise RuntimeError(f"分配器清单不一致：{manifest['allocator']} != {allocator}")


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
	archive = archives[0]
	if f"-{arguments.project_version}-" not in archive.name:
		raise RuntimeError(f"发行归档文件名缺少项目版本：{archive.name}")
	verify_checksum(archive)
	with tempfile.TemporaryDirectory(prefix="skyuv extracted package ") as directory:
		extract_root = Path(directory)
		shutil.unpack_archive(archive, extract_root)
		package_root = find_package_root(extract_root)
		verify_manifest(package_root, arguments.project_version, arguments.allocator)
		executable = package_root / "bin" / arguments.executable_name
		if not executable.is_file():
			raise RuntimeError(f"发行归档缺少主程序：{executable}")
		run_smoke(package_root, executable)
		run_echo(package_root, executable, extract_root / "echo.log")
	print("PACKAGE_EXTRACTED_RUNTIME_OK")
	return 0


if __name__ == "__main__":
	raise SystemExit(main())
