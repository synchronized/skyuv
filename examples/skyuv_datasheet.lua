local skynet = require "skynet"

local mode = ...

local function wait_version(object, version)
	for _ = 1, 100 do
		if object.version == version then
			return
		end
		skynet.sleep(1)
	end
	error("等待 datasheet 更新超时")
end

if mode == "reader" then
	local datasheet = require "skynet.datasheet"
	local object

	skynet.start(function()
		object = datasheet.query("skyuv-runtime")
		skynet.dispatch("lua", function(_, _, version)
			wait_version(object, version)
			skynet.retpack(object.name, object.version, object.profile)
		end)
	end)
else
	local builder = require "skynet.datasheet.builder"
	local datasheet = require "skynet.datasheet"

	skynet.start(function()
		builder.new("skyuv-runtime", {
			name = "skyuv-跨平台",
			version = 1,
			profile = { enabled = true, tags = { "actor", "libuv" } },
		})

		local reader = skynet.newservice(SERVICE_NAME, "reader")
		local object = datasheet.query("skyuv-runtime")
		local profile = object.profile
		assert(object.version == 1 and profile.enabled)
		assert(#profile.tags == 2 and profile.tags[2] == "libuv")

		builder.update("skyuv-runtime", {
			name = "skyuv-跨平台",
			version = 2,
			profile = { enabled = false, tags = { "actor", "libuv", "datasheet" } },
		})
		wait_version(object, 2)
		assert(profile.enabled == false)
		assert(#profile.tags == 3 and profile.tags[3] == "datasheet")
		local name, version, remote_profile = skynet.call(reader, "lua", 2)
		assert(name == object.name and version == 2 and remote_profile.enabled == false)

		builder.update("skyuv-runtime", {
			name = "skyuv",
			version = 3,
			profile = 7,
		})
		wait_version(object, 3)
		assert(object.name == "skyuv" and object.profile == 7)
		assert(not pcall(function()
			return profile.enabled
		end))
		name, version, remote_profile = skynet.call(reader, "lua", 3)
		assert(name == "skyuv" and version == 3 and remote_profile == 7)

		skynet.send(reader, "debug", "EXIT")
		skynet.error("skyuv datasheet 跨服务更新验证通过")
		skynet.error("skyuv CMake 启动验证通过")
		skynet.exit()
	end)
end
