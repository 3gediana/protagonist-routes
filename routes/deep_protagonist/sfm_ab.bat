@echo off
set "CUDA_PATH=D:\NVIDIA\CUDA\v12.8"
set "PATH=D:\NVIDIA\CUDA\v12.8\bin;%PATH%"
cd /d D:\claude-code\c++\routes\deep_protagonist
echo === A/B CONTROL (mask OFF) start %DATE% %TIME% ===
.\build_devin_20260610\bin\deep_protagonist_train.exe --policy ppo --n-envs 8 --steps 10000000 --save-path checkpoints\sfm_ab_ctrl.pt --save-every 50 --metrics metrics\sfm_ab_ctrl.jsonl --seed 7 > sfm_ab_ctrl.log 2>&1
echo CTRL_EXIT=%ERRORLEVEL% %DATE% %TIME%
echo === A/B TREATMENT (mask ON) start %DATE% %TIME% ===
.\build_devin_20260610\bin\deep_protagonist_train.exe --policy ppo --n-envs 8 --steps 10000000 --save-path checkpoints\sfm_ab_treat.pt --save-every 50 --metrics metrics\sfm_ab_treat.jsonl --seed 7 --survive-first-mask > sfm_ab_treat.log 2>&1
echo TREAT_EXIT=%ERRORLEVEL% %DATE% %TIME%
echo === AB_DONE %DATE% %TIME% ===
