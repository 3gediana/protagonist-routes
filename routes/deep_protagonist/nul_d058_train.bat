@echo off
REM D-058: Shed recipe cut to 1 wood + 1 grass (was 4+2). PPO's biggest
REM blocker through D-053..D-057 was the 6-deposit horizon - the +10
REM completion prize was too far from the "I will build" decision
REM under gamma=0.99 discount. Cutting the chain to 2 deposits makes
REM the prize visible in training. WoodHouse/StoneHouse/BigHouse keep
REM original costs so progression has a ladder.
REM Combined with D-057 reward landscape (built_shelter 10 + deposit
REM milestone 0.5 + first_house fire_once_critical).
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" 1>nul 2>&1
cd /d D:\claude-code\c++\routes\deep_protagonist\build
copy /y ..\runs\ppo_d055_2M.pt ..\runs\ppo_d058_2M.pt 1>nul 2>&1
copy /y ..\runs\ppo_d055_2M.pt.opt ..\runs\ppo_d058_2M.pt.opt 1>nul 2>&1
del /q ..\runs\ppo_d058_2M.jsonl 2>nul
bin\deep_protagonist_train.exe --policy ppo --steps 2000000 --seed 12345 --metrics ..\runs\ppo_d058_2M.jsonl --save-path ..\runs\ppo_d058_2M.pt --save-every 50 --load ..\runs\ppo_d058_2M.pt
exit /b %errorlevel%
