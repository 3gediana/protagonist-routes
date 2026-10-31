$dp = 'D:\claude-code\c++\routes\deep_protagonist'
Write-Output '=== Python files in DP ==='
Get-ChildItem $dp -File -Filter '*.py' -ErrorAction SilentlyContinue | Select-Object Name | Format-Table -AutoSize
Write-Output '=== scripts dir ==='
Get-ChildItem "$dp\scripts" -File -ErrorAction SilentlyContinue | Select-Object Name | Format-Table -AutoSize
Write-Output '=== DP subfolders ==='
Get-ChildItem $dp -Directory | Select-Object Name | Format-Table -AutoSize
Write-Output '=== top-level python training ==='
Get-ChildItem 'D:\claude-code\c++\scripts' -File -Filter '*train*' -ErrorAction SilentlyContinue | Select-Object Name | Format-Table -AutoSize
Get-ChildItem 'D:\claude-code\c++\scripts' -File -Filter '*oracle*' -ErrorAction SilentlyContinue | Select-Object Name | Format-Table -AutoSize
Get-ChildItem 'D:\claude-code\c++\scripts' -File -Filter '*bc*' -ErrorAction SilentlyContinue | Select-Object Name | Format-Table -AutoSize
Write-Output '=== ft_gen eval status ==='
Get-ChildItem "$dp\runs" -File -Filter 'ft_gen*' | Select-Object Name,Length,LastWriteTime | Format-Table -AutoSize
