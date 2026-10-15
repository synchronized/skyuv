"""验证跨版本参考线、复测升级和不可比输入拒绝。"""

import json
from pathlib import Path
import subprocess
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "benchmarks"))
import baseline_regression as regression
from baseline_noise import ENVIRONMENT_KEYS


def write(path, value):
	path.parent.mkdir(parents=True, exist_ok=True)
	path.write_text(json.dumps(value), encoding="utf-8")


def fixture(root, commit, window):
	environment = {key: "fixed" for key in ENVIRONMENT_KEYS}
	environment.update(authoritative=True, dirty=False, runner_kind="self-hosted-fixed",
		commit=commit, timestamp_utc=window, cpu="Model name: fixed-cpu")
	write(root / "environment.json", environment)
	for name in regression.SCENARIOS:
		parameters = {"warmup_seconds": 2, "duration_seconds": 10, "iterations": 5}
		metadata = {"allocator": "jemalloc", "compiler": "clang-test", "build_type": "Release"}
		throughput = "throughput_bytes_per_second" if name == "tcp_backpressure" else "throughput_ops_per_second"
		result = {"status": "success", "implementation": "skyuv", "benchmark": {"name": name},
			"environment": {**metadata, "commit": commit, "dirty": False}, "parameters": parameters,
			"summary": {throughput + "_median": 1000, "latency_ms": {"p99": 0.3}, "packet_loss_ratio": 0},
			"samples": [{"iteration": i, "duration_seconds": 10, throughput: 1000,
				"latency_ms": {"p99": 0.3}, "packet_loss_ratio": 0} for i in range(1, 6)]}
		write(root / name / "skyuv.json", result)
		write(root / name / "result-comparison.json", {"benchmark": name, "paired": False,
			"parameters": {**parameters, **metadata}, "results": [{"implementation": "skyuv", "result": "skyuv.json",
			"process_metrics": {"available": True, "peak_rss_bytes": 1000000, "average_cpu_cores": 0.01}}]})


def edit(path, function):
	data = json.loads(path.read_text())
	function(data)
	write(path, data)


def throughput(root, name, value):
	def change(data):
		data["summary"]["throughput_ops_per_second_median"] = value
		for sample in data["samples"]:
			sample["throughput_ops_per_second"] = value
	edit(root / name / "skyuv.json", change)


def rejected(function, fragment):
	try:
		function()
	except ValueError as error:
		assert fragment in str(error), str(error)
	else:
		raise AssertionError(f"应拒绝：{fragment}")


def main():
	with tempfile.TemporaryDirectory() as directory:
		root = Path(directory)
		first, second, candidate, repeat = [root / name for name in ("first", "second", "candidate", "repeat")]
		for path, commit, window in ((first, "old", "1"), (second, "old", "2"),
			(candidate, "new", "3"), (repeat, "new", "4")):
			fixture(path, commit, window)
		check = lambda: regression.evaluate(first, second, candidate)
		assert check()["status"] == "within_reference"
		assert check()["blocking"] is False
		assert "不自动拒绝合并" in regression.render_markdown(check())
		throughput(second, "actor_ping_pong", 900)
		throughput(candidate, "actor_ping_pong", 860)
		assert check()["status"] == "within_reference"  # 使用两参考窗口较小值。
		throughput(second, "actor_ping_pong", 1000)
		throughput(candidate, "actor_ping_pong", 950)
		assert check()["status"] == "within_reference"  # 恰在线上不算超过。
		throughput(candidate, "actor_ping_pong", 949)
		assert check()["status"] == "retest"
		throughput(repeat, "tcp_echo", 800)
		assert regression.evaluate(first, second, candidate, repeat)["status"] == "retest"
		throughput(repeat, "actor_ping_pong", 940)
		assert regression.evaluate(first, second, candidate, repeat)["status"] == "investigate"
		fixture(candidate, "new", "3")
		manifest = candidate / "tcp_backpressure/result-comparison.json"
		edit(manifest, lambda d: d["results"][0]["process_metrics"].update(average_cpu_cores=0.06))
		assert check()["status"] == "within_reference"
		edit(manifest, lambda d: d["results"][0]["process_metrics"].update(average_cpu_cores=0.061))
		assert check()["status"] == "retest"
		fixture(candidate, "new", "3")
		manifest = candidate / "tcp_echo/result-comparison.json"
		edit(manifest, lambda d: d["results"][0]["process_metrics"].update(peak_rss_bytes=1300000))
		assert check()["status"] == "within_reference"
		edit(manifest, lambda d: d["results"][0]["process_metrics"].update(peak_rss_bytes=1300001))
		assert check()["status"] == "retest"
		fixture(candidate, "new", "3")
		edit(candidate / "tcp_echo/skyuv.json", lambda d: d["summary"]["latency_ms"].update(p99=0.421))
		assert check()["status"] == "retest"
		# 环境变化、样本损坏和错误场景不能被当作性能通过。
		for key in ("machine_identity", "clang", "cpu_count", "submodules", "benchmark_address_space_limit_bytes"):
			fixture(candidate, "new", "3")
			edit(candidate / "environment.json", lambda d: d.update({key: "changed"}))
			rejected(check, key)
		for mutation, message in ((lambda d: d.update(status="error"), "执行未成功"),
			(lambda d: d["samples"].pop(), "至少需要五轮"),
			(lambda d: d["summary"].update(throughput_ops_per_second_median=123), "中位吞吐"),
			(lambda d: d["samples"][0].update(throughput_ops_per_second=float("nan")), "指标无效"),
			(lambda d: d["samples"][0].update(duration_seconds=1), "测量未完成"),
			(lambda d: d["parameters"].update(seed=2), "参数不可比")):
			fixture(candidate, "new", "3")
			edit(candidate / "tcp_echo/skyuv.json", mutation)
			rejected(check, message)
		fixture(candidate, "new", "3")
		edit(candidate / "udp_request_reply/skyuv.json", lambda d: d["summary"].update(packet_loss_ratio=0.01))
		rejected(check, "丢包")
		fixture(candidate, "new", "3")
		rejected(lambda: regression.evaluate(first, first, candidate), "不同窗口")
		rejected(lambda: regression.evaluate(first, second, candidate, candidate), "不同窗口")
		edit(candidate / "environment.json", lambda d: d.update(authoritative=False))
		rejected(check, "固定 Runner")
		fixture(candidate, "new", "3")
		fixture(second, "other-reference", "2")
		rejected(check, "commit")
		fixture(second, "old", "2")
		fixture(repeat, "other-candidate", "4")
		rejected(lambda: regression.evaluate(first, second, candidate, repeat), "commit")
		fixture(candidate, "new", "3")
		(candidate / "tcp_echo/result-comparison.json").unlink()
		rejected(check, "五类完整")
		fixture(candidate, "new", "3")
		throughput(candidate, "actor_ping_pong", 500)
		command = [sys.executable, str(Path(regression.__file__)), "--reference-first", str(first),
			"--reference-second", str(second), "--candidate", str(candidate),
			"--output-json", str(root / "report.json"), "--output-markdown", str(root / "report.md")]
		assert subprocess.run(command, capture_output=True).returncode == 0
		assert json.loads((root / "report.json").read_text())["status"] == "retest"
		edit(candidate / "environment.json", lambda d: d.update(dirty=True))
		assert subprocess.run(command, capture_output=True).returncode == 2
	return 0


if __name__ == "__main__":
	raise SystemExit(main())
