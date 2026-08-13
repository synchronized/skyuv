param(
	[Parameter(Mandatory = $true)]
	[string]$Executable,

	[Parameter(Mandatory = $true)]
	[string]$Config
)

$ErrorActionPreference = "Stop"
$token = [Guid]::NewGuid().ToString("N")
$inputPath = Join-Path $env:TEMP "skyuv-client-stdin-$token.in"
$outputPath = Join-Path $env:TEMP "skyuv-client-stdin-$token.out.log"
$errorPath = Join-Path $env:TEMP "skyuv-client-stdin-$token.err.log"
$process = $null

try {
	[IO.File]::WriteAllText($inputPath, "alpha`n`n中文行`n", [Text.UTF8Encoding]::new($false))
	$process = Start-Process `
		-FilePath $Executable `
		-ArgumentList $Config `
		-NoNewWindow `
		-PassThru `
		-RedirectStandardInput $inputPath `
		-RedirectStandardOutput $outputPath `
		-RedirectStandardError $errorPath
	$deadline = [DateTime]::UtcNow.AddSeconds(8)
	$matched = $false
	do {
		Start-Sleep -Milliseconds 100
		if (Test-Path -LiteralPath $outputPath) {
			$output = [string](Get-Content -LiteralPath $outputPath -Raw)
			$matched = $output -like "*skyuv client.socket stdin 验证通过*"
		}
	} while (-not $matched -and -not $process.HasExited -and [DateTime]::UtcNow -lt $deadline)
	if (-not $matched) {
		$errorOutput = if (Test-Path -LiteralPath $errorPath) {
			Get-Content -LiteralPath $errorPath -Raw
		} else {
			""
		}
		throw "client.socket stdin 验证未观察到成功标志。`n$output`n$errorOutput"
	}
} finally {
	if ($null -ne $process -and -not $process.HasExited) {
		Stop-Process -Id $process.Id -Force
		$process.WaitForExit()
	}
	Remove-Item -LiteralPath $inputPath, $outputPath, $errorPath -ErrorAction SilentlyContinue
}
