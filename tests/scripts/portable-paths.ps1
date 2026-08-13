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
	Copy-Item -LiteralPath $PortableDirectory -Destination $modules -Recurse
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
