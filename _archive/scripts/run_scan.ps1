cd D:\claude-code\c++
$base = "data/runs/long_run_80gen_stage4b_s55274_tail"
$rd = (Get-ChildItem $base -Directory | Sort-Object LastWriteTime -Descending | Select-Object -First 1).FullName
$td = "$rd/trace/generation_2/episode_0"
Write-Host "TRACE=$td"
python scan_agents.py "$td/agent_trace.csv"
