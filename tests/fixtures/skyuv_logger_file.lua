local skynet = require "skynet.manager"
local control = require "skyuv.control"

local before = "skyuv logger 文件写入验证：重开前"
local after = "skyuv logger 文件写入验证：重开后"

skynet.start(function()
	local filename = skynet.getenv("logger")
	local offset = 0
	if filename then
		local file = assert(io.open(filename, "r"))
		offset = assert(file:seek("end"))
		file:close()
	end
	skynet.error(before)
	assert(control.reopen_log(), "logger 不可用")
	skynet.error(after)
	if filename then
		-- 只检查本次追加的日志，防止前一次启动的标记掩盖异步投递未完成。
		local deadline = skynet.now() + 500
		while true do
			local file = assert(io.open(filename, "r"))
			assert(file:seek("set", offset))
			local content = assert(file:read("*a"))
			file:close()
			if content:find(before, 1, true) and content:find(after, 1, true) then
				break
			end
			assert(skynet.now() < deadline, "logger 未在截止时间前写入本次标记")
			skynet.sleep(1)
		end
	else
		skynet.sleep(5)
	end
	skynet.abort()
end)
