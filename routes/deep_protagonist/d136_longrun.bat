@echo off
rem D-136 one-click long run: fresh PPO, 10M steps, new 15-milestone baseline.
rem Tech-tree actions are unlocked by default since D-136 round 3.
cd /d D:\claude-code\c++\routes\deep_protagonist
.\build_devin_20260610\bin\deep_protagonist_train.exe --policy ppo --n-envs 8 ^
  --steps 10000000 ^
  --save-path checkpoints\d136_longrun.pt --save-every 50 ^
  --metrics metrics\d136_longrun.jsonl --seed 7 ^
  > d136_longrun.log 2>&1
