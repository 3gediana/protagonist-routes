$paths = @(
  'D:/claude-code/c++/routes/protagonist_ecology/data/runs/onepot_sC_long_50002/20260603-124316/checkpoint_gen79',
  'D:/claude-code/c++/routes/protagonist_ecology/data/runs/onepot_sC_long_50003/20260603-124316/checkpoint_gen79'
)
foreach ($p in $paths) {
  $exists = Test-Path $p
  $nbin = 0
  if ($exists) { $nbin = @(Get-ChildItem -Path $p -Filter 'genome_*.bin' -ErrorAction SilentlyContinue).Count }
  $tw = Test-Path (Join-Path $p 'triple_world\main_world_manifest.json')
  Write-Output ($p + '  exists=' + $exists + '  genome_bins=' + $nbin + '  triple_world_manifest=' + $tw)
}
# also list what timestamp dirs actually exist for 50002/50003
foreach ($s in @('50002','50003')) {
  $base = 'D:/claude-code/c++/routes/protagonist_ecology/data/runs/onepot_sC_long_' + $s
  Write-Output ('--- ' + $base + ' timestamp dirs ---')
  if (Test-Path $base) { Get-ChildItem $base -Directory | ForEach-Object { $_.Name } } else { Write-Output 'BASE MISSING' }
}
