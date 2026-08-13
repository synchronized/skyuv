local skynet = require "skynet"

skynet.start(function()
	local socket = require "client.socket"

	assert(type(socket.connect) == "function")
	assert(type(socket.recv) == "function")
	assert(type(socket.send) == "function")
	assert(type(socket.shutdown) == "function")
	assert(type(socket.close) == "function")
	assert(type(socket.usleep) == "function")
	assert(type(socket.readstdin) == "function")
	skynet.error("skyuv client.socket 加载验证通过")
	skynet.error("skyuv CMake 启动验证通过")
	skynet.exit()
end)
