local skynet = require "skynet"
local bson = require "bson"

skynet.start(function()
	local source = {
		name = "skyuv",
		count = 42,
		enabled = true,
		nested = { value = "跨平台" },
		array = { 1, 2, 3 },
		null_value = bson.null,
	}
	local encoded = bson.encode(source)
	local decoded = bson.decode(encoded)

	assert(decoded.name == source.name)
	assert(decoded.count == source.count)
	assert(decoded.enabled == source.enabled)
	assert(decoded.nested.value == source.nested.value)
	assert(#decoded.array == 3 and decoded.array[3] == 3)
	assert(decoded.null_value == bson.null)
	local objectid = bson.objectid("00112233445566778899aabb")
	assert(#objectid == 14)
	skynet.error("skyuv BSON 编解码验证通过")
	skynet.error("skyuv CMake 启动验证通过")
	skynet.exit()
end)
