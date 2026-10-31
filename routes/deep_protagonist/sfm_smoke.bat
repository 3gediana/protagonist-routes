@echo off
set "CUDA_PATH=D:\NVIDIA\CUDA\v12.8"
set "PATH=D:\NVIDIA\CUDA\v12.8\bin;%PATH%"
cd /d D:\claude-code\c++\routes\deep_protagonist
.\build_devin_20260610\bin\deep_protagonist_train.exe --policy ppo --n-envs 8 --steps 40000 --save-path checkpoints\sfm_smoke.pt --save-every 999999 --metrics metrics\sfm_smoke.jsonl --seed 7 --survive-first-mask > sfm_smoke.log 2>&1
echo SMOKE_EXIT=%ERRORLEVEL%
