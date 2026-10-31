cd D:\claude-code\c++
$mk = (Select-String -Path build_ninja_cuda/CMakeCache.txt -Pattern 'CMAKE_MAKE_PROGRAM:').Line
Write-Host $mk
Write-Host "=== build (no-op, target neural-eco-protagonist) ==="
$sw = [System.Diagnostics.Stopwatch]::StartNew()
& "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" --build build_ninja_cuda --target neural-eco-protagonist 2>&1 | Select-Object -Last 20
$sw.Stop()
Write-Host ("EXIT=" + $LASTEXITCODE + " elapsed=" + [int]$sw.Elapsed.TotalSeconds + "s")
