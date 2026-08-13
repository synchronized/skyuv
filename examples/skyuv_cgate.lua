local skynet = require "skynet"
require "skynet.manager"

local gate
local connections = {}

skynet.register_protocol {
	name = "text",
	id = skynet.PTYPE_TEXT,
	pack = function(message)
		return message
	end,
	unpack = skynet.tostring,
}

skynet.register_protocol {
	name = "client",
	id = skynet.PTYPE_CLIENT,
}

local function send_command(command)
	skynet.send(gate, "text", command)
end

local function dispatch_gate_message(message)
	local fd, event, detail = message:match("^(%d+) (%a+) ?(.*)$")
	assert(fd and event, "无效的 C gate 消息：" .. message)
	fd = assert(tonumber(fd))

	if event == "open" then
		connections[fd] = true
		send_command("start " .. fd)
	elseif event == "data" then
		assert(connections[fd], "收到未知连接的数据")
		local response = string.pack(">I2", #detail) .. detail .. string.pack("<I4", fd)
		assert(skynet.rawsend(gate, "client", response))
	elseif event == "close" then
		connections[fd] = nil
		send_command("close")
		skynet.error("skyuv C gate 关闭验证通过")
	else
		error("未处理的 C gate 事件：" .. event)
	end
end

skynet.start(function()
	skynet.dispatch("text", function(_, source, message)
		assert(source == gate)
		skynet.ignoreret()
		dispatch_gate_message(message)
	end)

	gate = assert(skynet.launch("gate", "S", skynet.address(skynet.self()), "127.0.0.1:25285", 3, 16))
	skynet.error("skyuv C gate 已就绪")
end)
