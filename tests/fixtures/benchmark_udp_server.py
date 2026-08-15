"""供 UDP 配对编排协议测试使用的 echo 服务。"""

from __future__ import annotations

import os
import socketserver


class EchoHandler(socketserver.BaseRequestHandler):
	def handle(self) -> None:
		data, server_socket = self.request
		server_socket.sendto(data, self.client_address)


class EchoServer(socketserver.ThreadingUDPServer):
	allow_reuse_address = True


port = int(os.environ.get("SKYUV_BENCHMARK_PORT", "25287"))
with EchoServer(("127.0.0.1", port), EchoHandler) as server:
	print("SKYUV_UDP_BENCHMARK_READY", flush=True)
	server.serve_forever()
