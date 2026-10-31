$ErrorActionPreference = "SilentlyContinue"
cd D:\claude-code\c++
$base = (Get-ChildItem 'data/runs/long_run_80gen_stage4b_s63810_tail' -Directory | Sort-Object LastWriteTime -Descending | Select-Object -First 1).FullName
$ckpt = Join-Path $base 'checkpoint_gen79'
$ckptToml = ($ckpt -replace '\\','/')
$src = "tmp/layer1/long_run_80gen_stage4b_s63810_tail.toml"
$dst = "tmp/layer1/cumulative_smoke_resume.toml"
$c = Get-Content $src -Raw
$c = $c -replace "generations = 80", "generations = 81"          # resume@80 -> run only gen 80
$c = $c -replace 'runs_dir = "data/runs/long_run_80gen_stage4b_s63810_tail"', 'runs_dir = "data/runs/cumulative_smoke_resume"'
$c = $c -replace "checkpoint_every = 10", "checkpoint_every = 0"  # no checkpoint write for smoke
$c = $c -replace "tail_generations = 1", "tail_generations = 0"   # no trace
$c = $c -replace "(?m)^\[runtime\]", ("[runtime]`r`nresume_checkpoint_path = `"$ckptToml`"")
Set-Content -Path $dst -Value $c -NoNewline
Select-String -Path $dst -Pattern 'resume_checkpoint_path|runs_dir =|^generations =|^checkpoint_every|tail_generations' | ForEach-Object { $_.LineNumber.ToString() + ': ' + $_.Line }
$bat = "tmp/run_cumulative_smoke.bat"
$batc = "@echo off`r`ncd /d D:\claude-code\c++`r`nbuild_ninja_cuda\bin\neural-eco-protagonist.exe --config tmp\layer1\cumulative_smoke_resume.toml > tmp\runlogs\cumulative_smoke_resume.log 2>&1`r`necho done > tmp\runlogs\cumulative_smoke_resume.done`r`n"
Set-Content -Path $bat -Value $batc -NoNewline
Write-Host "SMOKE PREP DONE (not launched)."
