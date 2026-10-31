$pe='D:\claude-code\c++\routes\protagonist_ecology'
Write-Output '=== GPU ==='
nvidia-smi --query-gpu=memory.used,memory.total,utilization.gpu --format=csv,noheader,nounits
foreach ($seed in @(50022,50023,50024)) {
  Write-Output "=== seed $seed recent gens ==="
  $log=Join-Path $pe "onepot_sD_stable_$seed.log"
  if (!(Test-Path $log)) { Write-Output 'missing log'; continue }
  $lines=Select-String -Path $log -Pattern 'Gen \d+ \| protagonist.*me_cov=' | ForEach-Object { $_.Line } | Select-Object -Last 8
  foreach ($line in $lines) { Write-Output $line }
}
