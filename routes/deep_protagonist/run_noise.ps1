Set-Location 'D:\claude-code\c++\routes\deep_protagonist'
Remove-Item "runs\eval_v11\NOISEDONE.txt" -ErrorAction SilentlyContinue
powershell -NoProfile -ExecutionPolicy Bypass -File eval_gate.ps1 -ckpt runs\bc_v13.pt -tag v13n30 -seeds "1700000,1900000" -eps 30
powershell -NoProfile -ExecutionPolicy Bypass -File eval_gate.ps1 -ckpt runs\bc_v9.pt  -tag v9n30  -seeds "1700000,1900000" -eps 30
"NOISEDONE $(Get-Date -Format o)" | Out-File "runs\eval_v11\NOISEDONE.txt" -Encoding ASCII
