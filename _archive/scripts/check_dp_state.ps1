$dp = 'D:\claude-code\c++\routes\deep_protagonist'
Write-Output '=== DP checkpoints ==='
Get-ChildItem "$dp\runs" -File -Filter '*.pt' | Select-Object Name,Length,LastWriteTime | Format-Table -AutoSize
Write-Output '=== oracle scripts ==='
Get-ChildItem $dp -File -Filter '*oracle*' -ErrorAction SilentlyContinue | Select-Object Name | Format-Table -AutoSize
Write-Output '=== BC scripts ==='
Get-ChildItem $dp -File -Filter '*bc*' -ErrorAction SilentlyContinue | Select-Object Name | Format-Table -AutoSize
Write-Output '=== train scripts ==='
Get-ChildItem $dp -File -Filter '*train*' -ErrorAction SilentlyContinue | Select-Object Name | Format-Table -AutoSize
Write-Output '=== existing oracle demos ==='
Get-ChildItem "$dp\data" -Recurse -Filter '*oracle*' -ErrorAction SilentlyContinue | Select-Object FullName,Length | Format-Table -AutoSize
Write-Output '=== GPU remaining ==='
nvidia-smi --query-gpu=memory.used,memory.total,utilization.gpu --format=csv,noheader,nounits
