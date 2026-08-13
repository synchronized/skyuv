"""驱动 client.socket 回环测试，并检查 Lua 服务输出。"""

import argparse
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
	arguments = parser.parse_args()

	errors: list[BaseException] = []
	with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as listener:
		listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
		listener.bind(("127.0.0.1", 25286))
		listener.listen(1)
		thread = threading.Thread(target=serve, args=(listener, errors), daemon=True)
		thread.start()
		try:
			result = subprocess.run(
				[arguments.executable, arguments.config],
				cwd=arguments.cwd,
				capture_output=True,
				text=True,
				timeout=15,
				check=False,
			)
	except subprocess.TimeoutExpired as error:
			output = (error.stdout or "") + (error.stderr or "")
			if isinstance(output, bytes):
				output = output.decode(errors="replace")
	else:
		output = result.stdout + result.stderr
		if result.returncode != 0:
			sys.stderr.write(output)
			return result.returncode
		thread.join(timeout=2)

	if errors:
		raise errors[0]
	if "skyuv client.socket 回环验证通过" not in output:
		sys.stderr.write(output)
		return 1
	print("CLIENT_SOCKET connect_send_recv_shutdown_peer_close_refused")
	return 0


if __name__ == "__main__":
	raise SystemExit(main())
