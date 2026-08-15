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
