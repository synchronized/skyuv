local skynet = require "skynet"
local skynet_core = require "skynet.core"
local driver = require "skynet.socketdriver"

local TYPE_DATA = 1
local TYPE_CONNECT = 2
local TYPE_CLOSE = 3
local TYPE_ACCEPT = 4
local TYPE_ERROR = 5

local listener
local client
local accepted
local failed
local sequence = 0
local events = {}
local closing = false

local function record(name)
	sequence = sequence + 1
	events[name] = sequence
	skynet.error("TCP_EVENT " .. name)
end

local function require_before(first, second)
	assert(events[first], "缺少事件：" .. first)
	assert(events[second], "缺少事件：" .. second)
	assert(events[first] < events[second], first .. " 应早于 " .. second)
end

local function finish_if_ready()
	if closing then
		return
	end
	if not (events.client_close and events.accepted_close and events.connect_error) then
		return
	end
	closing = true
	require_before("listener_open", "listener_accept")
	require_before("listener_accept", "accepted_open")
	require_before("client_open", "accepted_data")
	require_before("accepted_data", "client_data")
	require_before("client_data", "client_close")
	require_before("client_data", "accepted_close")
	driver.close(listener)
	skynet.error("skyuv TCP 原始事件基线验证通过")
	skynet.timeout(10, skynet.abort)
end

skynet.register_protocol {
	name = "socket",
	id = skynet.PTYPE_SOCKET,
	unpack = driver.unpack,
	dispatch = function(_, _, event_type, id, ud, data)
		if event_type == TYPE_CONNECT then
			if id == listener then
				record("listener_open")
				client = assert(driver.connect("127.0.0.1", 25282))
			elseif id == client then
				record("client_open")
				assert(driver.send(client, "client-payload"))
			elseif id == accepted then
				record("accepted_open")
			else
				error("未知 CONNECT id")
			end
		elseif event_type == TYPE_ACCEPT then
			assert(id == listener)
			assert(ud and ud > 0)
			accepted = ud
			record("listener_accept")
			driver.start(accepted)
		elseif event_type == TYPE_DATA then
			local payload = skynet.tostring(data, ud)
			skynet_core.trash(data, ud)
			if id == accepted then
				assert(payload == "client-payload")
				record("accepted_data")
				assert(driver.send(accepted, "server-payload"))
			elseif id == client then
				assert(payload == "server-payload")
				record("client_data")
				driver.close(client)
			else
				error("未知 DATA id")
			end
		elseif event_type == TYPE_CLOSE then
			if id == client then
				record("client_close")
			elseif id == accepted then
				record("accepted_close")
			elseif id == listener then
				record("listener_close")
			end
			finish_if_ready()
		elseif event_type == TYPE_ERROR then
			assert(id == failed)
			assert(type(data) == "string" and #data > 0)
			record("connect_error")
			driver.shutdown(failed)
			finish_if_ready()
		else
			error("首期基线出现未预期事件类型：" .. tostring(event_type))
		end
	end,
}

skynet.start(function()
	listener = assert(driver.listen("127.0.0.1", 25282, 16))
	driver.start(listener)
	failed = assert(driver.connect("127.0.0.1", 1))
	skynet.timeout(500, function()
		error("TCP 原始事件基线超时")
	end)
end)
