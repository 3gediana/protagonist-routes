@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d D:\claude-code\c++
cmake --build build_ninja_cuda --target neural-eco-protagonist-batched
echo BUILD_EXIT=%ERRORLEVEL%
