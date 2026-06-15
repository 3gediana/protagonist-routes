@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
dumpbin /dependents "D:\claude-code\c++\build_ninja_cuda\bin\neural-eco-protagonist-batched.exe"
