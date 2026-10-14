param(
	[Parameter(Mandatory = $true)]
	[string]$Executable,

	[Parameter(Mandatory = $true)]
	[string]$PortableDirectory,

	[Parameter(Mandatory = $true)]
	[string]$SkynetSource,

	[Parameter(Mandatory = $true)]
	[string]$FixtureSource
)

$ErrorActionPreference = "Stop"
$token = [Guid]::NewGuid().ToString("N")
$root = $env:TEMP
$modules = Join-Path $root "skyuv path modules-$token"
$config = Join-Path $root "skyuv-paths-$token.conf"

try {
	# 只搬运本夹具启动链需要的模块；无关模块和调试符号不属于路径验收。
	$requiredModules = @(
		"cservice/logger.dll", "cservice/snlua.dll", "cservice/harbor.dll",
		"luaclib/skynet.dll", "luaclib/client.dll"
	)
	$prepareWatch = [Diagnostics.Stopwatch]::StartNew()
	foreach ($relativeModule in $requiredModules) {
		Write-Output "准备路径测试模块：$relativeModule"
		$source = Join-Path $PortableDirectory $relativeModule
		$destination = Join-Path $modules $relativeModule
		New-Item -ItemType Directory -Path (Split-Path -Parent $destination) -Force | Out-Null
		Copy-Item -LiteralPath $source -Destination $destination
	}
	Write-Output "路径测试模块准备完成，耗时 $($prepareWatch.Elapsed.TotalSeconds) 秒"
	$modulePath = $modules.Replace("\", "/")
	$sourcePath = $SkynetSource.Replace("\", "/")
	$fixturePath = $FixtureSource.Replace("\", "/")
	$content = @"
thread = 2
logger = nil
harbor = 0
bootstrap = "snlua bootstrap"
start = "skyuv_client_module"
cpath = "$modulePath/cservice/?.dll"
lua_path = "$sourcePath/lualib/?.lua;$sourcePath/lualib/?/init.lua"
lua_cpath = "$modulePath/luaclib/?.dll"
luaservice = "$sourcePath/service/?.lua;$fixturePath/?.lua"
lualoader = "$sourcePath/lualib/loader.lua"
"@
	[IO.File]::WriteAllText($config, $content, [Text.UTF8Encoding]::new($false))
	& "$PSScriptRoot/portable-smoke.ps1" `
		-Executable $Executable `
		-Config (Split-Path -Leaf $config) `
		-WorkingDirectory $root
} finally {
	Remove-Item -LiteralPath $modules -Recurse -Force -ErrorAction SilentlyContinue
	Remove-Item -LiteralPath $config -Force -ErrorAction SilentlyContinue
}
