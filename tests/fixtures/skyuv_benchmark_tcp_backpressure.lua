local skynet = require "skynet"
local socket = require "skynet.socket"

local host = "127.0.0.1"
local port = tonumber(os.getenv "SKYUV_BENCHMARK_PORT") or 25284
local chunk = string.rep("b", 64 * 1024)

local function stream_client(fd)
	socket.start(fd)
	if socket.readline(fd) ~= "start" then
		socket.close(fd)
		return
	end
	socket.warning(fd, function(_, size)
		skynet.error(string.format("SKYUV_BACKPRESSURE_WARNING %d", size))
	end)
	local writes = 0
	while socket.write(fd, chunk) do
		writes = writes + 1
		if writes % 64 == 0 then
			skynet.sleep(0)
		end
	end
	socket.close(fd)
end

skynet.start(function()
	local listen_fd = assert(socket.listen(host, port))
	socket.start(listen_fd, function(fd)
		skynet.fork(stream_client, fd)
	end)
	skynet.error(string.format("skyuv TCP 背压服务已就绪 %s:%d", host, port))
end)
