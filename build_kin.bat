@echo off
REM PE Phase-D Kin: rebuild plain neural-eco-protagonist (NOT -batched, which PE runs may hold).
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
set "CUDA_PATH=D:\NVIDIA\CUDA\v12.8"
set "CUDA_PATH_V12_8=D:\NVIDIA\CUDA\v12.8"
set "PATH=D:\NVIDIA\CUDA\v12.8\bin;%PATH%"
cd /d D:\claude-code\c++
echo === BUILD neural-eco-protagonist ===
cmake --build build_ninja_cuda --target neural-eco-protagonist -j 6
if errorlevel 1 ( echo BUILD_FAILED & exit /b 1 )
echo === BUILD_OK ===
