@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" 1>nul 2>&1
cd /d D:\claude-code\c++\routes\deep_protagonist\build
del /q ..\runs\d055_smoke_d054.jsonl 2>nul
bin\deep_protagonist_train.exe --policy ppo --steps 30000 --seed 12345 --metrics ..\runs\d055_smoke_d054.jsonl --load ..\runs\ppo_d054_2M.pt --no-update
exit /b %errorlevel%
