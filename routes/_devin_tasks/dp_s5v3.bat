@echo off
set PATH=D:\NVIDIA\CUDA\v12.8\bin;D:\claude-code\c++\routes\deep_protagonist\build\bin;%PATH%
cd /d D:\claude-code\c++\routes\deep_protagonist
build\bin\deep_protagonist_train.exe --policy ppo --n-envs 4 --seed 12345 --domain-rand --steps 16000000 --nutrition --nutri-protein-decay 40 --ppo-mask-fire-only --ent-coef 0.02 --lr 2e-4 --critic-warmup 64 --ppo-night-shaping 1.5 --ppo-day-shaping 1.0 --load runs\s5_v2.pt --save-path runs\s5_v3.pt --save-every 50 --metrics runs\s5_v3.jsonl > D:\claude-code\c++\routes\deep_protagonist\_devin_dp_logs\dp_s5v3.out 2> D:\claude-code\c++\routes\deep_protagonist\_devin_dp_logs\dp_s5v3.err
