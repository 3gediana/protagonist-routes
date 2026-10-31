$pe='D:\claude-code\c++\routes\protagonist_ecology'
Write-Output '=== GPU ==='
nvidia-smi --query-gpu=memory.used,memory.total,utilization.gpu --format=csv,noheader,nounits
Write-Output '=== stable process count ==='
$procs=Get-CimInstance Win32_Process | Where-Object { $_.CommandLine -match 'onepot_sD_stable_5002[2-4]\.toml' }
Write-Output @($procs).Count
$procs | ForEach-Object { if ($_.CommandLine -match 'onepot_sD_stable_(5002\d)\.toml') { Write-Output ("running seed="+$Matches[1]+" pid="+$_.ProcessId) } }
foreach ($seed in @(50022,50023,50024)) {
  $log=Join-Path $pe "onepot_sD_stable_$seed.log"
  Write-Output "=== seed $seed log ==="
  if (Test-Path $log) { Get-Content $log -Tail 20 } else { Write-Output 'missing log' }
}
