local skynet = require "skynet"
local memory = require "skynet.memory"

skynet.start(function()
	assert(type(memory.total()) == "number")
	assert(type(memory.block()) == "number")
	assert(type(memory.current()) == "number")
	assert(type(memory.info()) == "table")
	assert(type(memory.jestat()) == "table")
	assert(type(memory.mallctl("stats.allocated")) == "number")
	assert(type(memory.profactive()) == "boolean")
	assert(memory.profactive(true) == false)
	memory.dumpinfo()
	memory.dump()
	memory.dumpheap()

	skynet.error("skyuv memory API 与分配器降级验证通过")
	skynet.error("skyuv CMake 启动验证通过")
	skynet.exit()
end)
