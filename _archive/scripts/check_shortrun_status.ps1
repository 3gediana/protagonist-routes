Get-CimInstance Win32_Process | Where-Object { $_.CommandLine -match 'onepot_sD_shortrun' } | ForEach-Object { Write-Output "pid=$($_.ProcessId)" }
nvidia-smi --query-gpu=memory.used,memory.total --format=csv,noheader,nounits
$pe='D:\claude-code\c++\routes\protagonist_ecology'
foreach ($seed in @(50022,50023,50024)) {
  $log = Join-Path $pe "onepot_sD_shortrun_$seed.log"
  Write-Output "=== $seed ==="
  if (Test-Path $log) {
    Select-String -Path $log -Pattern 'Gen \d+ \| protagonist.*me_cov=' | ForEach-Object { $_.Line } | Select-Object -Last 3
    Get-Content $log -Tail 3
  } else { Write-Output 'no log' }
}
