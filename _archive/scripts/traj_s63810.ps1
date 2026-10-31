cd D:\claude-code\c++
$base = "routes/protagonist_ecology/data/runs/long_run_80gen_stage4b_s63810_tail"
if (-not (Test-Path $base)) { $base = "data/runs/long_run_80gen_stage4b_s63810_tail" }
$rd = (Get-ChildItem $base -Directory | Sort-Object LastWriteTime -Descending | Select-Object -First 1).FullName
$tracedir = "$rd/trace/generation_79/episode_0"
Write-Host "TRACEDIR=$tracedir"
Get-ChildItem $tracedir | Select-Object Name,Length
Write-Host "=== ANALYZE gen79 ==="
python scripts/protagonist_trajectory_analyze.py $tracedir --out tmp/traj_s63810_gen79.json --brained-only
Write-Host "=== DELETE TRACE ==="
Remove-Item "$rd/trace" -Recurse -Force
Write-Host "trace deleted"
Get-PSDrive D | Select-Object Used,Free
