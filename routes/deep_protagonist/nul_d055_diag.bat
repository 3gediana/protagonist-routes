@echo off
REM D-055: pure eval of d053 ckpt to see real progression state.
REM Adds stage/collect_total/explored_m/houses_built/sites_active fields
REM so we can finally answer "why does PPO never enter Mid stage?"
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" 1>nul 2>&1
cd /d D:\claude-code\c++\routes\deep_protagonist\build
del /q ..\runs\d055_eval_d053.jsonl 2>nul
bin\deep_protagonist_train.exe --policy ppo --steps 5000000 --episodes 100 --seed 99999 --metrics ..\runs\d055_eval_d053.jsonl --load ..\runs\ppo_d053_2M.pt --no-update
exit /b %errorlevel%
