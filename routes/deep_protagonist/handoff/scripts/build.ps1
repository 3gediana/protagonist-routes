<#
.SYNOPSIS
  Compile the DP training binary (deep_protagonist_train.exe) into build_d122_v2.
.DESCRIPTION
  Finds the Visual Studio dev environment via vswhere, then runs the cmake build.
  Output exe: build_d122_v2\bin\deep_protagonist_train.exe
  Build log:  runs\dp_build.log
.EXAMPLE
  powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1
#>
$ErrorActionPreference = 'Continue'
$dp = 'D:\claude-code\c++\routes\deep_protagonist'
$bd = Join-Path $dp 'build_d122_v2'

$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vsdev = $null
if (Test-Path $vswhere) {
  $inst = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
  if ($inst) { $vsdev = Join-Path $inst 'Common7\Tools\VsDevCmd.bat' }
}
Write-Output ("vsdev=" + $vsdev)
$log = Join-Path $dp 'runs\dp_build.log'
if ($vsdev -and (Test-Path $vsdev)) {
  $cmd = 'call "' + $vsdev + '" -arch=amd64 -host_arch=amd64 >NUL && cmake --build "' + $bd + '" --target deep_protagonist_train -j 16'
} else {
  $cmd = 'cmake --build "' + $bd + '" --target deep_protagonist_train -j 16'
}
Write-Output "=== BUILD START (tail 30 lines) ==="
cmd /c $cmd 2>&1 | Tee-Object -FilePath $log | Select-Object -Last 30
$exe = Join-Path $bd 'bin\deep_protagonist_train.exe'
if (Test-Path $exe) { Write-Output ("BUILD_OK exe LastWrite: " + (Get-Item $exe).LastWriteTime) } else { Write-Output "EXE MISSING -- check $log" }
