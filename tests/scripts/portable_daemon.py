"""验证真实 Unix daemon 的跨目录 PID、冲突、正常关闭及 logger 失败清理。"""

import argparse
import fcntl
import json
from pathlib import Path
import re
import subprocess
import tempfile
import time


def wait_for(condition, message: str) -> None:
	deadline = time.monotonic() + 10
	while time.monotonic() < deadline:
		if condition():
			return
		time.sleep(0.02)
	raise RuntimeError(message)


def main() -> int:
	parser = argparse.ArgumentParser(description=__doc__)
	parser.add_argument('--executable', type=Path, required=True)
	parser.add_argument('--config-template', type=Path, required=True)
	args = parser.parse_args()
	template = args.config_template.read_text()
	with tempfile.TemporaryDirectory(prefix='skyuv daemon runtime ') as directory:
		root = Path(directory).resolve()
		config_root = root / 'config space'
		install = root / 'install space'
		callers = [root / 'caller one', root / 'caller two']
		for path in [config_root, install, *callers]:
			path.mkdir()
		stop = config_root / 'stop flag'
		pidfile = config_root / 'run/node.pid'
		guard = Path(str(pidfile) + '.lock')
		logfile = config_root / 'log/node.log'
		config = config_root / 'node.conf'
		(config_root / 'runtime_pid_probe.lua').write_text('''local skynet = require "skynet.manager"
skynet.start(function()
\tskynet.error("PID_RUNTIME_READY")
\twhile true do
\t\tlocal file = io.open(skynet.getenv("probe_stop"))
\t\tif file then file:close(); break end
\t\tskynet.sleep(1)
\tend
\tskynet.abort()
end)
''')
		content = re.sub(r'^logger = .*$', 'logger = "node.log"', template, count=1, flags=re.MULTILINE)
		content += '\nstart = "runtime_pid_probe"\n'
		content += 'luaservice = luaservice .. ";" .. skyuv_config_root .. "/?.lua"\n'
		content += 'daemon = "node.pid"\nprobe_stop = ' + json.dumps(str(stop)) + '\n'
		config.write_text(content)
		command = [str(args.executable.resolve()), '--runtime-paths', '--install-root', str(install), str(config)]
		for iteration, caller in enumerate(callers):
			stop.unlink(missing_ok=True)
			logfile.unlink(missing_ok=True)
			with (root / 'launch.log').open('w') as launch_log:
				process = subprocess.Popen(command, cwd=caller, stdout=launch_log, stderr=launch_log)
				try:
					wait_for(lambda: logfile.is_file() and 'PID_RUNTIME_READY' in logfile.read_text(),
						'daemon 服务未就绪')
					assert pidfile.is_file()
					pid = int(pidfile.read_text())
					assert pid > 0
					conflict = subprocess.run(command, cwd=callers[1 - iteration], capture_output=True,
						text=True, encoding='utf-8', timeout=5)
					assert conflict.returncode != 0 and '无法锁定 PID' in conflict.stderr, conflict.stderr
					assert int(pidfile.read_text()) == pid
				finally:
					stop.touch()
					process.wait(timeout=10)
				wait_for(lambda: not pidfile.exists(), 'daemon 正常退出未清理 PID')
				assert guard.is_file()

		# Linux 的 daemon 父进程可先返回 0；必须等待子进程释放锁并核对 PID 清理。
		config.write_text(content.replace('logger = "node.log"', 'logger = "missing/log.txt"'))
		with (root / 'launch failure.log').open('w') as launch_log:
			process = subprocess.Popen(command, cwd=callers[0], stdout=launch_log, stderr=launch_log)
			process.wait(timeout=10)
			def unlocked() -> bool:
				with guard.open() as file:
					try:
						fcntl.flock(file, fcntl.LOCK_EX | fcntl.LOCK_NB)
						return not pidfile.exists()
					except BlockingIOError:
						return False
			wait_for(unlocked, 'logger 启动失败后 PID 或锁未释放')
	print('真实 daemon 跨目录 PID、冲突及正常/错误退出清理验证通过')
	return 0


if __name__ == '__main__':
	raise SystemExit(main())
