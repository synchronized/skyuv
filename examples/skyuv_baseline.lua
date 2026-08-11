local skynet = require "skynet.manager"

local mode = ...

if mode == "worker" then
	skynet.start(function()
		skynet.dispatch("lua", function(_, _, command, value)
			assert(command == "ping")
			skynet.retpack("pong", value)
		end)
	end)
	return
end

skynet.start(function()
	local worker = skynet.newservice("skyuv_baseline", "worker")
	local response, value = skynet.call(worker, "lua", "ping", 42)
	assert(response == "pong" and value == 42)
	skynet.error("skyuv Actor 消息验证通过")

	skynet.timeout(2, function()
		skynet.error("skyuv 定时器验证通过")
		skynet.error("skyuv 正常关闭验证开始")
		skynet.abort()
	end)
end)
