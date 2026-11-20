# Compiles and runs profile_test.cpp with the MSVC toolchain.
$ErrorActionPreference = "Stop"
$sdk = if ($env:AESDK) { $env:AESDK } else { "$env:USERPROFILE\Downloads\AfterEffectsSDK_27.0.0_win\AfterEffectsSDK\Examples" }
$vs = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -property installationPath
$obj = Join-Path $PSScriptRoot "obj"
New-Item -ItemType Directory -Force $obj | Out-Null

$includes = "/I`"$sdk\Headers`" /I`"$sdk\Headers\SP`" /I`"$sdk\Headers\Win`" /I`"$sdk\Resources`" /I`"$sdk\Util`""
$sources = "`"$PSScriptRoot\profile_test.cpp`" `"$sdk\Util\AEGP_SuiteHandler.cpp`" `"$sdk\Util\MissingSuiteError.cpp`""
$cmd = "`"$vs\VC\Auxiliary\Build\vcvars64.bat`" >nul 2>nul && cl /nologo /std:c++17 /EHsc /O2 /DMSWindows /DWIN32 /D_WINDOWS $includes $sources /Fo`"$obj\\`" /Fe`"$obj\profile_test.exe`" >`"$obj\build.log`""
cmd /c $cmd
if ($LASTEXITCODE -ne 0) { Get-Content "$obj\build.log"; throw "Test build failed." }
& "$obj\profile_test.exe"
