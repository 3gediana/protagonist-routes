$ErrorActionPreference='Stop'
$pe='D:\claude-code\c++\routes\protagonist_ecology'
$template=Join-Path $pe 'onepot_sD_persist_ckpt_50023.toml'
$seeds = @(50022,50023,50024)
$ckptBase = @{
    50022 = 'D:/claude-code/c++/routes/protagonist_ecology/data/runs/onepot_sD_warm_50022/20260607-184222/checkpoint_gen309'
    50023 = 'D:/claude-code/c++/routes/protagonist_ecology/data/runs/onepot_sD_warm_50023/20260607-204900/checkpoint_gen337'
    50024 = 'D:/claude-code/c++/routes/protagonist_ecology/data/runs/onepot_sD_warm_50024/20260607-184227/checkpoint_gen309'
}
$startGen = @{ 50022=310; 50023=338; 50024=310 }

foreach ($seed in $seeds) {
    $dst = Join-Path $pe "onepot_sD_shortrun_$seed.toml"
    $text = Get-Content $template -Raw
    $text = $text -replace 'scenario_name = "[^"]+"', ('scenario_name = "onepot_sD_shortrun_{0}"' -f $seed)
    $text = $text -replace 'resume_checkpoint_path = "[^"]+"', ('resume_checkpoint_path = "{0}"' -f $ckptBase[$seed])
    $endGen = $startGen[$seed] + 20
    $text = $text -replace '(?m)^generations = \d+', ('generations = {0}' -f $endGen)
    $text = $text -replace '(?m)^random_seed = \d+', ('random_seed = {0}' -f $seed)
    $text = $text -replace '(?m)^checkpoint_every = \d+', 'checkpoint_every = 5'
    Set-Content -Path $dst -Value $text -Encoding UTF8
    Write-Output "=== $seed ==="
    Select-String -Path $dst -Pattern 'scenario_name|resume_checkpoint_path|generations =|random_seed|checkpoint_every|lifetime_archive_enabled' | ForEach-Object { $_.Line }
}
