local skynet = require "skynet"
local lpeg = require "lpeg"

skynet.start(function()
	local alpha = lpeg.R("az", "AZ")
	local identifier = lpeg.C(alpha ^ 1)
	local separator = lpeg.S(",;")
	local list = lpeg.Ct(identifier * (separator * identifier) ^ 0) * -1
	local captures = list:match("actor,libuv;Skynet")
	assert(#captures == 3)
	assert(captures[1] == "actor" and captures[3] == "Skynet")
	assert(list:match("actor,") == nil)
	assert((lpeg.P("跨平台") * -1):match("跨平台") == #"跨平台" + 1)
	assert((lpeg.P("sky") + lpeg.P("uv")):match("uv") == 3)
	assert((lpeg.P(1) ^ 0 * -1):match("包含\0零字节") == #"包含\0零字节" + 1)

	skynet.error("skyuv lpeg 独立模式验证通过")
	skynet.error("skyuv CMake 启动验证通过")
	skynet.exit()
end)
