@echo off
REM D-054: continue training from ppo_d053_2M.pt (which already drinks but
REM dies more from cross-region foraging). Same reward landscape as D-053,
REM just longer training so PPO settles into a stable food + drink + avoid
REM joint policy.
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" 1>nul 2>&1
cd /d D:\claude-code\c++\routes\deep_protagonist\build
copy /y ..\runs\ppo_d053_2M.pt ..\runs\ppo_d054_2M.pt 1>nul 2>&1
copy /y ..\runs\ppo_d053_2M.pt.opt ..\runs\ppo_d054_2M.pt.opt 1>nul 2>&1
del /q ..\runs\ppo_d054_2M.jsonl 2>nul
bin\deep_protagonist_train.exe --policy ppo --steps 2000000 --seed 12345 --metrics ..\runs\ppo_d054_2M.jsonl --save-path ..\runs\ppo_d054_2M.pt --save-every 50 --load ..\runs\ppo_d054_2M.pt
exit /b %errorlevel%
