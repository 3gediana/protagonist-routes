$ErrorActionPreference='Stop'
$pe='D:\claude-code\c++\routes\protagonist_ecology'
$specs = @(
  @{Seed=50022; Ckpt='D:/claude-code/c++/routes/protagonist_ecology/data/runs/onepot_sD_warm_50022/20260607-184222/checkpoint_gen309'; Gens=330},
  @{Seed=50023; Ckpt='D:/claude-code/c++/routes/protagonist_ecology/data/runs/onepot_sD_warm_50023/20260607-184224/checkpoint_gen329'; Gens=350},
  @{Seed=50024; Ckpt='D:/claude-code/c++/routes/protagonist_ecology/data/runs/onepot_sD_warm_50024/20260607-184226/checkpoint_gen329'; Gens=350}
)
foreach ($sp in $specs) {
  $seed=$sp.Seed
  $src=Join-Path $pe "onepot_sD_warm_$seed.toml"
  $dst=Join-Path $pe "onepot_sD_stable_$seed.toml"
  $text=Get-Content $src -Raw
  $scenarioLine = 'scenario_name = "onepot_sD_stable_{0}"' -f $seed
  $resumeLine = 'resume_checkpoint_path = "{0}"' -f $sp.Ckpt
  $gensLine = 'generations = {0}' -f $sp.Gens
  $text=$text -replace 'scenario_name = "[^"]+"', $scenarioLine
  $text=$text -replace 'resume_checkpoint_path = "[^"]+"', $resumeLine
  $text=$text -replace '(?m)^generations = \d+', $gensLine
  $text=$text -replace '(?m)^hunt_bin_size = \d+', 'hunt_bin_size = 1'
  $text=$text -replace '(?m)^hunt_max_bins = \d+', 'hunt_max_bins = 3'
  if ($text -notmatch '(?m)^cvt_enabled\s*=') {
    $text=$text -replace '(?m)^hunt_max_bins = 3\s*$', "hunt_max_bins = 3`r`ncvt_enabled = true`r`ncvt_num_centroids = 64`r`ncvt_samples_per_centroid = 100`r`ncvt_kmeans_iters = 32"
  } else {
    $text=$text -replace '(?m)^cvt_enabled = \w+', 'cvt_enabled = true'
  }
  $text=$text -replace '(?m)^line_sigma = [0-9.]+', 'line_sigma = 0.05'
  $text=$text -replace '(?m)^mutation_boost = [0-9.]+', 'mutation_boost = 1.0'
  Set-Content -Path $dst -Value $text -Encoding UTF8
  Write-Output "WROTE $dst"
  Select-String -Path $dst -Pattern 'scenario_name|resume_checkpoint_path|generations =|hunt_bin_size|hunt_max_bins|cvt_enabled|line_sigma|mutation_boost' | ForEach-Object { Write-Output ('  ' + $_.Line) }
}
