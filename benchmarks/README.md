# 基准工具

基准与功能测试分开：这里的程序采集数据，不在共享 CI Runner 上设置绝对性能门槛。
结果格式由 `result.schema.json` 定义，通用参数和环境元数据由 `protocol.py` 提供。

## TCP 长连接 echo

先启动 skyuv 或原版 Skynet 的固定长度 echo 服务，再运行同一个客户端：

```powershell
python benchmarks/tcp_echo.py `
  --implementation skyuv `
  --build-type Release `
  --allocator system `
  --compiler "MSVC 19.x" `
  --message-size 64 `
  --output build/benchmarks/tcp-echo.json
```

默认连接 `127.0.0.1:25281`，预热 2 秒并测量 5 轮，每轮 10 秒。每次往返都会验证
响应内容；结果保存每轮吞吐和延迟 p50、p95、p99、最大值，并汇总中位吞吐。

## TCP 短连接

短连接基准复用相同的固定长度 echo 服务，但每次操作都会重新建立连接、完成一次 echo 并关闭：

```powershell
python benchmarks/tcp_short_connection.py `
  --implementation skyuv `
  --build-type Release `
  --allocator system `
  --compiler "MSVC 19.x" `
  --message-size 64 `
  --output build/benchmarks/tcp-short-connection.json
```

该场景用于观察 connect、accept、close 和 socket ID 回收的综合成本。正式对照时必须与长连接
基准使用相同主机、编译模式、分配器、消息尺寸和测量轮数。

## TCP 慢接收端与背压

背压服务持续写入固定内容，客户端先暂停读取，再按指定间隔小块读取：

```powershell
python benchmarks/tcp_backpressure.py `
  --implementation skyuv `
  --build-type Release `
  --allocator system `
  --compiler "MSVC 19.x" `
  --initial-pause 0.2 `
  --read-delay 0.005 `
  --read-size 4096 `
  --output build/benchmarks/tcp-backpressure.json
```

结果记录每轮读取次数、接收字节数和接收吞吐。服务端日志中的
`SKYUV_BACKPRESSURE_WARNING` 表明 Skynet 写队列已跨过 warning 阈值；正式运行需同时保存服务端日志。

## UDP request/reply

UDP 基准使用顺序 request/reply 建立可重复的基础数据：

```powershell
python benchmarks/udp_request_reply.py `
  --implementation skyuv `
  --build-type Release `
  --allocator system `
  --compiler "MSVC 19.x" `
  --message-size 64 `
  --output build/benchmarks/udp-request-reply.json
```

每个响应都会校验来源地址和完整内容。结果记录发送、接收、超时丢包、包速率以及成功响应的
p50、p95、p99 和最大延迟；首期固定单个在途请求，后续再用独立场景扩展并发窗口。

## Actor ping-pong

Actor 基准由 Python 启动器运行两个独立 Skynet Lua 服务，测量跨服务 `call/return` 往返：

```powershell
python benchmarks/actor_ping_pong.py `
  --implementation skyuv `
  --build-type Release `
  --allocator system `
  --compiler "MSVC 19.x" `
  --executable build/windows-vs2022-release/src/Release/skyuv_skynet_portable.exe `
  --config build/windows-vs2022-release/3rd/skyuv-benchmark-actor.conf `
  --output build/benchmarks/actor-ping-pong.json
```

Lua 服务使用纳秒级 `skynet.hpc()` 记录每次往返延迟；Python 汇总每轮吞吐，以及各轮延迟
分位数的中位数。测试原版 Skynet 时只需替换可执行文件和等价配置，启动器与 Lua 服务保持不变。
