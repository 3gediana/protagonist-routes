$pe='D:\claude-code\c++\routes\protagonist_ecology'
$log=Join-Path $pe 'onepot_sD_persist_ckpt_50023.log'
Write-Output '=== GPU/proc ==='
nvidia-smi --query-gpu=memory.used,memory.total,utilization.gpu --format=csv,noheader,nounits
Get-CimInstance Win32_Process | Where-Object { $_.CommandLine -match 'onepot_sD_persist_ckpt_50023\.toml' } | ForEach-Object { Write-Output ("running pid="+$_.ProcessId) }
Write-Output '=== gen lines ==='
if (Test-Path $log) { Select-String -Path $log -Pattern 'Gen \d+ \| protagonist.*me_cov=' | ForEach-Object { $_.Line } | Select-Object -Last 12 }
Write-Output '=== saved ckpts ==='
$base='D:\claude-code\c++\routes\protagonist_ecology\data\runs\onepot_sD_warm_50023'
$run=Get-ChildItem $base -Directory | Sort-Object LastWriteTime -Descending | Select-Object -First 1
if ($run) { Write-Output $run.FullName; Get-ChildItem $run.FullName -Directory -Filter 'checkpoint_gen*' | Sort-Object Name | Select-Object Name,LastWriteTime | Format-Table -AutoSize }
