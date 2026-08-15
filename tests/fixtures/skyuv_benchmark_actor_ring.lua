local skynet = require "skynet.manager"

local warmup = tonumber(skynet.getenv "benchmark_warmup") or 2
local duration = tonumber(skynet.getenv "benchmark_duration") or 10
local iterations = tonumber(skynet.getenv "benchmark_iterations") or 5
local actor_count = tonumber(skynet.getenv "benchmark_actors") or 8
local result

local function percentile(values, fraction)
	table.sort(values)
	return values[math.max(1, math.ceil(#values * fraction))] or 0
end

local function run_once(first, seconds)
	result = nil
	skynet.send(first, "lua", "run", seconds)
	while not result do
		skynet.wait(first)
	end
	return table.unpack(result)
end

skynet.start(function()
	local nodes = {}
	for index = 1, actor_count do
		nodes[index] = skynet.newservice "skyuv_benchmark_actor_ring_node"
	end
	for index, node in ipairs(nodes) do
		skynet.call(node, "lua", "setup", nodes[index % actor_count + 1], index == 1,
			actor_count, skynet.self())
	end
	skynet.dispatch("lua", function(_, _, command, operations, elapsed, latencies)
		assert(command == "done")
		result = { operations, elapsed, latencies }
		skynet.wakeup(nodes[1])
	end)
	if warmup > 0 then
		run_once(nodes[1], warmup)
	end
	for iteration = 1, iterations do
		local operations, elapsed, latencies = run_once(nodes[1], duration)
		skynet.error(string.format(
			"SKYUV_ACTOR_RING_SAMPLE %d %d %.9f %.6f %.6f %.6f %.6f",
			iteration, operations, elapsed, percentile(latencies, 0.50),
			percentile(latencies, 0.95), percentile(latencies, 0.99), latencies[#latencies] or 0
		))
	end
	for _, node in ipairs(nodes) do
		skynet.kill(node)
	end
	skynet.timeout(1, skynet.abort)
end)
