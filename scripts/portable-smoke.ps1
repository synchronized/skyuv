param(
	[Parameter(Mandatory = $true)]
	[string]$Executable,

	[Parameter(Mandatory = $true)]
	[string]$Config
)

$ErrorActionPreference = "Stop"
$token = [Guid]::NewGuid().ToString("N")
$outputPath = Join-Path $env:TEMP "skyuv-smoke-$token.out.log"
$errorPath = Join-Path $env:TEMP "skyuv-smoke-$token.err.log"
$process = $null

try {
	$process = Start-Process `
		-FilePath $Executable `
		-ArgumentList $Config `
		-NoNewWindow `
		-PassThru `
		-RedirectStandardOutput $outputPath `
		-RedirectStandardError $errorPath

	$deadline = [DateTime]::UtcNow.AddSeconds(5)
	$matched = $false
	do {
		Start-Sleep -Milliseconds 100
		if (Test-Path -LiteralPath $outputPath) {
			$output = [string](Get-Content -LiteralPath $outputPath -Raw)
			$matched = $output -like "*skyuv CMake 启动验证通过*"
		}
	} while (-not $matched -and -not $process.HasExited -and [DateTime]::UtcNow -lt $deadline)

	if (-not $matched) {
		$errorOutput = if (Test-Path -LiteralPath $errorPath) {
			Get-Content -LiteralPath $errorPath -Raw
		} else {
			""
		}
		throw "Windows Skynet 启动冒烟未观察到成功标志。`n$errorOutput"
	}
} finally {
	if ($null -ne $process -and -not $process.HasExited) {
		Stop-Process -Id $process.Id -Force
		$process.WaitForExit()
	}
	Remove-Item -LiteralPath $outputPath, $errorPath -ErrorAction SilentlyContinue
}
