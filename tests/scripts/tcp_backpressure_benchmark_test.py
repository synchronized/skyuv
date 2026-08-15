"""TCP 背压基准客户端的短时集成测试。"""

from __future__ import annotations

import json
import socketserver
import subprocess
import sys
import tempfile
import threading
from pathlib import Path


class StreamHandler(socketserver.BaseRequestHandler):
	def handle(self) -> None:
		if self.request.recv(64) != b"start\n":
			return
		try:
			while True:
				self.request.sendall(b"b" * 65536)
		except OSError:
			return


def main() -> int:
	repository = Path(__file__).resolve().parents[2]
	with socketserver.ThreadingTCPServer(("127.0.0.1", 0), StreamHandler) as server:
		thread = threading.Thread(target=server.serve_forever)
		thread.start()
		try:
			with tempfile.TemporaryDirectory() as directory:
				output = Path(directory) / "result.json"
				completed = subprocess.run(
					[
						sys.executable,
						str(repository / "benchmarks" / "tcp_backpressure.py"),
						"--implementation", "skyuv",
						"--build-type", "Release",
						"--allocator", "system",
						"--compiler", "test",
						"--host", "127.0.0.1",
						"--port", str(server.server_address[1]),
						"--warmup", "0.01",
						"--duration", "0.03",
						"--iterations", "2",
						"--initial-pause", "0.01",
						"--read-delay", "0.001",
						"--read-size", "1024",
						"--output", str(output),
					],
					cwd=repository,
					check=False,
				)
				if completed.returncode != 0:
					return completed.returncode
				result = json.loads(output.read_text(encoding="utf-8"))
				assert result["status"] == "success"
				assert len(result["samples"]) == 2
				assert all(sample["bytes_received"] > 0 for sample in result["samples"])
		finally:
			server.shutdown()
			thread.join()
	return 0


if __name__ == "__main__":
	raise SystemExit(main())
