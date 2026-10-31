$ErrorActionPreference='Continue'
$pe   = 'D:\claude-code\c++\routes\protagonist_ecology'
$tdir = 'D:\claude-code\c++\routes\_devin_tasks'
$ldir = Join-Path $pe '_devin_pe_logs'
New-Item -ItemType Directory -Force -Path $tdir | Out-Null
New-Item -ItemType Directory -Force -Path $ldir | Out-Null

# new -> source(known-good, currently running)
$map = @{ 50013 = 50012; 50014 = 50012 }

foreach ($new in ($map.Keys | Sort-Object)) {
  $src = $map[$new]
  $srcToml = Join-Path $pe ("onepot_sC_long_{0}.toml" -f $src)
  $dstToml = Join-Path $pe ("onepot_sC_long_{0}.toml" -f $new)
  if (-not (Test-Path $srcToml)) { Write-Output ("MISSING src toml: " + $srcToml); continue }
  (Get-Content $srcToml -Raw).Replace([string]$src, [string]$new) | Set-Content -Encoding ASCII $dstToml
  Write-Output ("wrote " + $dstToml)

  $bat = Join-Path $tdir ("pe_{0}.bat" -f $new)
  $batBody = @"
@echo off
set PATH=D:\claude-code\c++\routes\deep_protagonist\third_party\libtorch\lib;D:\NVIDIA\CUDA\v12.8\bin;D:\claude-code\c++\build_ninja_cuda\bin;%PATH%
cd /d D:\claude-code\c++\routes\protagonist_ecology
D:\claude-code\c++\build_ninja_cuda\bin\neural-eco-protagonist-rhythm.exe --config onepot_sC_long_$new.toml > D:\claude-code\c++\routes\protagonist_ecology\_devin_pe_logs\pe_$new.out 2> D:\claude-code\c++\routes\protagonist_ecology\_devin_pe_logs\pe_$new.err
"@
  Set-Content -Encoding ASCII -Path $bat -Value $batBody
  Write-Output ("wrote " + $bat)

  $task = "devin_pe_$new"
  cmd /c "schtasks /create /tn $task /tr ""cmd /c $bat"" /sc once /st 00:00 /f" 2>&1 | Out-String | Write-Output
  cmd /c "schtasks /run /tn $task" 2>&1 | Out-String | Write-Output
  Write-Output ("launched task " + $task)
}
Start-Sleep -Seconds 14
Write-Output ("pe_procs=" + (@(Get-Process neural-eco-protagonist-rhythm -ErrorAction SilentlyContinue).Count))
