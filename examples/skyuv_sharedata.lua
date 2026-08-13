local skynet = require "skynet"
local sharedata = require "skynet.sharedata"

local function wait_version(object, version)
	for _ = 1, 100 do
		if object.version == version then
			return
		end
		skynet.sleep(1)
	end
	error("等待 sharedata 更新超时")
end

skynet.start(function()
	sharedata.new("skyuv-runtime", {
		version = 1,
		name = "skyuv-跨平台",
		profile = { enabled = true, tags = { "actor", "libuv" } },
	})

	local object = sharedata.query("skyuv-runtime")
	local profile = object.profile
	assert(object.version == 1 and profile.enabled)
	assert(#profile.tags == 2 and profile.tags[2] == "libuv")
	assert(not pcall(function()
		object.version = 99
	end))

	local copy = sharedata.deepcopy("skyuv-runtime")
	assert(copy.name == object.name and copy.profile.tags[1] == "actor")
	copy.version = 100
	assert(object.version == 1)

	sharedata.update("skyuv-runtime", {
		version = 2,
		name = "skyuv-跨平台",
		profile = { enabled = false, tags = { "actor", "libuv", "sharedata" } },
	})
	wait_version(object, 2)
	assert(profile.enabled == false)
	assert(#profile.tags == 3 and profile.tags[3] == "sharedata")

	sharedata.update("skyuv-runtime", { version = 3, name = "skyuv" })
	wait_version(object, 3)
	assert(object.profile == nil)
	assert(not pcall(function()
		return profile.enabled
	end))

	sharedata.delete("skyuv-runtime")
	sharedata.flush()
	skynet.error("skyuv sharedata 更新与回收验证通过")
	skynet.error("skyuv CMake 启动验证通过")
	skynet.exit()
end)
