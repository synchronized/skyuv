local skynet = require "skynet"
local cluster = require "skynet.cluster"

skynet.start(function()
	skynet.dispatch("lua", function(_, _, command, value)
		assert(command == "echo")
		skynet.retpack("provider", value)
	end)

	cluster.reload {
		provider = "127.0.0.1:25317",
	}
	cluster.register("skyuv_echo")
	cluster.open(25317)
	skynet.error("skyuv cluster 提供节点已就绪")
end)
