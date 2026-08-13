local skynet = require "skynet"
local cluster = require "skynet.cluster.core"

skynet.start(function()
	assert(cluster.isname("@service"))
	assert(not cluster.isname("service"))
	assert(type(cluster.nodename()) == "string")
	assert(#cluster.nodename() > 0)

	local trace = cluster.packtrace("skyuv-trace")
	assert(trace:byte(1) == 0 and trace:byte(2) == 12)
	assert(cluster.unpackrequest(trace:sub(3)) == "skyuv-trace")

	local response = cluster.packresponse(42, true, "响应-data")
	assert(response:byte(1) == 0)
	local session, ok, payload = cluster.unpackresponse(response:sub(3))
	assert(session == 42)
	assert(ok)
	assert(payload == "响应-data")

	local failed = cluster.packresponse(43, false, "cluster-error")
	local failed_session, failed_ok, reason = cluster.unpackresponse(failed:sub(3))
	assert(failed_session == 43)
	assert(not failed_ok)
	assert(reason == "cluster-error")

	skynet.error("skyuv cluster.core 编解码验证通过")
	skynet.error("skyuv CMake 启动验证通过")
	skynet.exit()
end)
