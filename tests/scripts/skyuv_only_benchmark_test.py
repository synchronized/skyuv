"""验证单实现采集无需原版路径，清单不伪装为配对结果。"""

import argparse
import json
from pathlib import Path
import subprocess
import sys
import tempfile


def main() -> int:
	parser = argparse.ArgumentParser()
	parser.add_argument("--executable", required=True)
	parser.add_argument("--actor-config", required=True)
	args = parser.parse_args()
	repository = Path(__file__).resolve().parents[2]
	with tempfile.TemporaryDirectory(prefix="skyuv only benchmark ") as directory:
		for name, executable, config, extra in (
			("actor", args.executable, args.actor_config, []),
			("tcp", sys.executable, str(repository / "tests/fixtures/benchmark_echo_server.py"), ["--port", "25391"]),
			("udp", sys.executable, str(repository / "tests/fixtures/benchmark_udp_server.py"), ["--port", "25392"]),
		):
			output = Path(directory) / name
			command = [sys.executable, str(repository / f"benchmarks/compare_{name}.py"),
				"--skyuv-executable", executable, "--skyuv-config", config,
				"--build-type", "Release", "--allocator", "system", "--compiler", "protocol-test",
				"--warmup", "0.01", "--duration", "0.03", "--iterations", "1",
				"--output-directory", str(output), *extra]
			missing = subprocess.run(command, capture_output=True, timeout=10)
			assert missing.returncode == 2 and b"--upstream-executable" in missing.stderr
			subprocess.run([*command, "--skyuv-only"], check=True, timeout=15)
			manifest = json.loads(next(output.glob("*-comparison.json")).read_text())
			assert manifest["paired"] is False
			assert [entry["implementation"] for entry in manifest["results"]] == ["skyuv"]
	return 0


if __name__ == "__main__":
	raise SystemExit(main())
