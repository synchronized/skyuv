param(
	[Parameter(Mandatory = $true)]
	[string]$Executable,

	[Parameter(Mandatory = $true)]
	[string]$Config
)

$ErrorActionPreference = "Stop"
$token = [Guid]::NewGuid().ToString("N")
$outputPath = Join-Path $env:TEMP "skyuv-cgate-$token.out.log"
$errorPath = Join-Path $env:TEMP "skyuv-cgate-$token.err.log"
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

function Read-Output {
	if (Test-Path -LiteralPath $outputPath) {
		return [string](Get-Content -LiteralPath $outputPath -Raw)
	}
	return ""
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
	do {
		Start-Sleep -Milliseconds 100
		$output = Read-Output
		$ready = $output -like "*skyuv C gate 已就绪*"
	} while (-not $ready -and -not $process.HasExited -and [DateTime]::UtcNow -lt $deadline)

	if (-not $ready) {
		throw "Windows Skynet C gate 服务未就绪。`n$output"
	}

	$client = [System.Net.Sockets.TcpClient]::new()
	$client.ReceiveTimeout = 5000
	$client.Connect("127.0.0.1", 25285)
	$stream = $client.GetStream()
	$payload = [Text.Encoding]::UTF8.GetBytes("skyuv-c-gate-跨平台")
	$frame = New-Frame $payload
	$stream.Write($frame, 0, 1)
	$stream.Write($frame, 1, $frame.Length - 1)
	$received = Read-Exact $stream $frame.Length
	if (-not [System.Collections.StructuralComparisons]::StructuralEqualityComparer.Equals($frame, $received)) {
		throw "Windows Skynet C gate 回显内容不一致。"
	}

	$client.Dispose()
	$client = $null
	$deadline = [DateTime]::UtcNow.AddSeconds(5)
	do {
		Start-Sleep -Milliseconds 100
		$output = Read-Output
		$closed = $output -like "*skyuv C gate 关闭验证通过*"
	} while (-not $closed -and -not $process.HasExited -and [DateTime]::UtcNow -lt $deadline)
	if (-not $closed) {
		throw "Windows Skynet C gate 未完成关闭流程。`n$output"
	}
} catch {
	$output = Read-Output
	$errorOutput = if (Test-Path -LiteralPath $errorPath) {
		[string](Get-Content -LiteralPath $errorPath -Raw)
	} else {
		""
	}
	throw "$($_.Exception.Message)`n$output`n$errorOutput"
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
