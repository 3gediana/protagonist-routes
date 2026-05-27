@echo off
REM D-061: continue 2M from D-060 ckpt with the same config. D-060 ended
REM at score 0.125 still climbing - 2M was not enough to saturate. With
REM another 2M PPO should consolidate food+drink and start hitting
REM shelter / spear / house milestones.
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" 1>nul 2>&1
cd /d D:\claude-code\c++\routes\deep_protagonist\build
copy /y ..\runs\ppo_d060_2M.pt ..\runs\ppo_d061_2M.pt 1>nul 2>&1
copy /y ..\runs\ppo_d060_2M.pt.opt ..\runs\ppo_d061_2M.pt.opt 1>nul 2>&1
del /q ..\runs\ppo_d061_2M.jsonl 2>nul
bin\deep_protagonist_train.exe --policy ppo --steps 2000000 --seed 12345 --metrics ..\runs\ppo_d061_2M.jsonl --save-path ..\runs\ppo_d061_2M.pt --save-every 50 --load ..\runs\ppo_d061_2M.pt
exit /b %errorlevel%
