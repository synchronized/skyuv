local skynet = require "skynet"
local socket = require "skynet.socket"

local host = "127.0.0.1"
local port = 25280

local function echo_client(fd)
	socket.start(fd)
	local line = socket.readline(fd)
	if line then
		socket.write(fd, line .. "\n")
	end
	socket.close(fd)
end

skynet.start(function()
	local listen_fd = assert(socket.listen(host, port))
	socket.start(listen_fd, function(fd)
		skynet.fork(echo_client, fd)
	end)
	skynet.error(string.format("skyuv echo 已就绪 %s:%d", host, port))
end)
