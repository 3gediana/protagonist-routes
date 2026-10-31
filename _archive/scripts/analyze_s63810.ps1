cd D:\claude-code\c++
$base = "routes/protagonist_ecology/data/runs/long_run_80gen_stage4b_s63810_tail"
if (-not (Test-Path $base)) { $base = "data/runs/long_run_80gen_stage4b_s63810_tail" }
$rd = (Get-ChildItem $base -Directory | Sort-Object LastWriteTime -Descending | Select-Object -First 1).FullName
Write-Host "RUNDIR=$rd"
Write-Host "=== HARMONY ==="
python scripts/protagonist_harmony_check.py $rd
Write-Host "=== TRACE DIRS ==="
Get-ChildItem "$rd/trace" -Directory -ErrorAction SilentlyContinue | Select-Object Name
