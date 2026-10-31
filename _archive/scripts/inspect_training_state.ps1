$ErrorActionPreference = "Continue"
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8
$OutputEncoding = [System.Text.Encoding]::UTF8

Write-Output "=== MACHINE ==="
hostname
nvidia-smi --query-gpu=utilization.gpu,memory.used,memory.total --format=csv,noheader
Get-Process | Where-Object { $_.ProcessName -like "*deep*" -or $_.ProcessName -like "*neural*" -or $_.ProcessName -like "*python*" } |
  Select-Object ProcessName,Id,CPU,StartTime | Format-Table -AutoSize

Write-Output "=== REPO ==="
Set-Location "D:\claude-code\c++"
git rev-parse --is-shallow-repository
git status --short
git log --oneline -5

Write-Output "=== DP SCRIPTS ==="
Set-Location "D:\claude-code\c++\routes\deep_protagonist"
Get-ChildItem "handoff\scripts" | Select-Object Name,Length,LastWriteTime | Format-Table -AutoSize
Write-Output "--- DP runs latest files ---"
Get-ChildItem "runs" -File | Sort-Object LastWriteTime -Descending | Select-Object -First 30 Name,Length,LastWriteTime | Format-Table -AutoSize
Write-Output "--- DP monitor ---"
if (Test-Path "handoff\scripts\monitor.ps1") {
  powershell -NoProfile -ExecutionPolicy Bypass -File "handoff\scripts\monitor.ps1"
}

Write-Output "=== PE SCRIPTS ==="
Set-Location "D:\claude-code\c++\routes\protagonist_ecology"
Get-ChildItem "handoff\scripts" | Select-Object Name,Length,LastWriteTime | Format-Table -AutoSize
Write-Output "--- PE configs ---"
Get-ChildItem "onepot_sD_warm_*.toml" | Sort-Object LastWriteTime -Descending | Select-Object -First 15 Name,Length,LastWriteTime | Format-Table -AutoSize
Write-Output "--- PE latest run dirs ---"
Get-ChildItem "data\runs" -Directory | Where-Object { $_.Name -like "onepot_sD_warm_*" } |
  Sort-Object LastWriteTime -Descending | Select-Object -First 15 FullName,LastWriteTime | Format-Table -AutoSize
Write-Output "--- PE monitor ---"
if (Test-Path "handoff\scripts\monitor.ps1") {
  powershell -NoProfile -ExecutionPolicy Bypass -File "handoff\scripts\monitor.ps1"
}
