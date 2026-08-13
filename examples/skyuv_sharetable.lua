local skynet = require "skynet"
local sharetable = require "skynet.sharetable"

skynet.start(function()
	sharetable.loadtable("primary", {
		name = "skyuv-跨平台",
		version = 1,
		tags = { "actor", "libuv" },
	})
	sharetable.loadstring("secondary",
		"local enabled, count = ...; return { enabled = enabled, count = count }", true, 2)

	local primary = assert(sharetable.query("primary"))
	assert(primary.name == "skyuv-跨平台" and primary.version == 1)
	assert(#primary.tags == 2 and primary.tags[2] == "libuv")
	assert(not pcall(function()
		primary.version = 99
	end))

	local all = sharetable.queryall({ "primary", "secondary", "missing" })
	assert(all.primary.name == primary.name)
	assert(all.secondary.enabled and all.secondary.count == 2)
	assert(all.missing == nil)

	sharetable.loadtable("primary", {
		name = "skyuv-跨平台",
		version = 2,
		tags = { "actor", "libuv", "sharetable" },
	})
	sharetable.update("primary")
	assert(primary.version == 2)
	assert(#primary.tags == 3 and primary.tags[3] == "sharetable")

	skynet.error("skyuv sharetable 共享与更新验证通过")
	skynet.error("skyuv CMake 启动验证通过")
	skynet.exit()
end)
