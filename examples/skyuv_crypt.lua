local skynet = require "skynet"
local crypt = require "skynet.crypt"

skynet.start(function()
	assert(crypt.hexencode(crypt.sha1("abc")) ==
		"a9993e364706816aba3e25717850c26c9cd0d89d")
	assert(crypt.hexencode(crypt.hmac_sha1("key",
		"The quick brown fox jumps over the lazy dog")) ==
		"de7c9b85b8b78aa6bc8a7a36f70a90701c9db4d9")

	local binary = "skyuv\0跨平台"
	assert(crypt.base64decode(crypt.base64encode(binary)) == binary)
	assert(crypt.hexdecode(crypt.hexencode(binary)) == binary)
	assert(crypt.xor_str("\x0f\xf0\xaa", "\x33\x55") == "\x3c\xa5\x99")

	local key = "12345678"
	local source = "DES 往返消息"
	local encrypted = crypt.desencode(key, source, crypt.padding.iso7816_4)
	assert(encrypted ~= source)
	assert(crypt.desdecode(key, encrypted, crypt.padding.iso7816_4) == source)
	assert(#crypt.randomkey() == 8)
	assert(not pcall(crypt.base64decode, "非法输入"))
	assert(not pcall(crypt.xor_str, "data", ""))

	skynet.error("skyuv crypt 与 SHA-1 验证通过")
	skynet.error("skyuv CMake 启动验证通过")
	skynet.exit()
end)
