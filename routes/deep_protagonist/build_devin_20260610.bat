@echo off
REM D-122 RUNG 2: isolated build dir so the running build_d122 training exe is never touched.
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
set "CUDA_PATH=D:\NVIDIA\CUDA\v12.8"
set "CUDA_PATH_V12_8=D:\NVIDIA\CUDA\v12.8"
set "PATH=D:\NVIDIA\CUDA\v12.8\bin;%PATH%"
cd /d D:\claude-code\c++\routes\deep_protagonist
echo === CONFIGURE ===
cmake -S . -B build_devin_20260610 -G Ninja ^
  -DCMAKE_BUILD_TYPE=Release ^
  -DCMAKE_MAKE_PROGRAM=G:/ai/qtdownload/Tools/Ninja/ninja.exe ^
  -DCMAKE_CUDA_ARCHITECTURES=52 ^
  -DCMAKE_CUDA_COMPILER=D:/NVIDIA/CUDA/v12.8/bin/nvcc.exe ^
  -DCUDA_TOOLKIT_ROOT_DIR=D:/NVIDIA/CUDA/v12.8 ^
  -DCUDAToolkit_ROOT=D:/NVIDIA/CUDA/v12.8 ^
  -DTorch_DIR=D:/claude-code/c++/routes/deep_protagonist/third_party/libtorch/share/cmake/Torch
if errorlevel 1 ( echo CONFIGURE_FAILED & exit /b 1 )
echo === BUILD ===
cmake --build build_devin_20260610 --target deep_protagonist_train -j 6
if errorlevel 1 ( echo BUILD_FAILED & exit /b 1 )
echo === BUILD_OK ===

