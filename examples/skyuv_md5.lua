local skynet = require "skynet"
local md5 = require "md5"

skynet.start(function()
	assert(md5.sumhexa("") == "d41d8cd98f00b204e9800998ecf8427e")
	assert(md5.sumhexa("abc") == "900150983cd24fb0d6963f7d28e17f72")
	assert(md5.sumhexa("skyuv-跨平台") == "36007be434820f3a71d56c31005649ea")
	assert(md5.hmacmd5("The quick brown fox jumps over the lazy dog", "key") ==
		"80070713463e7749b90c2dc24911e275")
	assert(md5.exor("\x0f\xf0", "\x33\x55") == "\x3c\xa5")

	local source = "包含零字节\0与 UTF-8 的消息"
	local encrypted = md5.crypt(source, "skyuv-key", "fixed-seed")
	assert(encrypted ~= source)
	assert(md5.decrypt(encrypted, "skyuv-key") == source)
	assert(not pcall(md5.exor, "short", "longer"))
	assert(not pcall(md5.crypt, source, string.rep("k", 257), "seed"))

	skynet.error("skyuv md5 摘要与加解密验证通过")
	skynet.error("skyuv CMake 启动验证通过")
	skynet.exit()
end)
