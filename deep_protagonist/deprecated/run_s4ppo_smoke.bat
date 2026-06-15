@echo off
cd /d D:\claude-code\c++\routes\deep_protagonist
build\bin\deep_protagonist_train.exe --policy ppo --load runs\bc_v9.pt --n-envs 16 --steps 2500000 --seed 12360 --critic-warmup 64 --ent-coef 0.001 --ppo-night-shaping 2.0 --metrics runs\s4ppo_smoke.jsonl --save-path runs\s4ppo_smoke.pt --save-every 50 > runs\s4ppo_smoke.stdout.log 2> runs\s4ppo_smoke.stderr.log
echo SMOKE_DONE EXIT=%ERRORLEVEL% > runs\s4ppo_smoke.DONE
