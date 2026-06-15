$ErrorActionPreference = 'Continue'
$vc = 'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat'
$bd = 'D:\claude-code\c++\routes\deep_protagonist\build'
Write-Output '=== building (ninja via vcvars64) ==='
cmd /c "`"$vc`" >nul 2>&1 && cmake --build `"$bd`" --target deep_protagonist deep_protagonist_train 2>&1"
Write-Output ("=== cmake exit = " + $LASTEXITCODE + " ===")
Get-ChildItem "$bd\bin\deep_protagonist.exe","$bd\bin\deep_protagonist_train.exe" | Select-Object Name, LastWriteTime, Length | Format-Table -Auto
