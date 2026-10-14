"""验证新入口的 include、目录覆盖、日志重开与启动错误路径。"""

from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile


def lua_string(value: str | Path) -> str:
	return json.dumps(str(value).replace('\\', '/'), ensure_ascii=False)


def main() -> int:
	parser = argparse.ArgumentParser(description=__doc__)
	parser.add_argument('--executable', type=Path, required=True)
	parser.add_argument('--config-template', type=Path, required=True)
	args = parser.parse_args()
	executable = args.executable.resolve()
	template = args.config_template.read_text(encoding='utf-8')
	with tempfile.TemporaryDirectory(prefix='skyuv runtime paths ') as directory:
		# macOS 临时目录含符号链接，预期路径必须与入口的 realpath 一致。
		root = Path(directory).resolve()
		install = root / 'install space'
		config_root = root / ('config space' if os.name == 'nt' else 'config "quote space')
		callers = [root / 'caller one', root / 'caller two']
		for path in [install, config_root / 'nested', *callers]:
			path.mkdir(parents=True)
		config = config_root / 'node.conf'
		options = config_root / 'nested/options.conf'
		environment = os.environ.copy()
		for name in ('SKYUV_LOG_ROOT', 'SKYUV_RUN_ROOT'):
			environment.pop(name, None)

		def run(extra: list[str] | None = None, env: dict[str, str] | None = None,
				caller: Path | None = None, success: bool = True, absolute_config: bool = False) -> str:
			working = caller or callers[0]
			command = [str(executable), '--runtime-paths', '--install-root', str(install)]
			command += extra or []
			command += [str(config) if absolute_config else os.path.relpath(config, working)]
			result = subprocess.run(command, cwd=working, env=env or environment,
				capture_output=True, text=True, encoding='utf-8', errors='replace', timeout=10)
			output = result.stdout + result.stderr
			assert (result.returncode == 0) == success, output
			return output

		def configure(logger: str | Path | None = 'node.log', values: str = '') -> None:
			log_value = 'nil' if logger is None else lua_string(logger)
			content = re.sub(r'^logger = .*$', lambda _: f'logger = {log_value}',
				template, count=1, flags=re.MULTILINE)
			config.write_text(content + '\ninclude("nested/options.conf")\n', encoding='utf-8')
			options.write_text(values, encoding='utf-8')

		configure()
		for caller in callers:
			output = run(caller=caller)
			assert f'config_root={config_root.as_posix()}' in output, output
		log = config_root / 'log/node.log'
		text = log.read_text(encoding='utf-8')
		assert text.count('重开前') == 2 and text.count('重开后') == 2, text
		assert not (config_root / 'run').exists(), '前台运行意外创建运行目录'
		assert not any((caller / 'node.log').exists() for caller in callers)
		run(absolute_config=True)

		configure(values='skyuv_log_root = "configured log"\nskyuv_run_root = "configured run"')
		run()
		assert (config_root / 'configured log/node.log').is_file()
		env = environment | {'SKYUV_LOG_ROOT': 'environment log', 'SKYUV_RUN_ROOT': 'environment run'}
		output = run(env=env)
		assert (config_root / 'environment log/node.log').is_file()
		assert f'run_root={config_root.as_posix()}/environment run' in output
		output = run(['--log-root', 'command log', '--run-root', 'command run'], env=env)
		assert (config_root / 'command log/node.log').is_file()
		assert f'run_root={config_root.as_posix()}/command run' in output

		configure(logger=root / 'absolute.log')
		run(['--log-root', 'unused log'])
		assert (root / 'absolute.log').is_file()
		assert not (config_root / 'unused log').exists()
		configure(logger=None)
		run(['--log-root', 'stdout unused'])
		assert not (config_root / 'stdout unused').exists()

		configure(values='skyuv_install_root = "override"')
		assert '禁止覆盖保留路径变量' in run(success=False)
		configure(values='skyuv_config_root = "override"')
		assert '禁止覆盖保留路径变量' in run(success=False)
		configure(values='skyuv_log_root = false')
		assert '根目录解析失败' in run(success=False)
		configure()
		blocked = config_root / 'blocked'
		blocked.write_text('file', encoding='utf-8')
		assert str(blocked).replace('\\', '/') in run(['--log-root', 'blocked'], success=False)
		assert '参数或目录无效' in run(['--log-root', ''], success=False)
		assert '参数或目录无效' in run(['--unknown', 'value'], success=False)
		assert '参数或目录无效' in run(['--log-root', 'one', '--log-root', 'two'], success=False)
		configure(logger='missing parent/node.log')
		output = run(success=False)
		assert f'{config_root.as_posix()}/log/missing parent/node.log' in output
		if os.name == 'nt':
			configure(values='daemon = "node.pid"')
			output = run(['--log-root', 'daemon unused log', '--run-root', 'daemon unused run'], success=False)
			assert 'daemon mode is not supported on Windows' in output
			assert not (config_root / 'daemon unused log').exists()
			assert not (config_root / 'daemon unused run').exists()
		config.unlink()
		assert '无法解析主配置' in run(success=False)
	print('新运行路径的跨目录启动、覆盖顺序、日志重开及错误路径验证通过')
	return 0


if __name__ == '__main__':
	raise SystemExit(main())
