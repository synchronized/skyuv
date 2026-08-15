local skynet = require "skynet"

skynet.start(function()
	skynet.dispatch("lua", function(_, source, command)
		assert(command == "ping")
		skynet.ret(skynet.pack())
	end)
end)
