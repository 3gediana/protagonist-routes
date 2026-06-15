$ErrorActionPreference = 'Continue'
$dp = 'D:\claude-code\c++\routes\deep_protagonist'
$bd = Join-Path $dp 'build_d122_v2'
Write-Output "=== build dir (first 20) ==="
Get-ChildItem $bd -Name -ErrorAction SilentlyContinue | Select-Object -First 20
$hasNinja = Test-Path (Join-Path $bd 'build.ninja')
$hasSln   = Test-Path (Join-Path $bd 'deep_protagonist.sln')
Write-Output ("hasNinja=" + $hasNinja + " hasSln=" + $hasSln)

# locate VS dev cmd via vswhere
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
Write-Output "=== BUILD START ==="
cmd /c $cmd 2>&1 | Tee-Object -FilePath $log | Select-Object -Last 30
Write-Output "=== BUILD END ==="
$exe = Join-Path $bd 'bin\deep_protagonist_train.exe'
if (Test-Path $exe) { Write-Output ("exe LastWrite: " + (Get-Item $exe).LastWriteTime) } else { Write-Output "EXE MISSING" }
