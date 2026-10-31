@echo off
set PATH=D:\NVIDIA\CUDA\v12.8\bin;D:\claude-code\c++\routes\deep_protagonist\build\bin;%PATH%
cd /d D:\claude-code\c++\routes\deep_protagonist
build\bin\deep_protagonist_train.exe --policy ppo --n-envs 1 --seed 12345 --domain-rand --steps 3000000 --load runs\s4_bc_nutrition_v1.pt --nutrition --nutri-protein-decay 40 --ppo-mask-fire-only --ppo-kl-ref runs\s4_bc_nutrition_v1.pt --ppo-kl-coef 0.03 --ppo-target-kl 0.20 --ent-coef 0.01 --lr 5e-5 --critic-warmup 64 --ppo-night-shaping 1.5 --ppo-day-shaping 1.0 --save-path runs\s4ppo_nutrition_v6_kl.pt --save-every 50 --metrics runs\s4ppo_nutrition_v6_kl.jsonl > D:\claude-code\c++\routes\deep_protagonist\_devin_dp_logs\dp_v6.out 2> D:\claude-code\c++\routes\deep_protagonist\_devin_dp_logs\dp_v6.err
