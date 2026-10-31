@echo off
set "CUDA_PATH=D:\NVIDIA\CUDA\v12.8"
set "PATH=D:\NVIDIA\CUDA\v12.8\bin;%PATH%"
cd /d D:\claude-code\c++\routes\deep_protagonist
echo === OFF ===
.\build_devin_20260610\bin\deep_protagonist_train.exe --policy ppo --n-envs 8 --steps 200000 --save-path checkpoints\cmp_off.pt --save-every 999999 --metrics metrics\cmp_off.jsonl --seed 7 > cmp_off.log 2>&1
echo OFF_EXIT=%ERRORLEVEL%
echo === ON ===
.\build_devin_20260610\bin\deep_protagonist_train.exe --policy ppo --n-envs 8 --steps 200000 --save-path checkpoints\cmp_on.pt --save-every 999999 --metrics metrics\cmp_on.jsonl --seed 7 --survive-first-mask > cmp_on.log 2>&1
echo ON_EXIT=%ERRORLEVEL%
