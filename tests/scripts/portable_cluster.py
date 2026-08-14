"""跨平台验证 Skynet cluster 的双节点 RPC 与节点生命周期。"""

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
	parser.add_argument("provider_config")
	parser.add_argument("consumer_config")
	arguments = parser.parse_args()

	nodes: list[Node] = []
	try:
		provider = Node(arguments.executable, arguments.provider_config, "提供节点")
		nodes.append(provider)
		provider.wait_for_marker("skyuv cluster 提供节点已就绪", 10)

		consumer = Node(arguments.executable, arguments.consumer_config, "消费节点")
		nodes.append(consumer)
		consumer.wait_for_marker("skyuv cluster 双节点 RPC 验证通过", 10)
	except BaseException:
		dump_nodes(nodes)
		raise
	finally:
		for node in reversed(nodes):
			node.close()

	print("skyuv cluster 跨平台验证通过")
	return 0


if __name__ == "__main__":
	raise SystemExit(main())
