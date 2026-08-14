"""启动便携 Skynet，并等待 Lua 夹具输出成功标记。"""

import argparse
import subprocess
import sys
import tempfile
import time


def read_output(output_file) -> str:
	output_file.flush()
	output_file.seek(0)
	return output_file.read().decode("utf-8", errors="replace")


def main() -> int:
	parser = argparse.ArgumentParser()
	parser.add_argument("executable")
	parser.add_argument("config")
	parser.add_argument("--marker", default="skyuv CMake 启动验证通过")
	arguments = parser.parse_args()

	with tempfile.TemporaryFile() as output_file:
		process = subprocess.Popen(
			[arguments.executable, arguments.config],
			stdout=output_file,
			stderr=subprocess.STDOUT,
		)
		try:
			deadline = time.monotonic() + 8
			while time.monotonic() < deadline:
				output = read_output(output_file)
				if arguments.marker in output:
					print(arguments.marker)
					return 0
				if process.poll() is not None:
					break
				time.sleep(0.1)
			sys.stderr.write(read_output(output_file))
			raise RuntimeError(f"未观察到成功标记：{arguments.marker}")
		finally:
			if process.poll() is None:
				process.kill()
				process.wait()


if __name__ == "__main__":
	raise SystemExit(main())
