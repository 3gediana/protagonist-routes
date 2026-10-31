$pe='D:\claude-code\c++\routes\protagonist_ecology'
foreach ($seed in @(50021,50022,50023,50024,50025)) {
  Write-Output "=== $seed ==="
  Select-String -Path "$pe\onepot_sD_warm_$seed.toml" -Pattern 'me_neat|archive_breeding|cvt|bin_size|max_bins|mutation_boost|line_sigma|init_generations' | ForEach-Object { $_.Line }
}
