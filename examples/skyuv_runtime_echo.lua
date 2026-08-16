local skynet = require "skynet.manager"
local socket = require "skynet.socket"

local host = "127.0.0.1"
local port = 25490

skynet.start(function()
	local listen_fd = assert(socket.listen(host, port))
	socket.start(listen_fd, function(fd)
		socket.start(fd)
		local line = socket.readline(fd)
		if line then
			socket.write(fd, line .. "\n")
		end
		socket.close(fd)
		socket.close(listen_fd)
		skynet.error("SKYUV_RUNTIME_ECHO_OK")
		skynet.sleep(2)
		skynet.abort()
	end)
	skynet.error("SKYUV_RUNTIME_ECHO_READY")
end)
