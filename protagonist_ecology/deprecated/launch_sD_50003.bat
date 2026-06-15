@echo off
set PATH=D:\NVIDIA\CUDA\v12.8\bin;%PATH%
cd /d D:\claude-code\c++\routes\protagonist_ecology
D:\claude-code\c++\build_ninja_cuda\bin\neural-eco-protagonist-batched.exe --config onepot_sD_warm_50003.toml > D:\claude-code\c++\routes\protagonist_ecology\sD_50003.log 2>&1
