local skynet = require "skynet.manager"

local warmup = tonumber(skynet.getenv "benchmark_warmup") or 2
local duration = tonumber(skynet.getenv "benchmark_duration") or 1
local iterations = tonumber(skynet.getenv "benchmark_iterations") or 5
local timer_count = tonumber(skynet.getenv "benchmark_timer_count") or 10000

local function percentile(values, fraction)
	table.sort(values)
	return values[math.max(1, math.ceil(#values * fraction))] or 0
end

local function run_once(delay_seconds, count)
	local completed = 0
	local deviations = {}
	local delay_ticks = math.max(1, math.floor(delay_seconds * 100 + 0.5))
	local started = skynet.hpc()
	local expected = started + delay_ticks * 10000000
	for _ = 1, count do
		skynet.timeout(delay_ticks, function()
			completed = completed + 1
			deviations[completed] = math.max(0, skynet.hpc() - expected) / 1000000
			if completed == count then
				skynet.wakeup(deviations)
			end
		end)
	end
	while completed < count do
		skynet.wait(deviations)
	end
	return (skynet.hpc() - started) / 1000000000, deviations
end

skynet.start(function()
	if warmup > 0 then
		run_once(warmup, math.min(timer_count, 1000))
	end
	for iteration = 1, iterations do
		local elapsed, deviations = run_once(duration, timer_count)
		skynet.error(string.format(
			"SKYUV_TIMER_BURST_SAMPLE %d %d %.9f %.6f %.6f %.6f %.6f",
			iteration, #deviations, elapsed, percentile(deviations, 0.50),
			percentile(deviations, 0.95), percentile(deviations, 0.99), deviations[#deviations] or 0
		))
	end
	skynet.timeout(1, skynet.abort)
end)
