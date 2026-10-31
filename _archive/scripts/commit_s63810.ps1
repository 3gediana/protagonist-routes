cd D:\claude-code\c++
git add routes/protagonist_ecology/docx/PROGRESS.md routes/protagonist_ecology/docx/STATUS.md tmp/traj_3seed_verdict.md tmp/traj_s63810_gen79.json
git commit -F tmp/commit_msg_s63810.txt
Write-Host "=== HEAD ==="
git log --oneline -3
Write-Host "=== STATUS ==="
git status --short
