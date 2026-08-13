local skynet = require "skynet"
local gateserver = require "snax.gateserver"
local netpack = require "skynet.netpack"
local socketdriver = require "skynet.socketdriver"

local handler = {
	embed = true,
}

function handler.open(_, conf)
	skynet.error("skyuv gate 已就绪", conf.address, conf.port)
end

function handler.connect(fd)
	gateserver.openclient(fd)
end

function handler.message(fd, message, size)
	local framed, framed_size = netpack.pack(message, size)
	skynet.trash(message, size)
	socketdriver.send(fd, framed, framed_size)
end

function handler.command(command)
	error("不支持的 gate 命令：" .. tostring(command))
end

gateserver.start(handler)

skynet.start(function()
	skynet.call(skynet.self(), "lua", "open", {
		address = "127.0.0.1",
		port = 25284,
		maxclient = 16,
		nodelay = true,
	})
end)
