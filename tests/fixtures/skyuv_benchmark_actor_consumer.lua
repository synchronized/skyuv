local skynet = require "skynet"

local total = 0
local counts = {}

skynet.start(function()
	skynet.dispatch("lua", function(_, _, command, producer)
		if command == "reset" then
			total = 0
			counts = {}
			skynet.retpack()
		elseif command == "message" then
			total = total + 1
			counts[producer] = (counts[producer] or 0) + 1
		elseif command == "barrier" then
			skynet.retpack()
		elseif command == "snapshot" then
			skynet.retpack(total, counts)
		else
			error("未知消费者命令：" .. tostring(command))
		end
	end)
end)
