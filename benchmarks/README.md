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
