$r = 'D:/claude-code/c++/routes/deep_protagonist'
Write-Output '=== launch_build.ps1 ==='
if (Test-Path "$r/launch_build.ps1") { Get-Content "$r/launch_build.ps1" }
Write-Output '=== build dirs under route ==='
Get-ChildItem $r -Directory | Where-Object { $_.Name -like '*build*' } | Select-Object -ExpandProperty FullName
Write-Output '=== any CMakeCache under route ==='
Get-ChildItem $r -Recurse -Filter 'CMakeCache.txt' -ErrorAction SilentlyContinue -Depth 2 | Select-Object -ExpandProperty FullName
Write-Output '=== any .exe under route (top 2 levels) ==='
Get-ChildItem $r -Recurse -Filter '*.exe' -ErrorAction SilentlyContinue -Depth 2 | Select-Object FullName, LastWriteTime, Length | Format-Table -Auto
