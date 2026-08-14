"""跨平台验证 Skynet harbor 的跨节点调用与节点生命周期。"""

import argparse
import subprocess
import sys
import tempfile
import time


class Node:
	def __init__(self, executable: str, config: str, name: str) -> None:
		self.name = name
		self.output_file = tempfile.TemporaryFile()
		self.process = subprocess.Popen(
			[executable, config],
			stdout=self.output_file,
			stderr=subprocess.STDOUT,
		)

	def output(self) -> str:
		self.output_file.flush()
		self.output_file.seek(0)
		return self.output_file.read().decode("utf-8", errors="replace")

	def wait_for_marker(self, marker: str, timeout: float) -> None:
		deadline = time.monotonic() + timeout
		while time.monotonic() < deadline:
			if marker in self.output():
				return
			if self.process.poll() is not None:
				break
			time.sleep(0.1)
		raise RuntimeError(f"{self.name} 等待日志标记超时：{marker}\n{self.output()}")

	def wait_for_exit(self, timeout: float) -> None:
		try:
			self.process.wait(timeout=timeout)
		except subprocess.TimeoutExpired as error:
			raise RuntimeError(f"{self.name} 未按预期退出。\n{self.output()}") from error

	def kill(self) -> None:
		if self.process.poll() is None:
			self.process.kill()
			self.process.wait()

	def close(self) -> None:
		self.kill()
		self.output_file.close()


def dump_nodes(nodes: list[Node]) -> None:
	for node in nodes:
		sys.stderr.write(f"\n===== {node.name} =====\n{node.output()}")


def main() -> int:
	parser = argparse.ArgumentParser()
	parser.add_argument("executable")
	parser.add_argument("node1_config")
	parser.add_argument("node2_config")
	parser.add_argument("node3_config")
	arguments = parser.parse_args()

	nodes: list[Node] = []
	try:
		node1 = Node(arguments.executable, arguments.node1_config, "节点 1")
		nodes.append(node1)
		node1.wait_for_marker("skyuv harbor 节点 1 已就绪", 10)

		node2 = Node(arguments.executable, arguments.node2_config, "节点 2")
		nodes.append(node2)
		node2.wait_for_marker("skyuv harbor 跨节点调用验证通过", 10)
		node2.kill()
		node1.wait_for_marker("slave 2 is down", 10)

		retry = Node(arguments.executable, arguments.node2_config, "重复 ID 节点")
		nodes.append(retry)
		node1.wait_for_marker("Slave 2 already register", 10)
		retry.wait_for_exit(5)

		node3 = Node(arguments.executable, arguments.node3_config, "节点 3")
		nodes.append(node3)
		node3.wait_for_marker("skyuv harbor 跨节点调用验证通过", 10)
		node3.kill()
		node1.wait_for_marker("slave 3 is down", 10)
	except BaseException:
		dump_nodes(nodes)
		raise
	finally:
		for node in reversed(nodes):
			node.close()

	print("skyuv harbor 跨平台验证通过")
	return 0


if __name__ == "__main__":
	raise SystemExit(main())
