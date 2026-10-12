-- 仅使用统一模块目录，避免旧的 client/socket 产物掩盖加载问题。
package.cpath = assert(arg[1])
local socket = require "client.socket"
local crypt = require "client.crypt"
assert(type(socket.connect) == "function")
assert(type(socket.select) == "function")
local connected, reason = pcall(socket.connect, "127.0.0.1", 0)
assert(not connected)
assert(tostring(reason):find("ECONNREFUSED", 1, true) or
	tostring(reason):find("ETIMEDOUT", 1, true) or
	tostring(reason):find("EADDRNOTAVAIL", 1, true), tostring(reason))
assert(not pcall(socket.connect, "127.0.0.1", 65536))
assert(not pcall(socket.select, {}, nil, math.huge))
assert(crypt.hexencode(crypt.sha1("abc")) ==
	"a9993e364706816aba3e25717850c26c9cd0d89d")
local data = "skyuv\0client"
assert(crypt.base64decode(crypt.base64encode(data)) == data)
assert(crypt.desdecode("12345678", crypt.desencode("12345678", data)) == data)
assert(#crypt.randomkey() == 8)
