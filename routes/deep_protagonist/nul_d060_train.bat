@echo off
REM D-060: fresh PPO restart (no resume from D-055 or any other ckpt).
REM D-056..D-059 all failed because resume + reset_hidden + tiny world
REM change is enough to collapse PPO. Going fresh lets the policy learn
REM the simplified world from scratch.
REM Configuration:
REM - Shed recipe 1 wood + 1 grass (D-058 world change kept)
REM - all D-055 rewards intact (single-site rule, deposit +1, etc.)
REM - 2M steps from random init
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" 1>nul 2>&1
cd /d D:\claude-code\c++\routes\deep_protagonist\build
del /q ..\runs\ppo_d060_2M.pt ..\runs\ppo_d060_2M.pt.opt 2>nul
del /q ..\runs\ppo_d060_2M.jsonl 2>nul
bin\deep_protagonist_train.exe --policy ppo --steps 2000000 --seed 12345 --metrics ..\runs\ppo_d060_2M.jsonl --save-path ..\runs\ppo_d060_2M.pt --save-every 50
exit /b %errorlevel%
