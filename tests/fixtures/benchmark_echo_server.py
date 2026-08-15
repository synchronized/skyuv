"""供 TCP 配对编排协议测试使用的固定长度 echo 服务。"""

from __future__ import annotations

import os
import socketserver


class EchoHandler(socketserver.BaseRequestHandler):
	def handle(self) -> None:
		while data := self.request.recv(4096):
			self.request.sendall(data)


class EchoServer(socketserver.TCPServer):
	allow_reuse_address = True


message_size = int(os.environ.get("SKYUV_BENCHMARK_MESSAGE_SIZE", "64"))
port = int(os.environ.get("SKYUV_BENCHMARK_PORT", "25282"))
if message_size <= 0:
	raise ValueError("消息尺寸必须大于零")

with EchoServer(("127.0.0.1", port), EchoHandler) as server:
	server.serve_forever()
