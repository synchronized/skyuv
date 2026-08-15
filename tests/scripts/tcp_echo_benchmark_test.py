"""TCP echo 基准客户端的短时集成测试。"""

from __future__ import annotations

import json
import socketserver
import subprocess
import sys
import tempfile
import threading
from pathlib import Path


class EchoHandler(socketserver.BaseRequestHandler):
	def handle(self) -> None:
		while data := self.request.recv(4096):
			self.request.sendall(data)


def main() -> int:
	repository = Path(__file__).resolve().parents[2]
	with socketserver.ThreadingTCPServer(("127.0.0.1", 0), EchoHandler) as server:
		thread = threading.Thread(target=server.serve_forever)
		thread.start()
		try:
			with tempfile.TemporaryDirectory() as directory:
				output = Path(directory) / "result.json"
				completed = subprocess.run(
					[
						sys.executable,
						str(repository / "benchmarks" / "tcp_echo.py"),
						"--implementation", "skyuv",
						"--build-type", "Release",
						"--allocator", "system",
						"--compiler", "test",
						"--host", "127.0.0.1",
						"--port", str(server.server_address[1]),
						"--message-size", "17",
						"--warmup", "0.01",
						"--duration", "0.02",
						"--iterations", "2",
						"--output", str(output),
					],
					cwd=repository,
					check=False,
				)
				if completed.returncode != 0:
					return completed.returncode
				result = json.loads(output.read_text(encoding="utf-8"))
				assert result["status"] == "success"
				assert result["parameters"]["message_size_bytes"] == 17
				assert len(result["samples"]) == 2
				assert all(sample["operations"] > 0 for sample in result["samples"])
		finally:
			server.shutdown()
			thread.join()
	return 0


if __name__ == "__main__":
	raise SystemExit(main())
