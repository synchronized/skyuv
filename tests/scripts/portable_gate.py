"""跨平台验证 Lua gate 与 C gate 的分帧、回显和关闭行为。"""

import argparse
import socket
import subprocess
import sys
import tempfile
import time


def read_output(output_file) -> str:
	output_file.flush()
	output_file.seek(0)
	return output_file.read().decode("utf-8", errors="replace")


def wait_for_marker(process, output_file, marker: str, timeout: float) -> str:
	deadline = time.monotonic() + timeout
	while time.monotonic() < deadline:
		output = read_output(output_file)
		if marker in output:
			return output
		if process.poll() is not None:
			break
		time.sleep(0.1)
	output = read_output(output_file)
	raise RuntimeError(f"Skynet 未输出就绪标记 {marker!r}。\n{output}")


def make_frame(payload: bytes) -> bytes:
	return len(payload).to_bytes(2, byteorder="big") + payload


def receive_exact(client: socket.socket, size: int) -> bytes:
	result = bytearray()
	while len(result) < size:
		chunk = client.recv(size - len(result))
		if not chunk:
			raise RuntimeError(f"连接提前关闭：收到 {len(result)}/{size} 字节。")
		result.extend(chunk)
	return bytes(result)


def verify_frames(port: int, frames: list[bytes]) -> None:
	with socket.create_connection(("127.0.0.1", port), timeout=5) as client:
		client.settimeout(5)
		client.sendall(frames[0][:1])
		client.sendall(frames[0][1:])
		if len(frames) > 1:
			client.sendall(b"".join(frames[1:]))
		for frame in frames:
			received = receive_exact(client, len(frame))
			if received != frame:
				raise RuntimeError(
					f"gate 回显内容不一致：期望 {frame.hex()}，实际 {received.hex()}。"
				)


def stop_process(process) -> None:
	if process.poll() is not None:
		return
	process.terminate()
	try:
		process.wait(timeout=5)
	except subprocess.TimeoutExpired:
		process.kill()
		process.wait()


def main() -> int:
	parser = argparse.ArgumentParser()
	parser.add_argument("executable")
	parser.add_argument("config")
	parser.add_argument("--kind", choices=("gate", "cgate"), required=True)
	arguments = parser.parse_args()

	if arguments.kind == "gate":
		port = 25284
		ready_marker = "skyuv gate 已就绪"
		frames = [
			make_frame("skyuv-split-跨平台".encode()),
			make_frame(b"skyuv-coalesced-a"),
			make_frame(bytes((0, 1, 2, 0, 254, 255))),
		]
		closed_marker = None
	else:
		port = 25285
		ready_marker = "skyuv C gate 已就绪"
		frames = [make_frame("skyuv-c-gate-跨平台".encode())]
		closed_marker = "skyuv C gate 关闭验证通过"

	with tempfile.TemporaryFile() as output_file:
		process = subprocess.Popen(
			[arguments.executable, arguments.config],
			stdout=output_file,
			stderr=subprocess.STDOUT,
		)
		try:
			wait_for_marker(process, output_file, ready_marker, 8)
			verify_frames(port, frames)
			if closed_marker is not None:
				wait_for_marker(process, output_file, closed_marker, 8)
		except BaseException:
			sys.stderr.write(read_output(output_file))
			raise
		finally:
			stop_process(process)

	print(f"skyuv {arguments.kind} 跨平台验证通过")
	return 0


if __name__ == "__main__":
	raise SystemExit(main())
