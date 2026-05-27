@echo off
REM D-056: from D-055 ckpt with stronger building completion reward
REM - built_shelter 5 -> 15 (RewardSystem.hpp)
REM - first_house_built fire_once_critical (+5 milestone x crisis)
REM Goal: PPO completes its first ever building. With +15 + 5x milestone
REM the prize is on par with first_eat/first_drink combined and big
REM enough to survive the long horizon discount.
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" 1>nul 2>&1
cd /d D:\claude-code\c++\routes\deep_protagonist\build
copy /y ..\runs\ppo_d055_2M.pt ..\runs\ppo_d056_2M.pt 1>nul 2>&1
copy /y ..\runs\ppo_d055_2M.pt.opt ..\runs\ppo_d056_2M.pt.opt 1>nul 2>&1
del /q ..\runs\ppo_d056_2M.jsonl 2>nul
bin\deep_protagonist_train.exe --policy ppo --steps 2000000 --seed 12345 --metrics ..\runs\ppo_d056_2M.jsonl --save-path ..\runs\ppo_d056_2M.pt --save-every 50 --load ..\runs\ppo_d056_2M.pt
exit /b %errorlevel%
