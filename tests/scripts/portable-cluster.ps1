param(
	[Parameter(Mandatory = $true)]
	[string]$Executable,

	[Parameter(Mandatory = $true)]
	[string]$ProviderConfig,

	[Parameter(Mandatory = $true)]
	[string]$ConsumerConfig
)

$ErrorActionPreference = "Stop"
$token = [Guid]::NewGuid().ToString("N")
$providerOutputPath = Join-Path $env:TEMP "skyuv-cluster-provider-$token.out.log"
$providerErrorPath = Join-Path $env:TEMP "skyuv-cluster-provider-$token.err.log"
$consumerOutputPath = Join-Path $env:TEMP "skyuv-cluster-consumer-$token.out.log"
$consumerErrorPath = Join-Path $env:TEMP "skyuv-cluster-consumer-$token.err.log"
$provider = $null
$consumer = $null

function Read-Log([string]$Path) {
	if (Test-Path -LiteralPath $Path) {
		return [string](Get-Content -LiteralPath $Path -Raw)
	}
	return ""
}

function Wait-ForMarker($Process, [string]$Path, [string]$Marker, [int]$Seconds) {
	$deadline = [DateTime]::UtcNow.AddSeconds($Seconds)
	do {
		Start-Sleep -Milliseconds 100
		$output = Read-Log $Path
		if ($output -like "*$Marker*") {
			return
		}
	} while (-not $Process.HasExited -and [DateTime]::UtcNow -lt $deadline)
	throw "等待日志标记超时：$Marker`n$output"
}

try {
	$provider = Start-Process `
		-FilePath $Executable `
		-ArgumentList $ProviderConfig `
		-NoNewWindow `
		-PassThru `
		-RedirectStandardOutput $providerOutputPath `
		-RedirectStandardError $providerErrorPath
	Wait-ForMarker $provider $providerOutputPath "skyuv cluster 提供节点已就绪" 8

	$consumer = Start-Process `
		-FilePath $Executable `
		-ArgumentList $ConsumerConfig `
		-NoNewWindow `
		-PassThru `
		-RedirectStandardOutput $consumerOutputPath `
		-RedirectStandardError $consumerErrorPath
	Wait-ForMarker $consumer $consumerOutputPath "skyuv cluster 双节点 RPC 验证通过" 8
	Stop-Process -Id $consumer.Id -Force
	$consumer.WaitForExit()
} catch {
	$providerOutput = Read-Log $providerOutputPath
	$providerError = Read-Log $providerErrorPath
	$consumerOutput = Read-Log $consumerOutputPath
	$consumerError = Read-Log $consumerErrorPath
	throw "$($_.Exception.Message)`n提供节点：`n$providerOutput`n$providerError`n消费节点：`n$consumerOutput`n$consumerError"
} finally {
	if ($null -ne $consumer -and -not $consumer.HasExited) {
		Stop-Process -Id $consumer.Id -Force
		$consumer.WaitForExit()
	}
	if ($null -ne $provider -and -not $provider.HasExited) {
		Stop-Process -Id $provider.Id -Force
		$provider.WaitForExit()
	}
	Remove-Item -LiteralPath `
		$providerOutputPath, $providerErrorPath, $consumerOutputPath, $consumerErrorPath `
		-ErrorAction SilentlyContinue
}
