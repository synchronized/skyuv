[CmdletBinding()]
param(
    [ValidateSet("configure", "build", "test", "all")]
    [string]$Action = "all",

    [string]$Preset = "windows-msvc-debug",

    [ValidateSet("x64", "x86", "arm64")]
    [string]$Architecture = "x64",

    [ValidateRange(0, 2147483647)]
    [int]$Jobs = 0,

    [switch]$Fresh
)

$ErrorActionPreference = "Stop"

function Invoke-CheckedCommand {
    param(
        [Parameter(Mandatory)]
        [string]$Command,

        [Parameter(Mandatory)]
        [string[]]$Arguments
    )

    & $Command @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "命令执行失败（退出码 $LASTEXITCODE）：$Command $($Arguments -join ' ')"
    }
}

$vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path -LiteralPath $vswhere)) {
    throw "未找到 vswhere.exe，请安装 Visual Studio Installer 和 C++ 桌面开发工作负载。"
}

$installationPath = & $vswhere -latest -products * `
    -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
    -property installationPath
if ($LASTEXITCODE -ne 0 -or -not $installationPath) {
    throw "未找到包含 MSVC C++ 工具链的 Visual Studio。"
}

$vsDevCmd = Join-Path $installationPath.Trim() "Common7\Tools\VsDevCmd.bat"
if (-not (Test-Path -LiteralPath $vsDevCmd)) {
    throw "未找到 Visual Studio 开发环境脚本：$vsDevCmd"
}

# 批处理脚本无法修改父 PowerShell，因此读取并导入它生成的环境变量。
$environmentLines = & $env:ComSpec /d /s /c "`"$vsDevCmd`" -no_logo -arch=$Architecture -host_arch=x64 && set"
if ($LASTEXITCODE -ne 0) {
    throw "初始化 Visual Studio 开发环境失败。"
}

foreach ($line in $environmentLines) {
    if ($line -match '^([^=]+)=(.*)$') {
        Set-Item -Path "Env:$($Matches[1])" -Value $Matches[2]
    }
}

# CMake 会优先采用外部 LD；清除 Scoop/MSYS 等环境遗留，确保 MSVC 使用 link.exe。
Remove-Item -Path Env:LD -ErrorAction SilentlyContinue

$configureArguments = @("--preset", $Preset)
$buildArguments = @("--build", "--preset", $Preset)
if ($Fresh) {
    $configureArguments += "--fresh"
}
if ($Jobs -gt 0) {
    $buildArguments += @("-j", $Jobs)
}

switch ($Action) {
    "configure" { Invoke-CheckedCommand cmake $configureArguments }
    "build" { Invoke-CheckedCommand cmake $buildArguments }
    "test" { Invoke-CheckedCommand ctest @("--preset", $Preset) }
    "all" {
        Invoke-CheckedCommand cmake $configureArguments
        Invoke-CheckedCommand cmake $buildArguments
        Invoke-CheckedCommand ctest @("--preset", $Preset)
    }
}
