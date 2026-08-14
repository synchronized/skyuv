"""驱动 client.socket 回环测试，并检查 Lua 服务输出。"""

import argparse
import signal
import socket
import subprocess
import sys
import threading


def serve(listener: socket.socket, errors: list[BaseException]) -> None:
	try:
		connection, _ = listener.accept()
		with connection:
			payload = connection.recv(4096)
			if not payload:
				raise RuntimeError("client.socket 未发送测试数据")
			connection.sendall(payload)
			while connection.recv(4096):
				pass
	except BaseException as error:  # 测试线程必须把失败传回主线程。
		errors.append(error)


def main() -> int:
	parser = argparse.ArgumentParser()
	parser.add_argument("executable")
	parser.add_argument("config")
	parser.add_argument("--cwd", default=None)
	parser.add_argument("--allow-timeout-after-success", action="store_true")
	parser.add_argument("--allow-abort-after-success", action="store_true")
	arguments = parser.parse_args()

	errors: list[BaseException] = []
	with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as listener:
		listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
		listener.bind(("127.0.0.1", 25286))
		listener.listen(1)
		thread = threading.Thread(target=serve, args=(listener, errors), daemon=True)
		thread.start()
		process = subprocess.Popen(
			[arguments.executable, arguments.config],
			cwd=arguments.cwd,
			stdin=subprocess.PIPE,
			stdout=subprocess.PIPE,
			stderr=subprocess.PIPE,
			text=True,
		)
		try:
			result = process.wait(timeout=15)
		except subprocess.TimeoutExpired:
			process.kill()
			process.wait()
			result = None if arguments.allow_timeout_after_success else process.returncode
		assert process.stdout is not None
		assert process.stderr is not None
		output = process.stdout.read() + process.stderr.read()
		success_marker = "skyuv client.socket 回环验证通过" in output
		abort_after_success = (
			arguments.allow_abort_after_success
			and success_marker
			and result == -signal.SIGABRT
		)
		if result not in (0, None) and not abort_after_success:
			sys.stderr.write(output)
			return result
		thread.join(timeout=2)

	if errors:
		raise errors[0]
	if not success_marker:
		sys.stderr.write(output)
		return 1
	print("CLIENT_SOCKET connect_send_recv_shutdown_peer_close_refused")
	return 0


if __name__ == "__main__":
	raise SystemExit(main())
