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
local finished = false

local function finish_if_ready()
	if finished or not (accepted_closed and client_closed) then
		return
	end
	finished = true
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
				if not client then
					client = assert(driver.connect("127.0.0.1", 25283))
				end
			elseif accepted and id == accepted then
				if resumed then
					-- start 恢复已连接 socket 时，原版会产生 transfer OPEN。
					return
				end
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
				-- connect 的 OPEN 可能先于 driver.connect 返回并写入 client。
				client = client or id
				assert(id == client)
				-- 等 accepted start 后再发送，确保 pause 请求先进入控制队列。
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
			if id == client and accepted_closed then
				-- 强制 shutdown 后，对端可能报告 reset，也可能报告 close。
				client_closed = true
				finish_if_ready()
			else
				error("TCP 控制语义出现错误：" .. tostring(data))
			end
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
