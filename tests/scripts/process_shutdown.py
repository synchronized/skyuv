"""启动便携 Skynet，并验证外部停止信号会触发正常退出。"""

import argparse
import os
import signal
import subprocess
import sys
import time


def main() -> int:
	parser = argparse.ArgumentParser()
	parser.add_argument("executable")
	parser.add_argument("config")
	parser.add_argument("--windows-console-break", action="store_true")
	arguments = parser.parse_args()
	creation_flags = subprocess.CREATE_NEW_PROCESS_GROUP if arguments.windows_console_break else 0
	process = subprocess.Popen(
		[arguments.executable, arguments.config],
		stdout=subprocess.PIPE,
		stderr=subprocess.STDOUT,
		text=True,
		encoding="utf-8",
		errors="replace",
		creationflags=creation_flags,
	)
	try:
		time.sleep(1)
		if process.poll() is not None:
			raise RuntimeError("Skynet 在停止信号发送前已经退出")
		if arguments.windows_console_break:
			os.kill(process.pid, signal.CTRL_BREAK_EVENT)
		else:
			process.send_signal(signal.SIGTERM)
		output, _ = process.communicate(timeout=10)
	except BaseException:
		process.kill()
		process.wait()
		raise

	if process.returncode != 0:
		sys.stderr.write(output)
		return process.returncode
	if "skyuv 进程停止验证已就绪" not in output:
		sys.stderr.write(output)
		return 1
	print("PROCESS_SHUTDOWN graceful")
	return 0


if __name__ == "__main__":
	raise SystemExit(main())
