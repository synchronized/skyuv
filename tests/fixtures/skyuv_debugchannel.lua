local skynet = require "skynet"
local debugchannel = require "skynet.debugchannel"

skynet.start(function()
	local writer, handle = debugchannel.create()
	local reader = debugchannel.connect(handle)

	assert(reader:read() == nil)
	writer:write("调试命令\0payload")
	assert(reader:read() == "调试命令\0payload")
	reader:write("第二条命令")
	assert(writer:read() == "第二条命令")

	local hook_count = 0
	local function hooked_work()
		local total = 0
		for index = 1, 20 do
			total = total + index
		end
		return total
	end
	debugchannel.sethook(function(event)
		assert(type(event) == "string")
		hook_count = hook_count + 1
		return false
	end, "", 1)
	assert(hooked_work() == 210)
	debugchannel.sethook()
	assert(hook_count > 0)

	writer = nil
	reader = nil
	collectgarbage("collect")
	skynet.error("skyuv debugchannel 队列、二进制数据与调试钩子验证通过")
	skynet.error("skyuv CMake 启动验证通过")
	skynet.exit()
end)
