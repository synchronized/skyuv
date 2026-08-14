"""验证 logger 在跨平台文件路径上写入并完成显式重开。"""

import argparse
from pathlib import Path
import subprocess
import sys


def main() -> int:
	parser = argparse.ArgumentParser()
	parser.add_argument("executable")
	parser.add_argument("config")
	parser.add_argument("log_file")
	arguments = parser.parse_args()

	log_file = Path(arguments.log_file)
	log_file.parent.mkdir(parents=True, exist_ok=True)
	log_file.unlink(missing_ok=True)
	try:
		result = subprocess.run(
			[arguments.executable, arguments.config],
			stdout=subprocess.PIPE,
			stderr=subprocess.STDOUT,
			text=True,
			encoding="utf-8",
			errors="replace",
			timeout=10,
			check=False,
		)
		if result.returncode != 0:
			sys.stderr.write(result.stdout)
			return result.returncode
		if not log_file.is_file():
			raise RuntimeError(f"logger 未创建目标文件：{log_file}")
		content = log_file.read_text(encoding="utf-8")
		for marker in (
			"skyuv logger 文件写入验证：重开前",
			"skyuv logger 文件写入验证：重开后",
		):
			if marker not in content:
				raise RuntimeError(f"日志缺少标记 {marker!r}：\n{content}")
	finally:
		log_file.unlink(missing_ok=True)

	print("skyuv logger 文件路径与重开验证通过")
	return 0


if __name__ == "__main__":
	raise SystemExit(main())
