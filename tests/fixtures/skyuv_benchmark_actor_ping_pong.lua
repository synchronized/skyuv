local skynet = require "skynet.manager"

local warmup = tonumber(skynet.getenv "benchmark_warmup") or 2
local duration = tonumber(skynet.getenv "benchmark_duration") or 10
local iterations = tonumber(skynet.getenv "benchmark_iterations") or 5

local function percentile(values, fraction)
	table.sort(values)
	local index = math.max(1, math.ceil(#values * fraction))
	return values[index] or 0
end

local function run_for(peer, seconds, measure)
	local latencies = {}
	local operations = 0
	local started = skynet.hpc()
	local deadline = started + seconds * 1000000000
	while skynet.hpc() < deadline do
		local operation_started = skynet.hpc()
		skynet.call(peer, "lua", "ping")
		operations = operations + 1
		if measure then
			latencies[operations] = (skynet.hpc() - operation_started) / 1000000
		end
	end
	return operations, (skynet.hpc() - started) / 1000000000, latencies
end

skynet.start(function()
	local peer = skynet.newservice "skyuv_benchmark_actor_peer"
	if warmup > 0 then
		run_for(peer, warmup, false)
	end
	for iteration = 1, iterations do
		local operations, elapsed, latencies = run_for(peer, duration, true)
		local p50 = percentile(latencies, 0.50)
		local p95 = percentile(latencies, 0.95)
		local p99 = percentile(latencies, 0.99)
		local maximum = latencies[#latencies] or 0
		skynet.error(string.format(
			"SKYUV_ACTOR_SAMPLE %d %d %.9f %.6f %.6f %.6f %.6f",
			iteration, operations, elapsed, p50, p95, p99, maximum
		))
	end
	skynet.kill(peer)
	skynet.timeout(1, skynet.abort)
end)
