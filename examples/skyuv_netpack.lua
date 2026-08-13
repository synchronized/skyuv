local skynet = require "skynet"
local netpack = require "skynet.netpack"

local function roundtrip(source)
	local pointer, size = netpack.pack(source)
	assert(size == #source + 2)
	local framed = netpack.tostring(pointer, size)
	local declared = string.unpack(">I2", framed)
	assert(declared == #source)
	assert(framed:sub(3) == source)
end

skynet.start(function()
	roundtrip("")
	roundtrip("skyuv\0跨平台")
	roundtrip(string.rep("x", 65535))
	assert(not pcall(netpack.pack, string.rep("x", 65536)))

	skynet.error("skyuv netpack 帧编码与所有权验证通过")
	skynet.error("skyuv CMake 启动验证通过")
	skynet.exit()
end)
