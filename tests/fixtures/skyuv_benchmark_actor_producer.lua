local skynet = require "skynet"

skynet.start(function()
	skynet.dispatch("lua", function(_, _, command, consumer, producer, duration)
		assert(command == "run")
		local count = 0
		local deadline = skynet.hpc() + duration * 1000000000
		while skynet.hpc() < deadline do
			skynet.send(consumer, "lua", "message", producer)
			count = count + 1
			if count % 256 == 0 then
				skynet.sleep(0)
			end
		end
		-- 同一来源的消息保持顺序；barrier 返回即表示此前消息均已消费。
		skynet.call(consumer, "lua", "barrier", producer)
		skynet.retpack(count)
	end)
end)
