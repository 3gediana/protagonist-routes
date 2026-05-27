@echo off
REM D-059: revert all reward changes (built_shelter back to 5, no
REM deposit milestone, no first_house x5). Keep only the world change:
REM Shed recipe is now 1 wood + 1 grass = 2 deposits.
REM Hypothesis: D-055 ckpt's policy is fine, what was missing was a
REM way to actually finish a building before PPO loses interest. With
REM Shed at 2 deposits a single Mid-stage life can complete one.
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" 1>nul 2>&1
cd /d D:\claude-code\c++\routes\deep_protagonist\build
copy /y ..\runs\ppo_d055_2M.pt ..\runs\ppo_d059_2M.pt 1>nul 2>&1
copy /y ..\runs\ppo_d055_2M.pt.opt ..\runs\ppo_d059_2M.pt.opt 1>nul 2>&1
del /q ..\runs\ppo_d059_2M.jsonl 2>nul
bin\deep_protagonist_train.exe --policy ppo --steps 2000000 --seed 12345 --metrics ..\runs\ppo_d059_2M.jsonl --save-path ..\runs\ppo_d059_2M.pt --save-every 50 --load ..\runs\ppo_d059_2M.pt
exit /b %errorlevel%
