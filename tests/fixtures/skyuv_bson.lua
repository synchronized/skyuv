local skynet = require "skynet"
local bson = require "bson"

local function expect_error(label, marker, operation)
	local ok, message = pcall(operation)
	assert(not ok, label .. " 未按预期失败")
	assert(tostring(message):find(marker, 1, true),
		label .. " 错误信息不明确：" .. tostring(message))
end

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

	expect_error("非表编码", "table expected", function()
		bson.encode("not-a-table")
	end)
	expect_error("数字字典键", "key can't be number", function()
		bson.encode({ [5] = "value" })
	end)
	expect_error("不支持的值类型", "Invalid value type", function()
		bson.encode({ callback = function() end })
	end)
	expect_error("非法 UTF-8", "Invalid utf8 string", function()
		bson.encode({ value = string.char(0xff) })
	end)
	expect_error("非法 ObjectID 长度", "Invalid objectid", function()
		bson.objectid("001122")
	end)
	expect_error("非法有序字典", "Invalid ordered dict", function()
		bson.encode_order("key")
	end)
	expect_error("非法 BSON 子类型", "Invalid subtype", function()
		bson.encode({ value = string.char(0, 1) })
	end)
	local circular = {}
	circular.self = circular
	expect_error("循环引用", "Too depth while encoding bson", function()
		bson.encode(circular)
	end)

	-- 多次错误路径必须完整回收临时编码缓冲区，之后仍可正常使用模块。
	assert(bson.decode(bson.encode({ recovered = true })).recovered)
	skynet.error("skyuv BSON 编解码验证通过")
	skynet.error("skyuv CMake 启动验证通过")
	skynet.exit()
end)
