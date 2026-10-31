$ErrorActionPreference='Stop'
$pe='D:\claude-code\c++\routes\protagonist_ecology'
$src=Join-Path $pe 'onepot_sD_persist_50023.toml'
$dst=Join-Path $pe 'onepot_sD_persist_ckpt_50023.toml'
$text=Get-Content $src -Raw
$text=$text -replace 'scenario_name = "[^"]+"', 'scenario_name = "onepot_sD_persist_ckpt_50023"'
$text=$text -replace '(?m)^checkpoint_every = \d+', 'checkpoint_every = 1'
Set-Content -Path $dst -Value $text -Encoding UTF8
Select-String -Path $dst -Pattern 'scenario_name|resume_checkpoint_path|generations =|checkpoint_every|lifetime_archive_enabled|line_sigma|mutation_boost' | ForEach-Object { $_.Line }
