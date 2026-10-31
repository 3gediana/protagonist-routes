cd D:\claude-code\c++
$log = 'tmp/runlogs/long_run_80gen_stage4b_s55274_tail.log'
Write-Host "=== last 12 log lines ==="
Get-Content $log -Tail 12
Write-Host ""
Write-Host "=== harmony / verdict lines ==="
Select-String -Path $log -Pattern 'harmony|Harmony|ALL PASS|PASS|FAIL|invariant|sim_time' | Select-Object -Last 10 | ForEach-Object { $_.Line }
Write-Host ""
Write-Host "=== gen79 trace dirs ==="
$ts = (Get-ChildItem 'data/runs/long_run_80gen_stage4b_s55274_tail' -Directory | Sort-Object LastWriteTime -Descending | Select-Object -First 1).FullName
Write-Host "RUNTS=$ts"
$tracedir = Join-Path $ts 'trace'
Get-ChildItem $tracedir -Directory -ErrorAction SilentlyContinue | ForEach-Object { Write-Host $_.Name }
Write-Host ""
Write-Host "=== agent_trace.csv under each generation/episode ==="
Get-ChildItem $tracedir -Recurse -Filter 'agent_trace.csv' -ErrorAction SilentlyContinue | ForEach-Object { Write-Host ("{0}  size={1}KB" -f $_.FullName, [int]($_.Length/1KB)) }
