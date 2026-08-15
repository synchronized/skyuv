local skynet = require "skynet"
local socket = require "skynet.socket"

local host = "127.0.0.1"
local port = 25285

skynet.start(function()
	local server
	server = socket.udp(function(data, from)
		assert(socket.sendto(server, from, data))
	end, host, port)
	assert(server)
	skynet.error(string.format("skyuv UDP 基准服务已就绪 %s:%d", host, port))
end)
