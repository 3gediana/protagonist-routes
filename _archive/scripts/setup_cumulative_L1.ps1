$ErrorActionPreference = "SilentlyContinue"
cd D:\claude-code\c++

# Resolve s63810 gen79 checkpoint (the lineage base for cumulative Link-1)
$base = (Get-ChildItem 'data/runs/long_run_80gen_stage4b_s63810_tail' -Directory | Sort-Object LastWriteTime -Descending | Select-Object -First 1).FullName
$ckpt = Join-Path $base 'checkpoint_gen79'
if(-not (Test-Path (Join-Path $ckpt 'checkpoint_state.toml'))){ Write-Host "FATAL: checkpoint_state.toml missing at $ckpt"; exit 1 }
$ckptToml = ($ckpt -replace '\\','/')
Write-Host "RESUME_FROM=$ckptToml"

$src = "tmp/layer1/long_run_80gen_stage4b_s63810_tail.toml"
$dst = "tmp/layer1/cumulative_L1_from_s63810_g160.toml"
$c = Get-Content $src -Raw
# continue same lineage to gen 159 (80 more gens)
$c = $c -replace "generations = 80", "generations = 160"
$c = $c -replace 'runs_dir = "data/runs/long_run_80gen_stage4b_s63810_tail"', 'runs_dir = "data/runs/cumulative_L1_from_s63810_g160"'
# inject warm-start resume path under [runtime]
$c = $c -replace "(?m)^\[runtime\]", ("[runtime]`r`nresume_checkpoint_path = `"$ckptToml`"")
Set-Content -Path $dst -Value $c -NoNewline

Write-Host "=== toml verify ==="
Select-String -Path $dst -Pattern 'resume_checkpoint_path|random_seed =|runs_dir =|^max_generations|tail_generations|^generations =|^checkpoint_every' | ForEach-Object { $_.LineNumber.ToString() + ': ' + $_.Line }

$bat = "tmp/run_cumulative_L1.bat"
$batc = "@echo off`r`ncd /d D:\claude-code\c++`r`nbuild_ninja_cuda\bin\neural-eco-protagonist.exe --config tmp\layer1\cumulative_L1_from_s63810_g160.toml > tmp\runlogs\cumulative_L1_from_s63810_g160.log 2>&1`r`necho done > tmp\runlogs\cumulative_L1_from_s63810_g160.done`r`n"
Set-Content -Path $bat -Value $batc -NoNewline
Write-Host "PREP DONE (not launched). bat=$bat"
