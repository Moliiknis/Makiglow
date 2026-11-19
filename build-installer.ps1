# Builds release\Makiglow-1.0.0-Setup.exe. Usage: .\build-installer.ps1 [-SkipBuild]
# Needs Inno Setup 6 (winget install JRSoftware.InnoSetup), Visual Studio with C++ and the After Effects SDK (see README).
param([switch]$SkipBuild)
$ErrorActionPreference = "Stop"

if (-not $SkipBuild) {
    & "$PSScriptRoot\Native\build.ps1"
}

$iscc = @(
    "$env:LOCALAPPDATA\Programs\Inno Setup 6\ISCC.exe",
    "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe",
    "$env:ProgramFiles\Inno Setup 6\ISCC.exe"
) | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $iscc) { throw "Inno Setup 6 not found. Install it: winget install JRSoftware.InnoSetup" }

& $iscc /Qp "$PSScriptRoot\installer\Makiglow.iss"
if ($LASTEXITCODE -ne 0) { throw "Installer build failed." }
Write-Host "Built: $PSScriptRoot\release\Makiglow-1.0.0-Setup.exe"
