local skynet = require "skynet"

local next_node
local leader = false
local actor_count
local coordinator
local deadline
local operations
local latencies
local started

local function forward(hops, lap_started)
	skynet.send(next_node, "lua", "token", hops + 1, lap_started)
end

skynet.start(function()
	skynet.dispatch("lua", function(_, _, command, ...)
		if command == "setup" then
			next_node, leader, actor_count, coordinator = ...
			skynet.retpack()
		elseif command == "run" then
			local seconds = ...
			deadline = skynet.hpc() + seconds * 1000000000
			operations = 0
			latencies = {}
			started = skynet.hpc()
			forward(0, skynet.hpc())
		elseif command == "token" then
			local hops, lap_started = ...
			if leader and hops == actor_count then
				operations = operations + actor_count
				latencies[#latencies + 1] = (skynet.hpc() - lap_started) / 1000000
				if skynet.hpc() >= deadline then
					skynet.send(coordinator, "lua", "done", operations,
						(skynet.hpc() - started) / 1000000000, latencies)
				else
					forward(0, skynet.hpc())
				end
			else
				forward(hops, lap_started)
			end
		else
			error("未知环形 Actor 命令：" .. tostring(command))
		end
	end)
end)
