"""验证 soak 编排器的轮次、日志和结构化汇总协议。"""

from __future__ import annotations

import json
import os
import subprocess
import sys
import tempfile
from pathlib import Path


def main() -> int:
	repository = Path(__file__).resolve().parents[2]
	with tempfile.TemporaryDirectory() as directory:
		output = Path(directory)
		environment = os.environ.copy()
		environment["PYTHONIOENCODING"] = "cp1252"
		completed = subprocess.run([
			sys.executable, str(repository / "benchmarks" / "soak.py"),
			"--duration-seconds", "60", "--max-iterations", "2",
			"--output-directory", str(output), "--", sys.executable, "-c",
			"import sys; print('长时稳定性'); assert sys.stdout.encoding.lower().replace('-', '') == 'utf8'",
		], check=False, env=environment)
		if completed.returncode != 0:
			return completed.returncode
		summary = json.loads((output / "soak-summary.json").read_text(encoding="utf-8"))
		assert summary["status"] == "success"
		assert summary["completed_iterations"] == 2
		assert len(summary["iterations"]) == 2
		assert all((output / item["log"]).is_file() for item in summary["iterations"])
		assert all("长时稳定性" in (output / item["log"]).read_text(encoding="utf-8") for item in summary["iterations"])
	return 0


if __name__ == "__main__":
	raise SystemExit(main())
