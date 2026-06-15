@echo off
cd /d D:\claude-code\c++\routes\deep_protagonist
build_d122\bin\deep_protagonist_train.exe --policy ppo --load runs\w1_v7.pt --weather --allow-cook --no-update --n-envs 8 --steps 200000 --episodes 6 --metrics runs\_d122_smoke_rung1.jsonl --seed 24 > runs\_d122_smoke_rung1.stdout 2> runs\_d122_smoke_rung1.stderr
echo EXITCODE=%errorlevel%
