@echo off
cd /d D:\claude-code\c++\routes\deep_protagonist
build\bin\deep_protagonist_train.exe --policy ppo --load runs\bc_g.pt --steps 5000000 --seed 12350 --critic-warmup 64 --ent-coef 0.001 --metrics runs\ppo_d101_5M.jsonl --save-path runs\ppo_d101_5M.pt --save-every 100 > runs\ppo_d101_5M.stdout.log 2> runs\ppo_d101_5M.stderr.log
