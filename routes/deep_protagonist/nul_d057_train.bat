@echo off
REM D-057: D-056 was too aggressive (built_shelter 15 + house x5 = PPO
REM collapse, score 0.022, entropy 16+). Roll back to a moderate set.
REM - built_shelter 15 -> 10 (still 2x of D-055 baseline 5)
REM - first_house_built keeps fire_once_critical (+5 milestone)
REM - NEW: each deposit fires event_milestone(0.5) shaping
REM Resume from D-055 ckpt to avoid D-056's collapse state.
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" 1>nul 2>&1
cd /d D:\claude-code\c++\routes\deep_protagonist\build
copy /y ..\runs\ppo_d055_2M.pt ..\runs\ppo_d057_2M.pt 1>nul 2>&1
copy /y ..\runs\ppo_d055_2M.pt.opt ..\runs\ppo_d057_2M.pt.opt 1>nul 2>&1
del /q ..\runs\ppo_d057_2M.jsonl 2>nul
bin\deep_protagonist_train.exe --policy ppo --steps 2000000 --seed 12345 --metrics ..\runs\ppo_d057_2M.jsonl --save-path ..\runs\ppo_d057_2M.pt --save-every 50 --load ..\runs\ppo_d057_2M.pt
exit /b %errorlevel%
