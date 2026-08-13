local skynet = require "skynet.manager"
local socket = require "client.socket"

skynet.start(function()
	local lines = {}
	local status
	for _ = 1, 5000 do
		local line
		line, status = socket.readstdin()
		if line ~= nil then
			lines[#lines + 1] = line
		elseif status ~= nil then
			break
		else
			socket.usleep(1000)
		end
	end
	assert(#lines == 3, "stdin 行数不一致")
	assert(lines[1] == "alpha", "stdin 第一行不一致")
	assert(lines[2] == "", "stdin 空行未保留")
	assert(lines[3] == "中文行", "stdin UTF-8 行不一致")
	assert(status == "eof", "stdin 未报告 EOF")
	skynet.error("skyuv client.socket stdin 验证通过")
	skynet.error("skyuv CMake 启动验证通过")
	skynet.sleep(2)
	skynet.abort()
end)
