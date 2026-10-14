"""验证日志重开失败仍可写入旧句柄，恢复后可切换到新文件。"""

import argparse
from pathlib import Path
import subprocess
import tempfile


def main() -> int:
	parser = argparse.ArgumentParser(description=__doc__)
	parser.add_argument('executable', type=Path)
	args = parser.parse_args()
	with tempfile.TemporaryDirectory(prefix='skyuv logger reopen ') as directory:
		root = Path(directory)
		current = root / 'current.log'
		recovery = root / 'recovered.log'
		for attempt in range(20):
			result = subprocess.run([str(args.executable.resolve()), str(current),
				str(root / 'missing/log.txt'), str(recovery)], capture_output=True,
				text=True, encoding='utf-8', errors='replace', timeout=5)
			assert result.returncode == 0, result.stdout + result.stderr
			assert '日志重开失败' in result.stderr
			assert str(root / 'missing/log.txt') in result.stderr
		assert current.read_text() == 'before failure\nafter failure\n' * 20
		assert recovery.read_text() == 'after recovery\n' * 20
		assert not (root / 'missing').exists()
	print('日志重开失败保留与恢复切换的 20 次重复测试通过')
	return 0


if __name__ == '__main__':
	raise SystemExit(main())
