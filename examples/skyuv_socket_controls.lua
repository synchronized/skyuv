local skynet = require "skynet.manager"
local driver = require "skynet.socketdriver"

local TYPE_DATA = 1
local TYPE_CONNECT = 2
local TYPE_CLOSE = 3
local TYPE_ACCEPT = 4
local TYPE_ERROR = 5

local listener
local client
local accepted
local paused = false
local resumed = false
local accepted_closed = false
local client_closed = false

local function finish_if_ready()
	if not (accepted_closed and client_closed) then
		return
	end
	driver.close(listener)
	skynet.error("skyuv TCP 控制语义基线验证通过")
	skynet.timeout(2, skynet.abort)
end

skynet.register_protocol {
	name = "socket",
	id = skynet.PTYPE_SOCKET,
	unpack = driver.unpack,
	dispatch = function(_, _, event_type, id, ud, data)
		if event_type == TYPE_CONNECT then
			if id == listener then
				client = assert(driver.connect("127.0.0.1", 25283))
			elseif id == client then
				-- 等 accepted start 后再发送，确保 pause 请求先进入控制队列。
			elseif id == accepted then
				driver.nodelay(accepted)
				driver.pause(accepted)
				paused = true
				driver.send(client, "paused-payload")
				skynet.timeout(10, function()
					assert(not resumed)
					resumed = true
					driver.start(accepted)
				end)
			else
				error("未知 CONNECT id")
			end
		elseif event_type == TYPE_ACCEPT then
			assert(id == listener)
			accepted = assert(ud)
			driver.start(accepted)
		elseif event_type == TYPE_DATA then
			assert(id == accepted)
			assert(paused and resumed, "pause 期间不应收到 DATA")
			assert(skynet.tostring(data, ud) == "paused-payload")
			skynet.trash(data, ud)
			skynet.error("TCP_CONTROL pause_resume")
			driver.shutdown(accepted)
		elseif event_type == TYPE_CLOSE then
			if id == accepted then
				accepted_closed = true
				skynet.error("TCP_CONTROL shutdown_close")
			elseif id == client then
				client_closed = true
			end
			finish_if_ready()
		elseif event_type == TYPE_ERROR then
			error("TCP 控制语义出现错误：" .. tostring(data))
		end
	end,
}

skynet.start(function()
	listener = assert(driver.listen("127.0.0.1", 25283, 16))
	driver.start(listener)
	skynet.timeout(500, function()
		error("TCP 控制语义基线超时")
	end)
end)
