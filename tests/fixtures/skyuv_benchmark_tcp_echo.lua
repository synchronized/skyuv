local skynet = require "skynet"
local socket = require "skynet.socket"

local host = "127.0.0.1"
local port = 25281
local message_size = tonumber(skynet.getenv "benchmark_message_size") or 64

local function echo_client(fd)
	socket.start(fd)
	while true do
		local data = socket.read(fd, message_size)
		if not data then
			break
		end
		socket.write(fd, data)
	end
	socket.close(fd)
end

skynet.start(function()
	local listen_fd = assert(socket.listen(host, port))
	socket.start(listen_fd, function(fd)
		skynet.fork(echo_client, fd)
	end)
	skynet.error(string.format("skyuv TCP 基准服务已就绪 %s:%d，消息尺寸 %d", host, port, message_size))
end)
