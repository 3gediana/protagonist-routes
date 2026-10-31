@echo off
REM PE D/E action ports: build both exes (standard + batched) and run --action-selftest.
REM Builds in build_ninja_cuda; does NOT touch the deep_protagonist_train.exe DP process.
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
set "PATH=D:\NVIDIA\CUDA\v12.8\bin;%PATH%"
cd /d D:\claude-code\c++\build_ninja_cuda
echo === BUILD standard ===
cmake --build . --target neural-eco-protagonist -j 6
if errorlevel 1 ( echo BUILD_STD_FAILED & exit /b 1 )
echo === BUILD batched ===
cmake --build . --target neural-eco-protagonist-batched -j 6
if errorlevel 1 ( echo BUILD_BATCHED_FAILED & exit /b 1 )
echo === RUN action-selftest (standard) ===
bin\neural-eco-protagonist.exe --action-selftest
echo STD_EXIT=%errorlevel%
echo === RUN action-selftest (batched) ===
bin\neural-eco-protagonist-batched.exe --action-selftest
echo BATCHED_EXIT=%errorlevel%
echo === ALL_DONE ===
