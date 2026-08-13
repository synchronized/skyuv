local skynet = require "skynet"
local harbor = require "skynet.harbor"

skynet.start(function()
	local remote = assert(harbor.queryname("SKYUV_REMOTE"))
	assert(skynet.harbor(remote) == 1)
	local response = skynet.call(remote, "lua", "echo", "node2")
	assert(response == "node1:node2")
	skynet.error("skyuv harbor 跨节点调用验证通过")
end)
