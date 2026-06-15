@echo off
set "PATH=D:\claude-code\c++\routes\deep_protagonist\third_party\libtorch\lib;D:\NVIDIA\CUDA\v12.8\bin;%PATH%"
cd /d D:\claude-code\c++\routes\deep_protagonist
build_d122_v2\bin\deep_protagonist_train.exe --mine-selftest
echo MINE_SELFTEST_EXITCODE=%errorlevel%
