$ErrorActionPreference='Stop'
$pe='D:\claude-code\c++\routes\protagonist_ecology'
$seed=50023
$src=Join-Path $pe "onepot_sD_stable_$seed.toml"
$dst=Join-Path $pe "onepot_sD_persist_$seed.toml"
$text=Get-Content $src -Raw
$text=$text -replace 'scenario_name = "[^"]+"', 'scenario_name = "onepot_sD_persist_50023"'
$text=$text -replace 'resume_checkpoint_path = "[^"]+"', 'resume_checkpoint_path = "D:/claude-code/c++/routes/protagonist_ecology/data/runs/onepot_sD_warm_50023/20260607-184224/checkpoint_gen329"'
$text=$text -replace '(?m)^generations = \d+', 'generations = 338'
if ($text -notmatch '(?m)^lifetime_archive_enabled\s*=') {
  $text=$text -replace '(?m)^cvt_kmeans_iters = \d+\s*$', "cvt_kmeans_iters = 32`r`nlifetime_archive_enabled = true"
} else {
  $text=$text -replace '(?m)^lifetime_archive_enabled = \w+', 'lifetime_archive_enabled = true'
}
Set-Content -Path $dst -Value $text -Encoding UTF8
Select-String -Path $dst -Pattern 'scenario_name|resume_checkpoint_path|generations =|hunt_bin_size|hunt_max_bins|cvt_enabled|lifetime_archive_enabled|line_sigma|mutation_boost' | ForEach-Object { $_.Line }
