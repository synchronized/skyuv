"""供 TCP 背压配对编排协议测试使用的持续写入服务。"""

from __future__ import annotations

import os
import socketserver


class StreamHandler(socketserver.BaseRequestHandler):
	def handle(self) -> None:
		if self.request.recv(64) != b"start\n":
			return
		print("SKYUV_BACKPRESSURE_WARNING protocol-test", flush=True)
		try:
			while True:
				self.request.sendall(b"b" * 65536)
		except OSError:
			return


class StreamServer(socketserver.TCPServer):
	allow_reuse_address = True


port = int(os.environ.get("SKYUV_BENCHMARK_PORT", "25285"))
with StreamServer(("127.0.0.1", port), StreamHandler) as server:
	server.serve_forever()
