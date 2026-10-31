$ErrorActionPreference='Stop'
$pe='D:\claude-code\c++\routes\protagonist_ecology'
$root="$pe\data\runs\onepot_sD_warm_50025"
Write-Output "=== 50025 run dirs ==="
Get-ChildItem $root -Directory | Sort-Object LastWriteTime -Descending | Select-Object FullName,LastWriteTime | Format-Table -AutoSize
foreach ($run in (Get-ChildItem $root -Directory | Sort-Object LastWriteTime -Descending | Select-Object -First 4)) {
  Write-Output "--- $($run.FullName) ---"
  Get-ChildItem $run.FullName -Directory -Filter 'checkpoint_gen*' | Sort-Object Name | Select-Object Name,LastWriteTime | Format-Table -AutoSize
  $f=Join-Path $run.FullName 'fitness.csv'
  if (Test-Path $f) { Import-Csv $f | Select-Object -First 1 generation,me_coverage,best_fitness; Import-Csv $f | Select-Object -Last 1 generation,me_coverage,best_fitness }
}
