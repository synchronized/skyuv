local skynet = require "skynet.manager"

skynet.start(function()
	skynet.error("SKYUV_RUNTIME_SMOKE_OK")
	skynet.sleep(2)
	skynet.abort()
end)
