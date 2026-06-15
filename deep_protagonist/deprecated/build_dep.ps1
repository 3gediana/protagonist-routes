$ErrorActionPreference = 'Stop'
$r = 'D:/claude-code/c++/routes/deep_protagonist'
$cache = "$r/build/CMakeCache.txt"
Write-Output '=== generator / toolchain ==='
Select-String -Path $cache -Pattern '^CMAKE_GENERATOR:|^CMAKE_MAKE_PROGRAM:|^CMAKE_CXX_COMPILER:|^CMAKE_BUILD_TYPE:' | ForEach-Object { $_.Line }
