local skynet = require "skynet"
local socket = require "skynet.socket"

local host = "127.0.0.1"
local port = tonumber(os.getenv "SKYUV_BENCHMARK_PORT") or 25284
local chunk = string.rep("b", 64 * 1024)
local queue_limit = 4 * 1024 * 1024

local function queued_bytes(fd)
	for _, info in ipairs(socket.netstat()) do
		if info.id == fd then
			return info.wbuffer
		end
	end
	return nil
end

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
		if writes % 16 == 0 then
			-- 等待网络线程处理本批命令，避免快照尚未包含的发送请求无限积压。
			skynet.sleep(1)
			local queued = queued_bytes(fd)
			while queued and queued >= queue_limit do
				skynet.sleep(1)
				queued = queued_bytes(fd)
			end
			if queued == nil then
				break
			end
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
