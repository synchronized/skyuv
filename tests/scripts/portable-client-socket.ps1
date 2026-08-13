param(
	[Parameter(Mandatory = $true)]
	[string]$Executable,

	[Parameter(Mandatory = $true)]
	[string]$Config
)

$ErrorActionPreference = "Stop"
$token = [Guid]::NewGuid().ToString("N")
$outputPath = Join-Path $env:TEMP "skyuv-client-socket-$token.out.log"
$errorPath = Join-Path $env:TEMP "skyuv-client-socket-$token.err.log"
$listener = [System.Net.Sockets.TcpListener]::new([Net.IPAddress]::Loopback, 25286)
$process = $null
$client = $null

try {
	$listener.Start()
	$process = Start-Process `
		-FilePath $Executable `
		-ArgumentList $Config `
		-NoNewWindow `
		-PassThru `
		-RedirectStandardOutput $outputPath `
		-RedirectStandardError $errorPath
	$accept = $listener.AcceptTcpClientAsync()
	if (-not $accept.Wait(5000)) {
		throw "client.socket 未在超时前建立连接。"
	}
	$client = $accept.Result
	$stream = $client.GetStream()
	$buffer = [byte[]]::new(4096)
	$count = $stream.Read($buffer, 0, $buffer.Length)
	if ($count -le 0) {
		throw "client.socket 未发送测试数据。"
	}
	$stream.Write($buffer, 0, $count)
	# 等待客户端执行写关闭，再关闭服务端，使客户端观察到 recv 返回空串。
	while ($stream.Read($buffer, 0, $buffer.Length) -gt 0) {
	}
	$client.Dispose()
	$client = $null

	$deadline = [DateTime]::UtcNow.AddSeconds(5)
	$matched = $false
	do {
		Start-Sleep -Milliseconds 100
		if (Test-Path -LiteralPath $outputPath) {
			$output = [string](Get-Content -LiteralPath $outputPath -Raw)
			$matched = $output -like "*skyuv client.socket 回环验证通过*"
		}
	} while (-not $matched -and -not $process.HasExited -and [DateTime]::UtcNow -lt $deadline)
	if (-not $matched) {
		$errorOutput = if (Test-Path -LiteralPath $errorPath) {
			Get-Content -LiteralPath $errorPath -Raw
		} else {
			""
		}
		throw "client.socket 回环验证未观察到成功标志。`n$output`n$errorOutput"
	}
	if (-not $process.WaitForExit(5000)) {
		throw "client.socket 验证成功后节点未正常退出。"
	}
} finally {
	if ($null -ne $client) {
		$client.Dispose()
	}
	$listener.Stop()
	if ($null -ne $process -and -not $process.HasExited) {
		Stop-Process -Id $process.Id -Force
		$process.WaitForExit()
	}
	Remove-Item -LiteralPath $outputPath, $errorPath -ErrorAction SilentlyContinue
}
