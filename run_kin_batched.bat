@echo off
set "PATH=D:\NVIDIA\CUDA\v12.8\bin;%PATH%"
cd /d D:\claude-code\c++\build_ninja_cuda\bin
neural-eco-protagonist-batched.exe --kin-selftest
echo REAL_EXIT=%errorlevel%
