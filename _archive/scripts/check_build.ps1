cd D:\claude-code\c++
Write-Host "=== build dir ==="
Test-Path build_ninja_cuda/build.ninja
Test-Path build_ninja_cuda/CMakeCache.txt
Write-Host "=== ninja / cmake available? ==="
$ninja = Get-Command ninja -ErrorAction SilentlyContinue; if($ninja){ Write-Host ("ninja: " + $ninja.Source) } else { Write-Host "ninja: NOT on PATH" }
$cmake = Get-Command cmake -ErrorAction SilentlyContinue; if($cmake){ Write-Host ("cmake: " + $cmake.Source) } else { Write-Host "cmake: NOT on PATH" }
Write-Host "=== neural-eco-protagonist target in build.ninja? ==="
if(Test-Path build_ninja_cuda/build.ninja){ (Select-String -Path build_ninja_cuda/build.ninja -Pattern 'neural-eco-protagonist' | Select-Object -First 3 | ForEach-Object { $_.Line }) }
Write-Host "=== existing build helper scripts ==="
Get-ChildItem -Recurse -File -Include *.bat,*.ps1 -ErrorAction SilentlyContinue | Where-Object { $_.Name -match 'build|cmake|ninja|configure' } | Select-Object -ExpandProperty FullName | Select-Object -First 15
Write-Host "=== current binary timestamp ==="
Get-Item build_ninja_cuda/bin/neural-eco-protagonist.exe | Select-Object LastWriteTime,Length | Format-List
