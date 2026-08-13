local skynet = require "skynet"
local cluster = require "skynet.cluster"

skynet.start(function()
	cluster.reload {
		provider = "127.0.0.1:25317",
	}
	local source, value = cluster.call("provider", "@skyuv_echo", "echo", "跨节点请求")
	assert(source == "provider")
	assert(value == "跨节点请求")
	skynet.error("skyuv cluster 双节点 RPC 验证通过")
	skynet.exit()
end)
