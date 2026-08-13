param(
	[Parameter(Mandatory = $true)]
	[string]$Executable,

	[Parameter(Mandatory = $true)]
	[string]$Node1Config,

	[Parameter(Mandatory = $true)]
	[string]$Node2Config,

	[Parameter(Mandatory = $true)]
	[string]$Node3Config
)

$ErrorActionPreference = "Stop"
$token = [Guid]::NewGuid().ToString("N")
$node1OutputPath = Join-Path $env:TEMP "skyuv-harbor-node1-$token.out.log"
$node1ErrorPath = Join-Path $env:TEMP "skyuv-harbor-node1-$token.err.log"
$node2OutputPath = Join-Path $env:TEMP "skyuv-harbor-node2-$token.out.log"
$node2ErrorPath = Join-Path $env:TEMP "skyuv-harbor-node2-$token.err.log"
$retryOutputPath = Join-Path $env:TEMP "skyuv-harbor-retry-$token.out.log"
$retryErrorPath = Join-Path $env:TEMP "skyuv-harbor-retry-$token.err.log"
$node3OutputPath = Join-Path $env:TEMP "skyuv-harbor-node3-$token.out.log"
$node3ErrorPath = Join-Path $env:TEMP "skyuv-harbor-node3-$token.err.log"
$node1 = $null
$node2 = $null
$retry = $null
$node3 = $null

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
	$node1 = Start-Process `
		-FilePath $Executable `
		-ArgumentList $Node1Config `
		-NoNewWindow `
		-PassThru `
		-RedirectStandardOutput $node1OutputPath `
		-RedirectStandardError $node1ErrorPath
	Wait-ForMarker $node1 $node1OutputPath "skyuv harbor 节点 1 已就绪" 8

	$node2 = Start-Process `
		-FilePath $Executable `
		-ArgumentList $Node2Config `
		-NoNewWindow `
		-PassThru `
		-RedirectStandardOutput $node2OutputPath `
		-RedirectStandardError $node2ErrorPath
	Wait-ForMarker $node2 $node2OutputPath "skyuv harbor 跨节点调用验证通过" 8

	Stop-Process -Id $node2.Id -Force
	$node2.WaitForExit()
	Wait-ForMarker $node1 $node1OutputPath "slave 2 is down" 8

	$retry = Start-Process `
		-FilePath $Executable `
		-ArgumentList $Node2Config `
		-NoNewWindow `
		-PassThru `
		-RedirectStandardOutput $retryOutputPath `
		-RedirectStandardError $retryErrorPath
	Wait-ForMarker $node1 $node1OutputPath "Slave 2 already register" 8
	if (-not $retry.WaitForExit(5000)) {
		throw "复用 harbor ID 的节点未按预期终止。"
	}

	$node3 = Start-Process `
		-FilePath $Executable `
		-ArgumentList $Node3Config `
		-NoNewWindow `
		-PassThru `
		-RedirectStandardOutput $node3OutputPath `
		-RedirectStandardError $node3ErrorPath
	Wait-ForMarker $node3 $node3OutputPath "skyuv harbor 跨节点调用验证通过" 8
	Stop-Process -Id $node3.Id -Force
	$node3.WaitForExit()
	Wait-ForMarker $node1 $node1OutputPath "slave 3 is down" 8
} catch {
	$node1Output = Read-Log $node1OutputPath
	$node1Error = Read-Log $node1ErrorPath
	$node2Output = Read-Log $node2OutputPath
	$node2Error = Read-Log $node2ErrorPath
	$retryOutput = Read-Log $retryOutputPath
	$retryError = Read-Log $retryErrorPath
	$node3Output = Read-Log $node3OutputPath
	$node3Error = Read-Log $node3ErrorPath
	throw "$($_.Exception.Message)`n节点 1：`n$node1Output`n$node1Error`n节点 2：`n$node2Output`n$node2Error`n重复 ID 节点：`n$retryOutput`n$retryError`n节点 3：`n$node3Output`n$node3Error"
} finally {
	if ($null -ne $node3 -and -not $node3.HasExited) {
		Stop-Process -Id $node3.Id -Force
		$node3.WaitForExit()
	}
	if ($null -ne $retry -and -not $retry.HasExited) {
		Stop-Process -Id $retry.Id -Force
		$retry.WaitForExit()
	}
	if ($null -ne $node2 -and -not $node2.HasExited) {
		Stop-Process -Id $node2.Id -Force
		$node2.WaitForExit()
	}
	if ($null -ne $node1 -and -not $node1.HasExited) {
		Stop-Process -Id $node1.Id -Force
		$node1.WaitForExit()
	}
	Remove-Item -LiteralPath `
		$node1OutputPath, $node1ErrorPath, $node2OutputPath, $node2ErrorPath, `
		$retryOutputPath, $retryErrorPath, $node3OutputPath, $node3ErrorPath `
		-ErrorAction SilentlyContinue
}
