local skynet = require "skynet"
require "skynet.manager"

skynet.start(function()
	skynet.dispatch("lua", function(_, _, command, payload)
		assert(command == "echo")
		skynet.ret(skynet.pack("node1:" .. payload))
	end)

	skynet.register("SKYUV_REMOTE")
	skynet.error("skyuv harbor 节点 1 已就绪")
end)
