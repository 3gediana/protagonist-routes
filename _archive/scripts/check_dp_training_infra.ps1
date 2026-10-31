$dp = 'D:\claude-code\c++\routes\deep_protagonist'
Write-Output '=== demos_s4 ==='
Get-ChildItem "$dp\demos_s4" -File -ErrorAction SilentlyContinue | Select-Object Name,Length | Format-Table -AutoSize
Write-Output '=== configs ==='
Get-ChildItem "$dp\configs" -File -ErrorAction SilentlyContinue | Select-Object Name | Format-Table -AutoSize
Write-Output '=== DP src/train*.py ==='
Get-ChildItem "$dp\src" -Recurse -File -Filter 'train*.py' -ErrorAction SilentlyContinue | Select-Object FullName | Format-Table -AutoSize
Write-Output '=== DP src/oracle*.py ==='
Get-ChildItem "$dp\src" -Recurse -File -Filter 'oracle*.py' -ErrorAction SilentlyContinue | Select-Object FullName | Format-Table -AutoSize
Write-Output '=== DP tools ==='
Get-ChildItem "$dp\tools" -File -ErrorAction SilentlyContinue | Select-Object Name | Format-Table -AutoSize
Write-Output '=== top-level scripts/*.py ==='
Get-ChildItem 'D:\claude-code\c++\scripts' -File -Filter '*.py' | Select-Object Name | Format-Table -AutoSize
Write-Output '=== dp_repertoire_smoke.ps1 ==='
if(Test-Path 'D:\claude-code\c++\dp_repertoire_smoke.ps1'){ Get-Content 'D:\claude-code\c++\dp_repertoire_smoke.ps1' -Head 30 }
Write-Output '=== train_repertoire.ps1 ==='
Get-ChildItem 'D:\claude-code\c++' -File -Filter 'train_repertoire*' -ErrorAction SilentlyContinue | Select-Object Name | Format-Table -AutoSize
