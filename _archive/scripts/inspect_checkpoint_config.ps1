$pe='D:\claude-code\c++\routes\protagonist_ecology'
foreach ($seed in @(50022,50023,50024)) {
  Write-Output "=== seed $seed toml runtime/checkpoint ==="
  Select-String -Path "$pe\onepot_sD_warm_$seed.toml" -Pattern 'resume_checkpoint_path|generations|checkpoint_every|line_sigma|mutation_boost|hunt_bin_size|hunt_max_bins|cvt_enabled' | ForEach-Object { $_.Line }
  $run=(Get-ChildItem "$pe\data\runs\onepot_sD_warm_$seed" -Directory | Sort-Object LastWriteTime -Descending | Select-Object -First 1)
  Write-Output "run=$($run.FullName)"
  Get-ChildItem $run.FullName -Directory -Filter 'checkpoint_gen*' | Sort-Object Name | Select-Object -Last 5 Name,LastWriteTime | Format-Table -AutoSize
}
