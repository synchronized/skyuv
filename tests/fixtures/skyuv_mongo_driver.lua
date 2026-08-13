local skynet = require "skynet"
local bson = require "bson"
local driver = require "skynet.mongo.driver"

skynet.start(function()
	assert(driver.length(string.pack("<I4", 37)) == 33)

	local command = bson.encode_order("ping", 1, "$db", "admin")
	local message = driver.op_msg(42, 0, command)
	local length, request_id, response_to, opcode, flags, section =
		string.unpack("<i4i4i4i4i4B", message)
	assert(length == #message)
	assert(request_id == 42)
	assert(response_to == 0)
	assert(opcode == 2013)
	assert(flags == 0)
	assert(section == 0)
	assert(not driver.reply("短响应"))

	local mongo = require "skynet.db.mongo"
	assert(type(mongo.client) == "function")
	assert(mongo.null == bson.null)
	skynet.error("skyuv mongo 驱动封包与模块加载验证通过")
	skynet.error("skyuv CMake 启动验证通过")
	skynet.exit()
end)
