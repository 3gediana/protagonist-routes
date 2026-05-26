@echo off
REM D-050 long run: FRESH PPO (no --load).
REM Reason: d049 ckpt baked drink_action_prob -> 0 because old spawn never
REM produced r_water > 0. Resuming meant agent never tried drink again,
REM r_water stayed 0, defeating D-050. Fresh re-explores with the full
REM blueprint structural stack (D-043..D-050) already in place, so the
REM new run should converge faster than the original d048 fresh run.
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" 1>nul 2>&1
cd /d D:\claude-code\c++\routes\deep_protagonist\build
del /q ..\runs\ppo_d050_2M.pt 2>nul
del /q ..\runs\ppo_d050_2M.pt.opt 2>nul
del /q ..\runs\ppo_d050_2M.jsonl 2>nul
bin\deep_protagonist_train.exe --policy ppo --steps 2000000 --seed 12345 --metrics ..\runs\ppo_d050_2M.jsonl --save-path ..\runs\ppo_d050_2M.pt --save-every 50
exit /b %errorlevel%
