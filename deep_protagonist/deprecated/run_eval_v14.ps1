Set-Location 'D:\claude-code\c++\routes\deep_protagonist'
Remove-Item "runs\eval_v11\ALLEVAL14.txt" -ErrorAction SilentlyContinue
# 9-seed x 10ep baseline, same rebuilt exe, apples-to-apples
powershell -NoProfile -ExecutionPolicy Bypass -File eval_gate.ps1 -ckpt runs\bc_v14.pt -tag v14g
powershell -NoProfile -ExecutionPolicy Bypass -File eval_gate.ps1 -ckpt runs\bc_v9.pt  -tag v9g
# 30ep noise recheck on the two new-bank soft-retreat seeds (food-death focus)
powershell -NoProfile -ExecutionPolicy Bypass -File eval_gate.ps1 -ckpt runs\bc_v14.pt -tag v14n -seeds "1700000,1900000" -eps 30
powershell -NoProfile -ExecutionPolicy Bypass -File eval_gate.ps1 -ckpt runs\bc_v9.pt  -tag v9n  -seeds "1700000,1900000" -eps 30
"ALLEVAL14 $(Get-Date -Format o)" | Out-File "runs\eval_v11\ALLEVAL14.txt" -Encoding ASCII
