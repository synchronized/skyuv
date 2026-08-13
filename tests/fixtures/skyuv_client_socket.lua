local skynet = require "skynet"
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
	socket.close(fd)
	skynet.error("skyuv client.socket 回环验证通过")
	skynet.error("skyuv CMake 启动验证通过")
	skynet.exit()
end)
