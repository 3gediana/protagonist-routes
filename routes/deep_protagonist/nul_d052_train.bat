@echo off
REM D-052: alive_step kept at 0.05 (revert D-051), only first_eat/drink critical bonus +5 stays.
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" 1>nul 2>&1
cd /d D:\claude-code\c++\routes\deep_protagonist\build
del /q ..\runs\ppo_d052_2M.pt 2>nul
del /q ..\runs\ppo_d052_2M.pt.opt 2>nul
del /q ..\runs\ppo_d052_2M.jsonl 2>nul
bin\deep_protagonist_train.exe --policy ppo --steps 2000000 --seed 12345 --metrics ..\runs\ppo_d052_2M.jsonl --save-path ..\runs\ppo_d052_2M.pt --save-every 50
exit /b %errorlevel%
