cd D:\claude-code\c++
$base = "data/runs/long_run_80gen_stage4b_s55274_tail"
$rd = (Get-ChildItem $base -Directory | Sort-Object LastWriteTime -Descending | Select-Object -First 1).FullName
$td = "$rd/trace/generation_2/episode_0"
Write-Host "TRACE=$td"
python scripts/protagonist_trajectory_narrate.py $td --species protagonist --stride 20 --out tmp/narrate_protagonist_g2.md
python scripts/protagonist_trajectory_narrate.py $td --species pack_hunter --stride 20 --out tmp/narrate_packhunter_g2.md
Write-Host "=== species present ==="
python -c "import pandas as pd; d=pd.read_csv(r'$td/agent_trace.csv'); print(d['species_id'].value_counts().to_dict()); print('rows',len(d),'agents',d['agent_id'].nunique(),'ticks',d['step_index'].nunique())"
