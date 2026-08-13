local skynet = require "skynet"

skynet.start(function()
	skynet.error("skyuv CMake 启动验证通过")
	skynet.exit()
end)
