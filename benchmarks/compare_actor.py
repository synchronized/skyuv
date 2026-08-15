"""使用相同参数依次运行 skyuv 与原版 Skynet Actor 基准。"""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
from pathlib import Path


def main() -> int:
	parser = argparse.ArgumentParser(description=__doc__)
	parser.add_argument("--skyuv-executable", type=Path, required=True)
	parser.add_argument("--skyuv-config", type=Path, required=True)
	parser.add_argument("--upstream-executable", type=Path, required=True)
	parser.add_argument("--upstream-config", type=Path, required=True)
	parser.add_argument("--build-type", required=True)
	parser.add_argument("--allocator", required=True)
	parser.add_argument("--compiler", required=True)
	parser.add_argument("--warmup", type=float, default=2.0)
	parser.add_argument("--duration", type=float, default=10.0)
	parser.add_argument("--iterations", type=int, default=5)
	parser.add_argument("--output-directory", type=Path, required=True)
	args = parser.parse_args()

	args.output_directory.mkdir(parents=True, exist_ok=True)
	runner = Path(__file__).with_name("actor_ping_pong.py")
	results = []
	for implementation, executable, config in (
		("skyuv", args.skyuv_executable, args.skyuv_config),
		("upstream", args.upstream_executable, args.upstream_config),
	):
		output = args.output_directory / f"actor-ping-pong-{implementation}.json"
		command = [
			sys.executable, str(runner),
			"--implementation", implementation,
			"--build-type", args.build_type,
			"--allocator", args.allocator,
			"--compiler", args.compiler,
			"--warmup", str(args.warmup),
			"--duration", str(args.duration),
			"--iterations", str(args.iterations),
			"--executable", str(executable),
			"--config", str(config),
			"--output", str(output),
		]
		completed = subprocess.run(command, check=False)
		if completed.returncode != 0:
			return completed.returncode
		result = json.loads(output.read_text(encoding="utf-8"))
		results.append({
			"implementation": implementation,
			"result": output.name,
			"process_metrics": result["summary"]["process_metrics"],
		})

	manifest = {
		"benchmark": "actor_ping_pong",
		"paired": True,
		"parameters": {
			"warmup_seconds": args.warmup,
			"duration_seconds": args.duration,
			"iterations": args.iterations,
			"build_type": args.build_type,
			"allocator": args.allocator,
			"compiler": args.compiler,
		},
		"results": results,
	}
	(args.output_directory / "actor-ping-pong-comparison.json").write_text(
		json.dumps(manifest, ensure_ascii=False, indent=2, sort_keys=True) + "\n", encoding="utf-8"
	)
	print("Actor ping-pong 配对对照完成")
	return 0


if __name__ == "__main__":
	raise SystemExit(main())
