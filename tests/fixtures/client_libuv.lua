package.cpath = assert(arg[1])
local port = assert(tonumber(arg[2]))
local socket = require "client.socket"
local payload = string.rep("skyuv\0binary", 16384)
assert(socket.select({}, {}, 0) == false)
assert(socket.select(0.001) == false)
assert(not pcall(socket.select, {}))
assert(not pcall(socket.select, {123456789}, nil, 0))
local ok, reason = pcall(socket.connect, "127.0.0.1", port, 0)
assert(not ok and reason:find("ETIMEDOUT", 1, true), reason)
for _ = 1, 10 do
	local id = socket.connect("127.0.0.1", port, 2)
	local readable, writable = socket.select({}, {id, id}, 1)
	assert(#readable == 0 and writable[1] == id and writable[2] == id)
	socket.send(id, payload)
	socket.shutdown(id, "w")
	socket.shutdown(id, "w")
	assert(not pcall(socket.send, id, "after shutdown"))
	local chunks = {}
	while true do
		local ready = socket.select({id}, nil, 5)
		assert(ready and ready[1] == id)
		local data = socket.recv(id)
		assert(data ~= nil)
		if data == "" then break end
		chunks[#chunks + 1] = data
	end
	assert(table.concat(chunks) == payload)
	socket.close(id)
	socket.close(id)
	assert(not pcall(socket.recv, id))
end
-- 独立解释器卸载模块时有存活 TCP 连接，GC 必须等待关闭而不悬挂。
for _ = 1, 5 do
	socket = require "client.socket"
	socket.connect("127.0.0.1", port, 2)
	package.loaded["client.socket"] = nil
	socket = nil
	collectgarbage("collect")
end
print("client libuv lifecycle ok")
