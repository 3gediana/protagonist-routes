@echo off
REM args: %1=tag  %2=ablate-group(or NONE)  %3=episodes
set "CUDA_PATH=D:\NVIDIA\CUDA\v12.8"
set "PATH=D:\NVIDIA\CUDA\v12.8\bin;%PATH%"
cd /d D:\claude-code\c++\routes\deep_protagonist
if /I not "%~2"=="NONE" set "DP_OBS_ABLATE=%~2"
echo === EVAL tag=%~1 ablate=%DP_OBS_ABLATE% episodes=%~3 %TIME% ===
build_d122_v2\bin\deep_protagonist_train.exe --policy ppo --no-update ^
  --load checkpoints\ft_hyp_entropy_bg.pt ^
  --steps 600000 --episodes %~3 ^
  --metrics runs\eval_a3_%~1.jsonl
echo EVAL_EXIT=%ERRORLEVEL% %TIME%
