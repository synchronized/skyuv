param(
	[Parameter(Mandatory = $true)]
	[string]$Executable,

	[Parameter(Mandatory = $true)]
	[string]$Config
)

$ErrorActionPreference = "Stop"
$token = [Guid]::NewGuid().ToString("N")
$outputPath = Join-Path $env:TEMP "skyuv-gate-$token.out.log"
$errorPath = Join-Path $env:TEMP "skyuv-gate-$token.err.log"
$process = $null
$client = $null

function New-Frame([byte[]]$Payload) {
	$frame = [byte[]]::new($Payload.Length + 2)
	$frame[0] = [byte](($Payload.Length -shr 8) -band 0xff)
	$frame[1] = [byte]($Payload.Length -band 0xff)
	[Array]::Copy($Payload, 0, $frame, 2, $Payload.Length)
	return $frame
}

function Read-Exact($Stream, [int]$Size) {
	$result = [byte[]]::new($Size)
	$offset = 0
	while ($offset -lt $Size) {
		$count = $Stream.Read($result, $offset, $Size - $offset)
		if ($count -eq 0) {
			throw "连接提前关闭：收到 $offset/$Size 字节。"
		}
		$offset += $count
	}
	return $result
}

try {
	$process = Start-Process `
		-FilePath $Executable `
		-ArgumentList $Config `
		-NoNewWindow `
		-PassThru `
		-RedirectStandardOutput $outputPath `
		-RedirectStandardError $errorPath

	$deadline = [DateTime]::UtcNow.AddSeconds(5)
	$ready = $false
	do {
		Start-Sleep -Milliseconds 100
		$output = if (Test-Path -LiteralPath $outputPath) {
			[string](Get-Content -LiteralPath $outputPath -Raw)
		} else {
			""
		}
		$ready = $output -like "*skyuv gate 已就绪*"
	} while (-not $ready -and -not $process.HasExited -and [DateTime]::UtcNow -lt $deadline)

	if (-not $ready) {
		$errorOutput = if (Test-Path -LiteralPath $errorPath) {
			Get-Content -LiteralPath $errorPath -Raw
		} else {
			""
		}
		throw "Windows Skynet gate 服务未就绪。`n$output`n$errorOutput"
	}

	$client = [System.Net.Sockets.TcpClient]::new()
	$client.ReceiveTimeout = 5000
	$client.Connect("127.0.0.1", 25284)
	$stream = $client.GetStream()

	$payloads = [System.Collections.Generic.List[byte[]]]::new()
	$payloads.Add([Text.Encoding]::UTF8.GetBytes("skyuv-split-跨平台"))
	$payloads.Add([Text.Encoding]::UTF8.GetBytes("skyuv-coalesced-a"))
	$payloads.Add([byte[]](0, 1, 2, 0, 254, 255))
	$frames = [System.Collections.Generic.List[byte[]]]::new()
	foreach ($payload in $payloads) {
		$frames.Add((New-Frame $payload))
	}

	$stream.Write($frames[0], 0, 1)
	$stream.Write($frames[0], 1, $frames[0].Length - 1)
	$combined = [byte[]]::new($frames[1].Length + $frames[2].Length)
	[Array]::Copy($frames[1], 0, $combined, 0, $frames[1].Length)
	[Array]::Copy($frames[2], 0, $combined, $frames[1].Length, $frames[2].Length)
	$stream.Write($combined, 0, $combined.Length)

	foreach ($frame in $frames) {
		$received = Read-Exact $stream $frame.Length
		$equal = [System.Collections.StructuralComparisons]::StructuralEqualityComparer.Equals(
			$frame,
			$received
		)
		if (-not $equal) {
			throw "Windows Skynet gate 回显内容不一致：期望 $([Convert]::ToHexString($frame))，实际 $([Convert]::ToHexString($received))。"
		}
	}
} finally {
	if ($null -ne $client) {
		$client.Dispose()
	}
	if ($null -ne $process -and -not $process.HasExited) {
		Stop-Process -Id $process.Id -Force
		$process.WaitForExit()
	}
	Remove-Item -LiteralPath $outputPath, $errorPath -ErrorAction SilentlyContinue
}
