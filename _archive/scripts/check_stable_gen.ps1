$pe='D:\claude-code\c++\routes\protagonist_ecology'
Write-Output '=== GPU ==='
nvidia-smi --query-gpu=memory.used,memory.total,utilization.gpu --format=csv,noheader,nounits
Write-Output '=== processes ==='
Get-CimInstance Win32_Process | Where-Object { $_.CommandLine -match 'onepot_sD_stable_5002[2-4]\.toml' } | ForEach-Object { if ($_.CommandLine -match 'onepot_sD_stable_(5002\d)\.toml') { Write-Output ("running seed="+$Matches[1]+" pid="+$_.ProcessId) } }
foreach ($seed in @(50022,50023,50024)) {
  Write-Output "=== seed $seed gen lines ==="
  $f=Join-Path $pe "onepot_sD_stable_$seed.log"
  if (Test-Path $f) {
    Select-String -Path $f -Pattern '(GEN|generation|me_cov|me_coverage|CVT|cvt|archive|Archive)' -CaseSensitive:$false | Select-Object -Last 10 | ForEach-Object { Write-Output $_.Line }
    Write-Output "--- tail 5 ---"
    Get-Content $f -Tail 5
  } else { Write-Output 'missing log' }
}
