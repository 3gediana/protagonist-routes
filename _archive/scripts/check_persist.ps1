$pe='D:\claude-code\c++\routes\protagonist_ecology'
Write-Output '=== GPU/proc ==='
nvidia-smi --query-gpu=memory.used,memory.total,utilization.gpu --format=csv,noheader,nounits
Get-CimInstance Win32_Process | Where-Object { $_.CommandLine -match 'onepot_sD_persist_50023\.toml' } | ForEach-Object { Write-Output ("running pid="+$_.ProcessId) }
$log=Join-Path $pe 'onepot_sD_persist_50023.log'
Write-Output '=== recent gen lines ==='
if (Test-Path $log) { Select-String -Path $log -Pattern 'Gen \d+ \| protagonist.*me_cov=' | ForEach-Object { $_.Line } | Select-Object -Last 8 }
Write-Output '=== tail ==='
if (Test-Path $log) { Get-Content $log -Tail 10 }
