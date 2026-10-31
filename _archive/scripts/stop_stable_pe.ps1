$procs=Get-CimInstance Win32_Process | Where-Object { $_.CommandLine -match 'onepot_sD_stable_5002[2-4]\.toml' }
foreach ($p in $procs) {
  Write-Output "Stopping pid=$($p.ProcessId) cmd=$($p.CommandLine)"
  Stop-Process -Id $p.ProcessId -Force -ErrorAction SilentlyContinue
}
Start-Sleep -Seconds 2
Write-Output 'remaining:'
Get-CimInstance Win32_Process | Where-Object { $_.CommandLine -match 'onepot_sD_stable_5002[2-4]\.toml' } | ForEach-Object { Write-Output $_.ProcessId }
Write-Output 'gpu:'
nvidia-smi --query-gpu=memory.used,memory.total,utilization.gpu --format=csv,noheader,nounits
