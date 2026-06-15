Set-Location 'D:\claude-code\c++\routes\deep_protagonist'
.\eval_gate_mask.ps1 -ckpt runs\s4ppo_c1.pt -tag s4c1
.\eval_gate_mask.ps1 -ckpt runs\s4ppo_c2.pt -tag s4c2
"ALL_DONE $(Get-Date -Format o)" | Out-File "runs\eval_v11\done_s4c_all.txt" -Encoding ASCII
