$ErrorActionPreference = 'Continue'
$dp = 'D:\claude-code\c++\routes\deep_protagonist'
$bd = Join-Path $dp 'build_devin_20260610'   # the LIVE 6/22 build dir (truly incremental)
$jobs = if ($args.Count -ge 1) { [int]$args[0] } else { 6 }
Write-Output ("build dir = " + $bd + "  -j " + $jobs)
$hasNinja = Test-Path (Join-Path $bd 'build.ninja')
Write-Output ("hasNinja=" + $hasNinja)

$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vsdev = $null
if (Test-Path $vswhere) {
  $inst = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
  if ($inst) { $vsdev = Join-Path $inst 'Common7\Tools\VsDevCmd.bat' }
}
Write-Output ("vsdev=" + $vsdev)

$log = Join-Path $dp 'runs\dp_build_d1.log'
if ($vsdev -and (Test-Path $vsdev)) {
  $cmd = 'call "' + $vsdev + '" -arch=amd64 -host_arch=amd64 >NUL && cmake --build "' + $bd + '" --target deep_protagonist_train -j ' + $jobs
} else {
  $cmd = 'cmake --build "' + $bd + '" --target deep_protagonist_train -j ' + $jobs
}
Write-Output "=== BUILD START ==="
cmd /c $cmd 2>&1 | Tee-Object -FilePath $log | Select-Object -Last 40
Write-Output "=== BUILD END ==="
$exe = Join-Path $bd 'bin\deep_protagonist_train.exe'
if (Test-Path $exe) { Write-Output ("exe LastWrite: " + (Get-Item $exe).LastWriteTime) } else { Write-Output "EXE MISSING" }
