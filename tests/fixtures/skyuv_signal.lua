local skynet = require "skynet.manager"

skynet.start(function()
	skynet.error("skyuv SIGHUP 验证已就绪")
	skynet.sleep(500)
	skynet.error("skyuv SIGHUP 日志重开验证通过")
	skynet.sleep(2)
	skynet.abort()
end)
