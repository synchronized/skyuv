"""验证固定性能 Runner 预检规则。"""

from __future__ import annotations

import sys
from pathlib import Path


REPOSITORY = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPOSITORY / "benchmarks"))

import runner_preflight


def valid_snapshot() -> dict[str, object]:
	return {
		"operating_system": "Linux",
		"architecture": "x86_64",
		"cpu_count": 8,
		"memory_bytes": 16 * 1024**3,
		"disk_free_bytes": 20 * 1024**3,
		"load_average_1m": 1.0,
		"governors": ["performance"] * 8,
		"tools": {tool: f"{tool} test" for tool in runner_preflight.REQUIRED_TOOLS},
		"git_dirty": False,
	}


def main() -> int:
	errors, warnings = runner_preflight.evaluate(valid_snapshot())
	assert errors == []
	assert warnings == []

	invalid = valid_snapshot()
	invalid.update({
		"operating_system": "Windows",
		"memory_bytes": 4 * 1024**3,
		"governors": ["performance", "powersave"],
		"git_dirty": True,
	})
	invalid["tools"] = {**invalid["tools"], "ninja": None}
	errors, _ = runner_preflight.evaluate(invalid)
	assert len(errors) == 5
	assert any("Linux" in error for error in errors)
	assert any("ninja" in error for error in errors)

	warning_snapshot = valid_snapshot()
	warning_snapshot["governors"] = ["powersave"] * 8
	warning_snapshot["load_average_1m"] = 5.0
	errors, warnings = runner_preflight.evaluate(warning_snapshot)
	assert errors == []
	assert len(warnings) == 2
	return 0


if __name__ == "__main__":
	raise SystemExit(main())
