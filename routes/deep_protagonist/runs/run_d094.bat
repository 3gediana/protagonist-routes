@echo off
cd /D D:\claude-code\c++\routes\deep_protagonist
start "" /MIN cmd /c "build\bin\deep_protagonist_train.exe --policy ppo --steps 5000000 --seed 12359 --metrics runs\ppo_d094_5M.jsonl --save-path runs\ppo_d094_5M.pt --save-every 100 > runs\ppo_d094_5M.stdout.log 2> runs\ppo_d094_5M.stderr.log"
