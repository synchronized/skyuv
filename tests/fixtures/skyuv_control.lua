local skynet = require "skynet.manager"
local control = require "skyuv.control"

skynet.start(function()
	assert(control.reopen_log(), "logger 不可用")
	skynet.error("skyuv 跨平台日志重开验证通过")
	skynet.error("skyuv CMake 启动验证通过")
	skynet.sleep(2)
	skynet.abort()
end)
