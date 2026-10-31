cd D:\claude-code\c++
$base = "data/runs/long_run_80gen_stage4b_s55274_tail"
$rd = (Get-ChildItem $base -Directory | Sort-Object LastWriteTime -Descending | Select-Object -First 1).FullName
$td = "$rd/trace/generation_2/episode_0"
python scripts/protagonist_trajectory_narrate.py $td --agent 4 --stride 10 --out tmp/narrate_agent4_g2.md
Get-Content tmp/narrate_agent4_g2.md
