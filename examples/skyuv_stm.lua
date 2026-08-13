local skynet = require "skynet"
local stm = require "skynet.stm"

local mode = ...

if mode == "reader" then
	local reader

	skynet.start(function()
		skynet.dispatch("lua", function(_, _, command, object)
			if command == "init" then
				reader = stm.newcopy(object)
			else
				assert(command == "read" and reader)
			end
			skynet.retpack(reader(skynet.unpack))
		end)
	end)
else
	skynet.start(function()
		local service = skynet.newservice(SERVICE_NAME, "reader")
		local writer = stm.new(skynet.pack("初始值", 1))
		local object = stm.copy(writer)
		local changed, text, version = skynet.call(service, "lua", "init", object)
		assert(changed and text == "初始值" and version == 1)

		changed = skynet.call(service, "lua", "read")
		assert(changed == false)
		writer(skynet.pack("更新值", 2))
		changed, text, version = skynet.call(service, "lua", "read")
		assert(changed and text == "更新值" and version == 2)

		skynet.send(service, "debug", "EXIT")
		skynet.error("skyuv STM 跨服务更新验证通过")
		skynet.error("skyuv CMake 启动验证通过")
		skynet.exit()
	end)
end
