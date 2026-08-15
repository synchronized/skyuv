local skynet = require "skynet.manager"

local warmup = tonumber(skynet.getenv "benchmark_warmup") or 2
local duration = tonumber(skynet.getenv "benchmark_duration") or 10
local iterations = tonumber(skynet.getenv "benchmark_iterations") or 5
local producer_count = tonumber(skynet.getenv "benchmark_producers") or 4

local function run_once(consumer, producers, seconds)
	skynet.call(consumer, "lua", "reset")
	local completed = 0
	local sent = 0
	local started = skynet.hpc()
	for index, producer in ipairs(producers) do
		skynet.fork(function()
			local count = skynet.call(producer, "lua", "run", consumer, index, seconds)
			sent = sent + count
			completed = completed + 1
			if completed == #producers then
				skynet.wakeup(producers)
			end
		end)
	end
	while completed < #producers do
		skynet.wait(producers)
	end
	local elapsed = (skynet.hpc() - started) / 1000000000
	local received, counts = skynet.call(consumer, "lua", "snapshot")
	local minimum = counts[1] or 0
	local maximum = minimum
	for index = 2, producer_count do
		minimum = math.min(minimum, counts[index] or 0)
		maximum = math.max(maximum, counts[index] or 0)
	end
	assert(sent == received)
	return sent, received, elapsed, minimum, maximum
end

skynet.start(function()
	local consumer = skynet.newservice "skyuv_benchmark_actor_consumer"
	local producers = {}
	for index = 1, producer_count do
		producers[index] = skynet.newservice "skyuv_benchmark_actor_producer"
	end
	if warmup > 0 then
		run_once(consumer, producers, warmup)
	end
	for iteration = 1, iterations do
		local sent, received, elapsed, minimum, maximum = run_once(consumer, producers, duration)
		skynet.error(string.format(
			"SKYUV_ACTOR_MULTI_SAMPLE %d %d %d %.9f %d %d",
			iteration, sent, received, elapsed, minimum, maximum
		))
	end
	for _, producer in ipairs(producers) do
		skynet.kill(producer)
	end
	skynet.kill(consumer)
	skynet.timeout(1, skynet.abort)
end)
