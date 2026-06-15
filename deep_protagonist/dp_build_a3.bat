@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
set "CUDA_PATH=D:\NVIDIA\CUDA\v12.8"
set "CUDA_PATH_V12_8=D:\NVIDIA\CUDA\v12.8"
set "PATH=D:\NVIDIA\CUDA\v12.8\bin;%PATH%"
cd /d D:\claude-code\c++\routes\deep_protagonist
echo === BUILD START %TIME% ===
cmake --build build_d122_v2 --target deep_protagonist_train -j 16
echo BUILD_EXIT=%ERRORLEVEL% %TIME%
