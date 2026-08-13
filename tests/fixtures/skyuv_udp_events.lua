local skynet = require "skynet.manager"
local socket = require "skynet.socket"

local server4
local client4
local server6
local client6
local ipv4_done = false
local ipv6_done = false

local function finish_if_ready()
	if not (ipv4_done and ipv6_done) then
		return
	end
	socket.close(server4)
	socket.close(client4)
	socket.close(server6)
	socket.close(client6)
	skynet.error("skyuv UDP 原版事件基线验证通过")
	skynet.timeout(2, skynet.abort)
end

local function address(from, expected_size)
	local host, port = socket.udp_address(from)
	assert(#from == expected_size)
	assert(port > 0)
	return host, port
end

skynet.start(function()
	server4 = socket.udp(function(data, from)
		local host = address(from, 7)
		assert(host == "127.0.0.1")
		assert(data == "udp4-request")
		skynet.error("UDP_EVENT ipv4_receive address_size=7")
		assert(socket.sendto(server4, from, "udp4-response"))
	end, "127.0.0.1", 25384)
	client4 = socket.udp(function(data, from)
		local host, port = address(from, 7)
		assert(host == "127.0.0.1" and port == 25384)
		assert(data == "udp4-response")
		ipv4_done = true
		skynet.error("UDP_EVENT ipv4_reply")
		finish_if_ready()
	end)
	socket.udp_connect(client4, "127.0.0.1", 25384)
	assert(socket.write(client4, "udp4-request"))

	server6 = socket.udp_listen("::1", 25385, function(data, from)
		local host = address(from, 19)
		assert(host == "::1")
		assert(data == "udp6-request")
		skynet.error("UDP_EVENT ipv6_receive address_size=19")
		assert(socket.sendto(server6, from, "udp6-response"))
	end)
	client6 = socket.udp_dial("::1", 25385, function(data, from)
		local host, port = address(from, 19)
		assert(host == "::1" and port == 25385)
		assert(data == "udp6-response")
		ipv6_done = true
		skynet.error("UDP_EVENT ipv6_reply")
		finish_if_ready()
	end)
	assert(socket.write(client6, "udp6-request"))

	skynet.timeout(500, function()
		error("UDP 原版事件基线超时")
	end)
end)
