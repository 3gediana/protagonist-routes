$p = "D:\claude-code\c++\routes\deep_protagonist"
Set-Location $p
Write-Output "=== PWD ==="; (Get-Location).Path
Write-Output "=== build dirs / CMake ==="; Get-ChildItem -Name | Where-Object {$_ -match 'build|CMake|\.sln|\.bat|\.ps1'} | Select-Object -First 30
Write-Output "=== exe? ==="; Get-ChildItem -Recurse -Include *.exe -ErrorAction SilentlyContinue | Select-Object -First 10 FullName,Length,LastWriteTime
Write-Output "=== running train procs ==="; Get-Process | Where-Object {$_.ProcessName -match 'deep_protagonist|train|protagonist'} | Select-Object Id,ProcessName,StartTime
Write-Output "=== runs dir (latest) ==="; if(Test-Path "$p\runs"){ Get-ChildItem "$p\runs" | Sort-Object LastWriteTime -Descending | Select-Object -First 8 Name,LastWriteTime }
Write-Output "=== champion ckpts (*.pt newest) ==="; Get-ChildItem -Recurse -Include *.pt -ErrorAction SilentlyContinue | Sort-Object LastWriteTime -Descending | Select-Object -First 8 FullName,Length,LastWriteTime
