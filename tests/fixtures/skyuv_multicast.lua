local skynet = require "skynet"
local multicast = require "skynet.multicast"

local mode = ...

if mode == "subscriber" then
	local subscribed_channel

	skynet.start(function()
		skynet.dispatch("lua", function(_, _, command, channel_id, owner)
			if command == "subscribe" then
				subscribed_channel = multicast.new {
					channel = channel_id,
					dispatch = function(_, source, value)
						skynet.send(owner, "lua", "received", "subscriber", source, value)
					end,
				}
				subscribed_channel:subscribe()
				skynet.ret(skynet.pack())
			elseif command == "unsubscribe" then
				subscribed_channel:unsubscribe()
				skynet.ret(skynet.pack())
			elseif command == "stop" then
				skynet.exit()
			else
				error("未知 multicast 测试命令：" .. tostring(command))
			end
		end)
	end)
else
	local received = {}
	local waiting

	local function record(receiver, source, value)
		assert(source == skynet.self())
		received[receiver .. ":" .. value] = true
		if waiting then
			skynet.wakeup(waiting)
			waiting = nil
		end
	end

	local function wait_for(predicate)
		while not predicate() do
			waiting = coroutine.running()
			skynet.wait(waiting)
		end
	end

	skynet.start(function()
		skynet.dispatch("lua", function(_, _, command, receiver, source, value)
			assert(command == "received")
			record(receiver, source, value)
		end)

		local channel = multicast.new {
			dispatch = function(_, source, value)
				record("owner", source, value)
			end,
		}
		channel:subscribe()
		local subscriber = skynet.newservice("skyuv_multicast", "subscriber")
		skynet.call(
			subscriber,
			"lua",
			"subscribe",
			channel.channel,
			skynet.self()
		)

		channel:publish("first")
		wait_for(function()
			return received["owner:first"] and received["subscriber:first"]
		end)

		skynet.call(subscriber, "lua", "unsubscribe")
		channel:publish("second")
		wait_for(function()
			return received["owner:second"]
		end)
		skynet.sleep(1)
		assert(not received["subscriber:second"])

		channel:delete()
		skynet.send(subscriber, "lua", "stop")
		skynet.error("skyuv multicast 引用计数与订阅生命周期验证通过")
		skynet.error("skyuv CMake 启动验证通过")
		skynet.exit()
	end)
end
