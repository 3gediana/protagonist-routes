$pe='D:\claude-code\c++\routes\protagonist_ecology'
foreach ($seed in @(50022,50023,50024)) {
  Write-Output "=== seed $seed ==="
  $pattern="onepot_sD_stable_$seed"
  $rundir=Join-Path $pe "data\runs\$pattern"
  if (Test-Path $rundir) {
    $dirs=Get-ChildItem $rundir -Directory | Sort-Object Name -Descending | Select-Object -First 1
    if ($dirs) {
      Write-Output "run dir: $($dirs.Name)"
      $csvs=Get-ChildItem $dirs.FullName -Filter '*.csv' -ErrorAction SilentlyContinue
      Write-Output "csv count: $($csvs.Count)"
      foreach ($c in $csvs) { Write-Output "  $($c.Name) size=$($c.Length)" }
      $ckpts=Get-ChildItem $dirs.FullName -Filter 'checkpoint_gen*' -Directory -ErrorAction SilentlyContinue
      Write-Output "checkpoints: $($ckpts.Count)"
      foreach ($c in $ckpts) { Write-Output "  $($c.Name)" }
    }
  } else { Write-Output 'no run dir' }
  $log=Join-Path $pe "onepot_sD_stable_$seed.log"
  if (Test-Path $log) {
    $genlines=Select-String -Path $log -Pattern 'gen \d+|GEN \d+|generation \d+|GenComplete|EvolSummary|coverage' -CaseSensitive:$false | Select-Object -Last 5
    foreach ($g in $genlines) { Write-Output $g.Line }
  }
}
