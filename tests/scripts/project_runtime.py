"""验证三平台构建运行树、包外启动和重新部署缺失文件。"""

import argparse
from pathlib import Path
import subprocess
import tempfile

from package_extracted_runtime import run_echo, run_smoke


def main() -> int:
	parser = argparse.ArgumentParser(description=__doc__)
	parser.add_argument('--project-root', type=Path, required=True)
	parser.add_argument('--executable-name', required=True)
	parser.add_argument('--module-suffix', required=True)
	parser.add_argument('--build-directory', type=Path, required=True)
	parser.add_argument('--config', required=True)
	parser.add_argument('--cmake', required=True)
	args = parser.parse_args()
	root = args.project_root.resolve()
	for name in ('bin', 'cservice', 'luaclib', 'lualib', 'service', 'examples', 'licenses'):
		assert (root / name).is_dir(), f'运行目录缺失：{name}'
	executable = root / 'bin' / args.executable_name
	assert executable.is_file()
	with tempfile.TemporaryDirectory(prefix='skyuv project caller ') as directory:
		caller = Path(directory)
		run_smoke(executable, caller)
		run_echo(executable, caller / 'echo.log', caller)
		lua_name = 'lua.exe' if executable.suffix == '.exe' else 'lua'
		lua = root / 'bin' / lua_name
		if lua.exists():
			code = f'package.cpath = [[{root.as_posix()}/luaclib/?{args.module_suffix}]]; '
			code += "assert(require('client.socket')); assert(require('client.crypt'))"
			result = subprocess.run([str(lua), '-e', code], cwd=caller, capture_output=True,
				text=True, encoding='utf-8', errors='replace', timeout=10)
			assert result.returncode == 0, result.stdout + result.stderr

		# 删除生成目录内的两类部署产物，确认增量构建确实执行安装而非依赖旧副本。
		module = root / 'cservice' / f'logger{args.module_suffix}'
		config = root / 'examples/skyuv.conf'
		module.unlink()
		config.unlink()
		result = subprocess.run([args.cmake, '--build', str(args.build_directory),
			'--config', args.config, '--target', 'skyuv_stage_runtime', '--parallel', '2'],
			capture_output=True, text=True, encoding='utf-8', errors='replace', timeout=30)
		assert result.returncode == 0, result.stdout + result.stderr
		assert module.is_file() and config.is_file()
		run_smoke(executable, caller)
	print('三平台 project 布局、包外启动、Lua 加载和缺失文件重新部署验证通过')
	return 0


if __name__ == '__main__':
	raise SystemExit(main())
