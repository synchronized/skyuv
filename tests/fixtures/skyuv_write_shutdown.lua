local skynet = require "skynet"
local socket = require "skynet.socket"

local chunk = string.rep("x", 64 * 1024)

local function fill_write_queue(fd)
	socket.start(fd)
	if socket.readline(fd) ~= "start" then
		socket.close(fd)
		return
	end
	local queued = 0
	for _ = 1, 512 do
		if not socket.write(fd, chunk) then break end
		queued = queued + #chunk
	end
	skynet.error(string.format("SKYUV_WRITE_SHUTDOWN_QUEUED %d", queued))
end

skynet.start(function()
	local listener = assert(socket.listen("127.0.0.1", 25290))
	socket.start(listener, function(fd) skynet.fork(fill_write_queue, fd) end)
	skynet.error("skyuv 挂起写退出验证已就绪")
end)
