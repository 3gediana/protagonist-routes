Set-Location 'D:\claude-code\c++\routes\deep_protagonist'
Remove-Item "runs\eval_v11\ALLEVAL.txt" -ErrorAction SilentlyContinue
powershell -NoProfile -ExecutionPolicy Bypass -File eval_gate.ps1 -ckpt runs\bc_v13.pt -tag v13g
powershell -NoProfile -ExecutionPolicy Bypass -File eval_gate.ps1 -ckpt runs\bc_v9.pt  -tag v9g
"ALLEVAL $(Get-Date -Format o)" | Out-File "runs\eval_v11\ALLEVAL.txt" -Encoding ASCII
