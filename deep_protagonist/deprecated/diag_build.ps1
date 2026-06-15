$ErrorActionPreference = 'SilentlyContinue'
Set-Location 'D:\claude-code\c++\routes\deep_protagonist'
Write-Output '--- build scripts in route dir ---'
Get-ChildItem -File | Where-Object { $_.Extension -in '.ps1','.bat','.cmd' } | Select-Object -ExpandProperty Name
Write-Output '--- build scripts in c++ root ---'
Get-ChildItem 'D:\claude-code\c++' -File | Where-Object { $_.Extension -in '.ps1','.bat','.cmd' } | Select-Object -ExpandProperty Name
Write-Output '--- CMakeCache generator/compiler ---'
Select-String -Path 'build\CMakeCache.txt' -Pattern 'CMAKE_GENERATOR:|CMAKE_CXX_COMPILER:|CMAKE_MAKE_PROGRAM:' | ForEach-Object { $_.Line }
Write-Output '--- INCLUDE env currently ---'
Write-Output $env:INCLUDE
Write-Output '--- vcvars locations ---'
Get-ChildItem 'C:\Program Files (x86)\Microsoft Visual Studio','C:\Program Files\Microsoft Visual Studio' -Recurse -Filter 'vcvars64.bat' -ErrorAction SilentlyContinue | Select-Object -ExpandProperty FullName
