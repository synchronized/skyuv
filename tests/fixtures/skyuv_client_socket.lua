local skynet = require "skynet.manager"
local socket = require "client.socket"

skynet.start(function()
	local fd = assert(socket.connect("127.0.0.1", 25286))
	local payload = "skyuv-client-socket"

	socket.send(fd, payload)
	local response
	for _ = 1, 5000 do
		response = socket.recv(fd)
		if response ~= nil then
			break
		end
		socket.usleep(1000)
	end
	assert(response == payload, "client.socket 回环内容不一致")
	socket.shutdown(fd, "w")
	local closed
	for _ = 1, 5000 do
		closed = socket.recv(fd)
		if closed ~= nil then
			break
		end
		socket.usleep(1000)
	end
	assert(closed == "", "client.socket 未观察到对端关闭")
	socket.close(fd)

	local invalid_mode = pcall(socket.shutdown, -1, "x")
	assert(not invalid_mode, "client.socket 接受了无效 shutdown 模式")
	local refused = pcall(socket.connect, "127.0.0.1", 25285)
	assert(not refused, "client.socket 拒绝连接路径未报错")
	skynet.error("skyuv client.socket 回环验证通过")
	skynet.error("skyuv CMake 启动验证通过")
	skynet.sleep(2)
	skynet.abort()
end)
