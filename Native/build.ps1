# Builds BloomForge.aex (Release|x64). Usage: .\build.ps1 [-Configuration Debug] [-Install]
param(
    [string]$Configuration = "Release",
    [switch]$Install
)

$ErrorActionPreference = "Stop"
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) { throw "Visual Studio not found (no vswhere.exe). Install VS 2022 with 'Desktop development with C++'." }

$msbuild = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -find "MSBuild\**\Bin\MSBuild.exe" | Select-Object -First 1
if (-not $msbuild) { throw "MSBuild not found." }

& $msbuild "$PSScriptRoot\Win\BloomForge.vcxproj" /p:Configuration=$Configuration /p:Platform=x64 /m /nologo /v:minimal
if ($LASTEXITCODE -ne 0) { throw "Build failed." }

$aex = Join-Path $PSScriptRoot "..\build\$Configuration\Makiglow.aex"
Write-Host "Built: $((Resolve-Path $aex).Path)"

if ($Install) {
    $dst = "C:\Program Files\Adobe\Adobe After Effects 2026\Support Files\Plug-ins\Makimchi"
    New-Item -ItemType Directory -Force $dst | Out-Null
    Copy-Item $aex $dst -Force
    Write-Host "Installed to $dst"
}
