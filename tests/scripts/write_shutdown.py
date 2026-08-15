"""验证存在挂起 socket 写请求时，Skynet 仍可响应外部停止信号。"""

import argparse
import os
import signal
import socket
import subprocess
import sys
import time


def main() -> int:
	parser = argparse.ArgumentParser()
	parser.add_argument("executable")
	parser.add_argument("config")
	parser.add_argument("--windows-console-break", action="store_true")
	args = parser.parse_args()
	flags = subprocess.CREATE_NEW_PROCESS_GROUP if args.windows_console_break else 0
	process = subprocess.Popen(
		[args.executable, args.config], stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
		text=True, encoding="utf-8", errors="replace", creationflags=flags,
	)
	client = None
	try:
		deadline = time.monotonic() + 5
		while client is None:
			try:
				client = socket.create_connection(("127.0.0.1", 25290), timeout=0.5)
			except OSError:
				if process.poll() is not None or time.monotonic() >= deadline:
					raise RuntimeError("挂起写测试服务未就绪")
				time.sleep(0.05)
		client.sendall(b"start\n")
		time.sleep(1)
		if process.poll() is not None:
			raise RuntimeError("Skynet 在停止信号发送前已经退出")
		if args.windows_console_break:
			os.kill(process.pid, signal.CTRL_BREAK_EVENT)
		else:
			process.send_signal(signal.SIGTERM)
		output, _ = process.communicate(timeout=10)
	except BaseException:
		process.kill()
		process.wait()
		raise
	finally:
		if client is not None:
			client.close()
	if "SKYUV_WRITE_SHUTDOWN_QUEUED" not in output or process.returncode != 0:
		sys.stderr.write(output)
		return process.returncode or 1
	print("WRITE_SHUTDOWN pending_writes_retired")
	return 0


if __name__ == "__main__":
	raise SystemExit(main())
