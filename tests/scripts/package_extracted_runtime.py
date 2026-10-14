"""验证发行归档解压后可独立启动并完成 TCP echo。"""

from __future__ import annotations

import argparse
import hashlib
import os
import signal
import stat
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


def run_smoke(executable: Path, outside_root: Path) -> None:
	completed = subprocess.run(
		[executable, "examples/skyuv.conf"],
		cwd=outside_root,
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


def run_echo(executable: Path, log_path: Path, outside_root: Path) -> None:
	with log_path.open("wb") as log:
		process = subprocess.Popen(
			[executable, "examples/skyuv-echo.conf"],
			cwd=outside_root,
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


def run_runtime_paths(executable: Path, package_root: Path, outside_root: Path) -> None:
	deployment = outside_root / "deployment space"
	config_root = deployment / "config space"
	config_root.mkdir(parents=True)
	log_root = deployment / "log space"
	run_root = deployment / "run space"
	log_file = log_root / "node.log"
	pid_file = run_root / "node.pid"
	stop_file = config_root / "stop flag"
	config = config_root / "node.conf"
	content = (package_root / "examples/skyuv.conf").read_text(encoding="utf-8")
	content = content.replace("logger = nil", 'logger = "node.log"')
	content += '\nstart = "runtime_paths_probe"\n'
	content += 'luaservice = luaservice .. ";" .. skyuv_config_root .. "/?.lua"\n'
	content += 'expected_config = skyuv_config_root\nstop_file = skyuv_config_root .. "/stop flag"\n'
	config.write_text(content, encoding="utf-8")
	(config_root / "runtime_paths_probe.lua").write_text('''local skynet = require "skynet.manager"
local control = require "skyuv.control"
skynet.start(function()
	assert(skynet.getenv("skyuv_config_root") == skynet.getenv("expected_config"))
	skynet.error("SKYUV_RUNTIME_PATHS_BEFORE")
	assert(control.reopen_log())
	skynet.error("SKYUV_RUNTIME_PATHS_AFTER")
	local deadline = skynet.now() + 500
	while true do
		local file = assert(io.open(skynet.getenv("logger")))
		local text = assert(file:read("*a"))
		file:close()
		if text:find("SKYUV_RUNTIME_PATHS_BEFORE", 1, true) and
			text:find("SKYUV_RUNTIME_PATHS_AFTER", 1, true) then break end
		assert(skynet.now() < deadline)
		skynet.sleep(1)
	end
	if skynet.getenv("probe_daemon") then
		while true do
			local file = io.open(skynet.getenv("stop_file"))
			if file then file:close(); break end
			skynet.sleep(1)
		end
	end
	skynet.abort()
end)
''', encoding="utf-8")
	callers = [outside_root / "caller one", outside_root / "caller two"]
	for caller in callers:
		caller.mkdir()
	command = [str(executable), "--runtime-paths", "--log-root", str(log_root),
		"--run-root", str(run_root), str(config)]
	modes = {}
	if os.name != "nt":
		for path in [package_root, *package_root.rglob("*")]:
			if not path.is_symlink():
				modes[path] = stat.S_IMODE(path.stat().st_mode)
				path.chmod(modes[path] & ~0o222)
	try:
		for caller in callers:
			log_file.unlink(missing_ok=True)
			result = subprocess.run(command, cwd=caller, capture_output=True, timeout=15)
			if result.returncode != 0:
				raise RuntimeError((result.stdout + result.stderr).decode("utf-8", errors="replace"))
			text = log_file.read_text(encoding="utf-8")
			assert "SKYUV_RUNTIME_PATHS_BEFORE" in text and "SKYUV_RUNTIME_PATHS_AFTER" in text
			assert not run_root.exists(), "前台模式意外创建运行目录"
		if os.name != "nt":
			log_file.unlink()
			config.write_text(content + 'daemon = "node.pid"\nprobe_daemon = "true"\n', encoding="utf-8")
			with (deployment / "daemon launch.log").open("wb") as launch_log:
				process = subprocess.Popen(command, cwd=callers[0], stdout=launch_log, stderr=launch_log)
				owned_pid = None
				try:
					deadline = time.monotonic() + 10
					while time.monotonic() < deadline:
						if pid_file.is_file() and pid_file.read_text().strip():
							owned_pid = int(pid_file.read_text())
						if owned_pid and log_file.is_file() and "SKYUV_RUNTIME_PATHS_AFTER" in log_file.read_text():
							break
						time.sleep(0.02)
					else:
						raise RuntimeError("解压包 daemon 未就绪")
					conflict = subprocess.run(command, cwd=callers[1], capture_output=True, timeout=10)
					assert conflict.returncode != 0 and int(pid_file.read_text()) == owned_pid
				finally:
					stop_file.touch()
					process.wait(timeout=10)
					deadline = time.monotonic() + 10
					while pid_file.exists() and time.monotonic() < deadline:
						time.sleep(0.02)
					if pid_file.exists() and owned_pid and owned_pid > 0:
						try:
							os.kill(owned_pid, signal.SIGKILL)
						except ProcessLookupError:
							pass
						raise RuntimeError("解压包 daemon 退出后未清理 PID")
				assert Path(str(pid_file) + ".lock").is_file()
	finally:
		for path, mode in modes.items():
			path.chmod(mode)
	print("PACKAGE_RUNTIME_PATHS_OK")


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
		extract_root = Path(directory).resolve()
		shutil.unpack_archive(archive, extract_root)
		package_root = find_package_root(extract_root)
		verify_manifest(package_root, arguments.project_version, arguments.allocator)
		executable = package_root / "bin" / arguments.executable_name
		if not executable.is_file():
			raise RuntimeError(f"发行归档缺少主程序：{executable}")
		run_smoke(executable, extract_root)
		run_echo(executable, extract_root / "echo.log", extract_root)
		run_runtime_paths(executable, package_root, extract_root)
	print("PACKAGE_EXTRACTED_RUNTIME_OK")
	return 0


if __name__ == "__main__":
	raise SystemExit(main())
