$ErrorActionPreference='Stop'
$pe='D:\claude-code\c++\routes\protagonist_ecology'
$out='D:\claude-code\c++\pe_csv_pack'
Remove-Item $out -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Path $out | Out-Null
foreach ($seed in @(50021,50022,50023,50024,50025)) {
  $root="$pe\data\runs\onepot_sD_warm_$seed"
  $run=(Get-ChildItem $root -Directory | Sort-Object LastWriteTime -Descending | Select-Object -First 1).FullName
  foreach ($csv in @('fitness.csv','protagonist_behaviors.csv','evolution.csv')) {
    $src=Join-Path $run $csv
    if (Test-Path $src) { Copy-Item $src (Join-Path $out ("${seed}_$csv")) }
  }
}
Write-Output $out
