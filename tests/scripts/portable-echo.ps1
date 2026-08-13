param(
	[Parameter(Mandatory = $true)]
	[string]$Executable,

	[Parameter(Mandatory = $true)]
	[string]$Config
)

$ErrorActionPreference = "Stop"
$token = [Guid]::NewGuid().ToString("N")
$outputPath = Join-Path $env:TEMP "skyuv-echo-$token.out.log"
$errorPath = Join-Path $env:TEMP "skyuv-echo-$token.err.log"
$process = $null
$client = $null

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
		if (Test-Path -LiteralPath $outputPath) {
			$output = [string](Get-Content -LiteralPath $outputPath -Raw)
			$ready = $output -like "*skyuv echo 已就绪*"
		}
	} while (-not $ready -and -not $process.HasExited -and [DateTime]::UtcNow -lt $deadline)

	if (-not $ready) {
		$errorOutput = if (Test-Path -LiteralPath $errorPath) {
			Get-Content -LiteralPath $errorPath -Raw
		} else {
			""
		}
		throw "Windows Skynet echo 服务未就绪。`n$output`n$errorOutput"
	}

	$client = [System.Net.Sockets.TcpClient]::new()
	$client.Connect("127.0.0.1", 25280)
	$stream = $client.GetStream()
	$payload = [Text.Encoding]::UTF8.GetBytes("skyuv-echo-baseline`n")
	$stream.Write($payload, 0, $payload.Length)
	$received = [byte[]]::new($payload.Length)
	$offset = 0
	while ($offset -lt $received.Length) {
		$count = $stream.Read($received, $offset, $received.Length - $offset)
		if ($count -eq 0) {
			break
		}
		$offset += $count
	}
	$equal = [System.Collections.StructuralComparisons]::StructuralEqualityComparer.Equals(
		$payload,
		$received
	)
	if ($offset -ne $payload.Length -or -not $equal) {
		$expectedHex = [Convert]::ToHexString($payload)
		$receivedHex = [Convert]::ToHexString($received, 0, $offset)
		$output = if (Test-Path -LiteralPath $outputPath) {
			Get-Content -LiteralPath $outputPath -Raw
		} else {
			""
		}
		$errorOutput = if (Test-Path -LiteralPath $errorPath) {
			Get-Content -LiteralPath $errorPath -Raw
		} else {
			""
		}
		throw "Windows Skynet echo 内容不一致：收到 $offset/$($payload.Length) 字节，期望 $expectedHex，实际 $receivedHex。`n$output`n$errorOutput"
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
