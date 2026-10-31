Write-Output '=== prompt path ==='
Get-Item 'C:\Users\sans\Desktop\agent\prompt' -ErrorAction SilentlyContinue | Format-List FullName,PSIsContainer,Length,LastWriteTime
Get-ChildItem 'C:\Users\sans\Desktop\agent\prompt' -Force -ErrorAction SilentlyContinue | Select-Object Name,Length,LastWriteTime | Format-Table -AutoSize
Write-Output '=== project docs ==='
Get-ChildItem 'D:\claude-code\c++\docs' -Force -ErrorAction SilentlyContinue | Select-Object Name,Length,LastWriteTime | Format-Table -AutoSize
Get-ChildItem 'D:\claude-code\c++\routes\protagonist_ecology\docs' -Force -ErrorAction SilentlyContinue | Select-Object Name,Length,LastWriteTime | Format-Table -AutoSize
Get-ChildItem 'D:\claude-code\c++\routes\deep_protagonist\docs' -Force -ErrorAction SilentlyContinue | Select-Object Name,Length,LastWriteTime | Format-Table -AutoSize
Write-Output '=== git status root ==='
cd 'D:\claude-code\c++'; git status --short
