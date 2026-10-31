$ErrorActionPreference='SilentlyContinue'
$root='D:\claude-code\c++\routes'
Set-Location $root
Write-Output '=== harmony files ==='
Get-ChildItem -Recurse -File -Include '*harmony*' | Select-Object -First 50 FullName
Write-Output '=== status/progress files ==='
Get-ChildItem -Recurse -File -Include 'PROGRESS.md','STATUS.md','*STATUS*.md','*PROGRESS*.md' | Select-Object -First 80 FullName
Write-Output '=== grep harmony ==='
Select-String -Path (Get-ChildItem -Recurse -File -Include '*.md','*.ps1','*.py' | Select-Object -First 2000).FullName -Pattern 'harmony|7/7' | Select-Object -First 80 Path,LineNumber,Line
