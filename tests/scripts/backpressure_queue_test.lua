-- 模拟慢接收端、恢复读取与断开，验证负载不会在等待排空时继续堆积发送。
local fixture = assert(arg[1])
for iteration = 1, 20 do
	local queued, peak, writes, sleeps, closed = 0, 0, 0, 0, 0
	local drained = false
	local socket = {}
	function socket.start(fd, callback)
		if callback then callback(2) end
	end
	function socket.listen() return 1 end
	function socket.readline() return "start" end
	function socket.warning() end
	function socket.close() closed = closed + 1 end
	function socket.write(_, data)
		assert(sleeps < 200, "慢接收端未停止发送")
		queued = queued + #data
		peak = math.max(peak, queued)
		writes = writes + 1
		return true
	end
	function socket.netstat()
		if drained then return {} end
		return {{id = 2, wbuffer = queued}}
	end
	local skynet = {}
	function skynet.start(fn) fn() end
	function skynet.fork(fn, ...) fn(...) end
	function skynet.error() end
	function skynet.sleep(ticks)
		assert(ticks > 0, "应让出时间等待网络线程")
		sleeps = sleeps + 1
		if sleeps == 50 then
			assert(writes == 64, "停读期间发送队列仍在增长")
			queued = 0
		elseif sleeps == 100 then
			assert(writes == 128, "恢复读取后未重新发送或再次限流失败")
			drained = true
		end
	end
	package.loaded["skynet"] = skynet
	package.loaded["skynet.socket"] = socket
	assert(loadfile(fixture))()
	assert(peak <= 4 * 1024 * 1024, "待发送数据超过预算")
	assert(closed == 1, "断开后未关闭连接")
end
