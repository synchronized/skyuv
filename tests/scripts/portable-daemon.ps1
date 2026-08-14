param(
	[Parameter(Mandatory = $true)]
	[string]$Executable,

	[Parameter(Mandatory = $true)]
	[string]$Config
)

$ErrorActionPreference = "Stop"
$start = [Diagnostics.ProcessStartInfo]::new()
$start.FileName = $Executable
$start.ArgumentList.Add($Config)
$start.UseShellExecute = $false
$start.RedirectStandardOutput = $true
$start.RedirectStandardError = $true
$process = [Diagnostics.Process]::Start($start)

try {
	if (-not $process.WaitForExit(5000)) {
		throw "Windows daemon 不支持路径未及时退出。"
	}
	$output = $process.StandardOutput.ReadToEnd() + $process.StandardError.ReadToEnd()
	if ($process.ExitCode -eq 0) {
		throw "Windows daemon 配置被错误接受。`n$output"
	}
	if ($output -notlike "*daemon mode is not supported on Windows*") {
		throw "Windows daemon 失败未返回明确诊断。`n$output"
	}
} finally {
	if (-not $process.HasExited) {
		$process.Kill()
		$process.WaitForExit()
	}
	$process.Dispose()
}
