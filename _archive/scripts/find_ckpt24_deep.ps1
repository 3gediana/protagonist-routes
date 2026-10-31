$base = 'D:\claude-code\c++\routes\protagonist_ecology\data\runs\onepot_sD_warm_50024'
Write-Output "exists: $(Test-Path $base)"
Get-ChildItem $base -Directory | ForEach-Object {
  Write-Output $_.FullName
  $ckpts = Get-ChildItem $_.FullName -Directory -Filter 'checkpoint_gen*' | Sort-Object Name
  if ($ckpts.Count -gt 0) {
    $ckpts | Select-Object -Last 5 | ForEach-Object { Write-Output ("  " + $_.Name) }
  } else { Write-Output "  (no checkpoints)" }
}
