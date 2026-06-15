Set-Location 'D:\claude-code\c++\routes\deep_protagonist'
# Repeat-eval both candidate (masked) and champion (unmasked) on current exe,
# 30 episodes x 9 seeds -> per-seed food-death/nShel DISTRIBUTIONS (apples-to-apples).
.\eval_gate_mask.ps1 -ckpt runs\s4ppo_c1.pt -tag c1r -eps 30 -steps 240000
.\eval_gate.ps1      -ckpt runs\bc_v9.pt    -tag v9r -eps 30 -steps 240000
"ALL_DONE $(Get-Date -Format o)" | Out-File "runs\eval_v11\done_rep_all.txt" -Encoding ASCII
