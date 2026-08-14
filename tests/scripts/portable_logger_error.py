"""验证 logger 无法创建目标文件时明确失败。"""

import argparse
from pathlib import Path
import shutil
import subprocess


def main() -> int:
	parser = argparse.ArgumentParser()
	parser.add_argument("executable")
	parser.add_argument("config")
	parser.add_argument("log_file")
	arguments = parser.parse_args()

	log_file = Path(arguments.log_file)
	missing_parent = log_file.parent
	shutil.rmtree(missing_parent, ignore_errors=True)
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
		if result.returncode == 0:
			raise RuntimeError("logger 路径不可创建时 Skynet 错误地返回成功")
		if "Can't launch logger service" not in result.stdout:
			raise RuntimeError(f"logger 启动失败缺少明确诊断：\n{result.stdout}")
		if log_file.exists():
			raise RuntimeError(f"logger 错误路径意外创建了文件：{log_file}")
	finally:
		shutil.rmtree(missing_parent, ignore_errors=True)

	print("skyuv logger 不可创建路径验证通过")
	return 0


if __name__ == "__main__":
	raise SystemExit(main())
