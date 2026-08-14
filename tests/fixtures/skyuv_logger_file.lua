local skynet = require "skynet.manager"
local control = require "skyuv.control"

skynet.start(function()
	skynet.error("skyuv logger 文件写入验证：重开前")
	assert(control.reopen_log(), "logger 不可用")
	skynet.error("skyuv logger 文件写入验证：重开后")
	-- 日志投递是异步的，退役前让 logger 完成刷新。
	skynet.sleep(5)
	skynet.abort()
end)
