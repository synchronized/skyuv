local skynet = require "skynet"

skynet.start(function()
	skynet.error("skyuv 进程停止验证已就绪")
	skynet.fork(function()
		while true do
			skynet.sleep(1000)
		end
	end)
end)
