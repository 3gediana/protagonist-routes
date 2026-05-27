@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" 1>nul 2>&1
cd /d D:\claude-code\c++\routes\deep_protagonist\build
del /q ..\runs\d055_smoke.jsonl 2>nul
bin\deep_protagonist_train.exe --policy ppo --steps 100000 --seed 12345 --metrics ..\runs\d055_smoke.jsonl --load ..\runs\ppo_d053_2M.pt
exit /b %errorlevel%
