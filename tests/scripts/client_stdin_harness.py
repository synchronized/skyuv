"""驱动 client.socket 标准输入测试，并检查 Lua 服务输出。"""

import argparse
import signal
import subprocess
import sys


def main() -> int:
	parser = argparse.ArgumentParser()
	parser.add_argument("executable")
	parser.add_argument("config")
	parser.add_argument("--cwd", default=None)
	parser.add_argument("--allow-abort-after-success", action="store_true")
	arguments = parser.parse_args()

	process = subprocess.Popen(
		[arguments.executable, arguments.config],
		cwd=arguments.cwd,
		stdin=subprocess.PIPE,
		stdout=subprocess.PIPE,
		stderr=subprocess.PIPE,
		text=True,
		encoding="utf-8",
		errors="replace",
	)
	try:
		output, error_output = process.communicate(input="alpha\n\n中文行\n", timeout=15)
	except subprocess.TimeoutExpired:
		process.kill()
		output, error_output = process.communicate()
		sys.stderr.write(output + error_output)
		raise RuntimeError("client.socket stdin 验证超时，节点未正常退出")

	combined_output = output + error_output
	success_marker = "skyuv client.socket stdin 验证通过" in combined_output
	abort_after_success = (
		arguments.allow_abort_after_success
		and success_marker
		and process.returncode == -signal.SIGABRT
	)
	if process.returncode != 0 and not abort_after_success:
		sys.stderr.write(combined_output)
		return process.returncode
	if not success_marker:
		sys.stderr.write(combined_output)
		raise RuntimeError("未观察到 client.socket stdin 验证成功标记")

	print("CLIENT_STDIN lines_empty_utf8_eof")
	return 0


if __name__ == "__main__":
	raise SystemExit(main())
