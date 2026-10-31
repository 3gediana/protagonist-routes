@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
set "PATH=D:\NVIDIA\CUDA\v12.8\bin;%PATH%"
cd /d D:\claude-code\c++\build_ninja_cuda
cmake --build . --target neural-eco-protagonist -j 6
echo BUILD_EXIT=%errorlevel%
