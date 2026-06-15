Set-Location 'D:\claude-code\c++\routes\deep_protagonist'
.\eval_gate_mask.ps1 -ckpt runs\s4ppo_c3a.pt -tag c3a -eps 10 -steps 120000
.\eval_gate_mask.ps1 -ckpt runs\s4ppo_c3b.pt -tag c3b -eps 10 -steps 120000
"SCREEN_DONE $(Get-Date -Format o)" | Out-File runs\eval_v11\done_c3screen.txt -Encoding ASCII
