"""验证 Unix PID 锁、残留恢复、文件身份和并发启停。"""

from __future__ import annotations

import argparse
import os
from pathlib import Path
import resource
import subprocess
import tempfile
from concurrent.futures import ThreadPoolExecutor


def main() -> int:
	parser = argparse.ArgumentParser(description=__doc__)
	parser.add_argument('executable', type=Path)
	args = parser.parse_args()
	executable = args.executable.resolve()
	with tempfile.TemporaryDirectory(prefix='skyuv pid lifecycle ') as directory:
		root = Path(directory)
		pidfile = root / 'node.pid'

		def run(mode: str = 'cycle', path: Path = pidfile) -> subprocess.CompletedProcess[str]:
			return subprocess.run([str(executable), str(path), mode], capture_output=True,
				text=True, encoding='utf-8', timeout=10)

		def start() -> subprocess.Popen[str]:
			process = subprocess.Popen([str(executable), str(pidfile), 'hold'], stdin=subprocess.PIPE,
				stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, encoding='utf-8')
			assert process.stdout is not None
			assert process.stdout.readline().strip() == 'READY', process.communicate(timeout=5)
			assert int(pidfile.read_text()) == process.pid
			return process

		def stop(process: subprocess.Popen[str]) -> None:
			output, errors = process.communicate('\n', timeout=5)
			assert process.returncode == 0, output + errors

		assert run(path=root / 'missing/node.pid').returncode != 0
		assert not (root / 'missing').exists()
		pidfile.write_text(str(os.getpid()))
		assert run().returncode != 0
		assert int(pidfile.read_text()) == os.getpid(), '覆盖了仍存活的外部进程 PID'
		pidfile.write_text('invalid stale pid\n')
		assert run().returncode == 0
		assert not pidfile.exists()
		assert Path(str(pidfile) + '.lock').is_file()
		assert run('exit').returncode == 3
		assert not pidfile.exists(), '错误退出未清理 PID'

		for iteration in range(10):
			holder = start()
			try:
				with ThreadPoolExecutor(max_workers=8) as pool:
					attempts = list(pool.map(lambda _: run(), range(8)))
				assert all(result.returncode != 0 for result in attempts), '并发实例绕过了锁'
				assert int(pidfile.read_text()) == holder.pid
			finally:
				stop(holder)
			assert not pidfile.exists()

		for iteration in range(10):
			with ThreadPoolExecutor(max_workers=4) as pool:
				results = list(pool.map(lambda _: run(), range(4)))
			assert any(result.returncode == 0 for result in results), '并发启停未产生完成实例'
			assert not pidfile.exists(), '并发退出留下了 PID'

		holder = start()
		holder.kill()
		holder.communicate(timeout=5)
		assert pidfile.exists(), '异常退出夹具未留下 PID'
		assert run().returncode == 0, '异常退出后的残留 PID 未恢复'
		assert not pidfile.exists()

		holder = start()
		try:
			replacement = root / 'replacement.pid'
			replacement.write_text('replacement')
			os.replace(replacement, pidfile)
		finally:
			stop(holder)
		assert pidfile.read_text() == 'replacement', '误删了他人替换的 PID 文件'
		pidfile.unlink()

		target = root / 'symlink target'
		target.write_text('keep')
		pidfile.symlink_to(target)
		assert run().returncode != 0
		assert target.read_text() == 'keep'
		pidfile.unlink()
		guard = Path(str(pidfile) + '.lock')
		guard.unlink()
		guard.symlink_to(target)
		assert run().returncode != 0
		assert target.read_text() == 'keep'
		guard.unlink()

		def limit_descriptors() -> None:
			resource.setrlimit(resource.RLIMIT_NOFILE, (32, 32))

		result = subprocess.run([str(executable), str(pidfile), 'cycle'], capture_output=True,
			text=True, timeout=10, preexec_fn=limit_descriptors)
		assert result.returncode == 0, result.stderr
	print('Unix PID 并发、残留、错误退出、身份保护及 100 次资源释放测试通过')
	return 0


if __name__ == '__main__':
	raise SystemExit(main())
