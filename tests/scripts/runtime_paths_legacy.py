"""锁定旧入口的日志路径基准，供新目录模式保留兼容行为。"""

from __future__ import annotations

import argparse
import os
from pathlib import Path
import re
import subprocess
import tempfile


def main() -> int:
	parser = argparse.ArgumentParser(description=__doc__)
	parser.add_argument('--executable', type=Path, required=True)
	parser.add_argument('--config-template', type=Path, required=True)
	args = parser.parse_args()
	executable = args.executable.resolve()
	template = args.config_template.read_text(encoding='utf-8')
	markers = ('skyuv logger 文件写入验证：重开前', 'skyuv logger 文件写入验证：重开后')
	with tempfile.TemporaryDirectory(prefix='skyuv legacy paths ') as directory:
		root = Path(directory)
		config_root = root / 'config space'
		config_root.mkdir()
		config = config_root / 'node.conf'
		workdirs = [root / 'caller one', root / 'caller two']
		for workdir in workdirs:
			workdir.mkdir()
		for absolute in (False, True):
			log = root / 'shared log.txt' if absolute else Path('relative log.txt')
			content, count = re.subn(r'^logger = .*$', lambda _: f'logger = "{log.as_posix()}"', template, count=1, flags=re.MULTILINE)
			assert count == 1
			config.write_text(content, encoding='utf-8')
			for workdir in workdirs:
				completed = subprocess.run([str(executable), os.path.relpath(config, workdir)], cwd=workdir, capture_output=True, text=True, encoding='utf-8', errors='replace', timeout=10, check=False)
				assert completed.returncode == 0, completed.stdout + completed.stderr
				target = log if absolute else workdir / log
				text = target.read_text(encoding='utf-8')
				assert all(marker in text for marker in markers), text
				assert not (config_root / 'relative log.txt').exists()
			if absolute:
				assert text.count(markers[0]) == 2, '绝对日志路径未由两个工作目录共享'
	print('旧入口的相对日志工作目录基准、绝对路径和日志重开验证通过')
	return 0


if __name__ == '__main__':
	raise SystemExit(main())
