cd D:\claude-code\c++
git add routes/protagonist_ecology/docx/traj/realism_envperc_L1_daynight.md routes/protagonist_ecology/docx/traj/env_resp.py
git status --short -- routes/protagonist_ecology/docx/traj
$msg = "protagonist_ecology realism #1 ablation: env-perception ON vs OFF = no behavioral diff (honest null); day-night rhythm is world-driven. Phase B (env pressure) is the lever."
git commit -m $msg
git log --oneline -1
