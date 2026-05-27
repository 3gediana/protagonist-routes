@echo off
REM D-055: from D-053 ckpt with new building reward shaping
REM - place_blueprint refused while another site is incomplete (BuildingSystem)
REM - place_blueprint failure pays 0.05 cost on top of trigger_cost
REM - deposit gets its own +1.0 reward path (vs collect +0.5)
REM - jsonl logs progression: stage, collect_total, explored_m, houses_built, sites_active
REM Goal: PPO finally completes a building (D-053 placed 981 sites in 33 ep,
REM finished 0; now site quota is 1 at a time so deposit can pay off).
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" 1>nul 2>&1
cd /d D:\claude-code\c++\routes\deep_protagonist\build
copy /y ..\runs\ppo_d053_2M.pt ..\runs\ppo_d055_2M.pt 1>nul 2>&1
copy /y ..\runs\ppo_d053_2M.pt.opt ..\runs\ppo_d055_2M.pt.opt 1>nul 2>&1
del /q ..\runs\ppo_d055_2M.jsonl 2>nul
bin\deep_protagonist_train.exe --policy ppo --steps 2000000 --seed 12345 --metrics ..\runs\ppo_d055_2M.jsonl --save-path ..\runs\ppo_d055_2M.pt --save-every 50 --load ..\runs\ppo_d055_2M.pt
exit /b %errorlevel%
