local skynet = require "skynet"
local sproto = require "sproto"

local schema = [[
.Profile {
	id 0 : integer
	name 1 : string
	active 2 : boolean
	tags 3 : *string
}
]]

skynet.start(function()
	local protocol = sproto.parse(schema)
	local source = {
		id = 42,
		name = "skyuv-跨平台",
		active = true,
		tags = { "actor", "libuv", "sproto" },
	}
	local encoded = protocol:encode("Profile", source)
	local decoded = protocol:decode("Profile", encoded)

	assert(decoded.id == source.id)
	assert(decoded.name == source.name)
	assert(decoded.active == source.active)
	assert(#decoded.tags == 3 and decoded.tags[2] == "libuv")
	local packed = protocol:pencode("Profile", source)
	local unpacked = protocol:pdecode("Profile", packed)
	assert(unpacked.name == source.name and unpacked.tags[3] == "sproto")
	skynet.error("skyuv sproto 编解码验证通过")
	skynet.error("skyuv CMake 启动验证通过")
	skynet.exit()
end)
