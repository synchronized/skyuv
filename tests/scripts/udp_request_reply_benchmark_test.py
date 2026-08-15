"""UDP request/reply 基准客户端的短时集成测试。"""

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
		data, client = self.request
		client.sendto(data, self.client_address)


def main() -> int:
	repository = Path(__file__).resolve().parents[2]
	with socketserver.ThreadingUDPServer(("127.0.0.1", 0), EchoHandler) as server:
		thread = threading.Thread(target=server.serve_forever)
		thread.start()
		try:
			with tempfile.TemporaryDirectory() as directory:
				output = Path(directory) / "result.json"
				completed = subprocess.run(
					[
						sys.executable,
						str(repository / "benchmarks" / "udp_request_reply.py"),
						"--implementation", "skyuv",
						"--build-type", "Release",
						"--allocator", "system",
						"--compiler", "test",
						"--host", "127.0.0.1",
						"--port", str(server.server_address[1]),
						"--message-size", "31",
						"--warmup", "0.01",
						"--duration", "0.03",
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
				assert result["parameters"]["message_size_bytes"] == 31
				assert len(result["samples"]) == 2
				assert all(sample["packets_received"] > 0 for sample in result["samples"])
				assert result["summary"]["packet_loss_ratio"] == 0.0
		finally:
			server.shutdown()
			thread.join()
	return 0


if __name__ == "__main__":
	raise SystemExit(main())
