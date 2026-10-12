"""独立 Lua 客户端的动态端口收发、半关闭与 GC 验证。"""
import socket
import subprocess
import sys
import threading
import json


def main():
	errors = []
	with socket.socket() as listener:
		listener.bind(("127.0.0.1", 0))
		listener.listen(16)
		listener.settimeout(20)
		def serve():
			try:
				for _ in range(15):
					connection, _ = listener.accept()
					with connection:
						connection.settimeout(10)
						chunks = []
						while data := connection.recv(4096):
							chunks.append(data)
						if chunks:
							connection.sendall(b"".join(chunks))
			except BaseException as error:
				errors.append(error)
		thread = threading.Thread(target=serve, daemon=True)
		thread.start()
		result = subprocess.run([*sys.argv[1:], str(listener.getsockname()[1])],
			capture_output=True, text=True, encoding="utf-8", timeout=45)
		thread.join(timeout=2)
		if result.returncode:
			raise RuntimeError(result.stdout + result.stderr)
		if thread.is_alive() or errors:
			raise RuntimeError(str(errors) or "回环服务未退出")
		assert "client libuv lifecycle ok" in result.stdout
	# 保持输入管道打开，确保 GC 不依赖 stdin EOF 才能退出。
	program = "package.cpath=" + json.dumps(sys.argv[3]) + "; local s=require('client.socket'); s.readstdin(); package.loaded['client.socket']=nil; s=nil; collectgarbage('collect'); print('stdin gc ok')"
	process = subprocess.Popen([sys.argv[1], "-e", program], stdin=subprocess.PIPE,
		stdout=subprocess.PIPE, stderr=subprocess.PIPE)
	try:
		assert process.wait(timeout=5) == 0
		assert b"stdin gc ok" in process.stdout.read()
	finally:
		if process.poll() is None:
			process.kill()
			process.wait()
		process.stdin.close()
		process.stdout.close()
		process.stderr.close()
	print("独立 Lua 客户端生命周期验证通过")


if __name__ == "__main__":
	main()
