param(
	[Parameter(Mandatory = $true)]
	[string]$Executable,

	[Parameter(Mandatory = $true)]
	[string]$Node1Config,

	[Parameter(Mandatory = $true)]
	[string]$Node2Config
)

$ErrorActionPreference = "Stop"
$token = [Guid]::NewGuid().ToString("N")
$node1OutputPath = Join-Path $env:TEMP "skyuv-harbor-node1-$token.out.log"
$node1ErrorPath = Join-Path $env:TEMP "skyuv-harbor-node1-$token.err.log"
$node2OutputPath = Join-Path $env:TEMP "skyuv-harbor-node2-$token.out.log"
$node2ErrorPath = Join-Path $env:TEMP "skyuv-harbor-node2-$token.err.log"
$node1 = $null
$node2 = $null

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
} catch {
	$node1Output = Read-Log $node1OutputPath
	$node1Error = Read-Log $node1ErrorPath
	$node2Output = Read-Log $node2OutputPath
	$node2Error = Read-Log $node2ErrorPath
	throw "$($_.Exception.Message)`n节点 1：`n$node1Output`n$node1Error`n节点 2：`n$node2Output`n$node2Error"
} finally {
	if ($null -ne $node2 -and -not $node2.HasExited) {
		Stop-Process -Id $node2.Id -Force
		$node2.WaitForExit()
	}
	if ($null -ne $node1 -and -not $node1.HasExited) {
		Stop-Process -Id $node1.Id -Force
		$node1.WaitForExit()
	}
	Remove-Item -LiteralPath `
		$node1OutputPath, $node1ErrorPath, $node2OutputPath, $node2ErrorPath `
		-ErrorAction SilentlyContinue
}
