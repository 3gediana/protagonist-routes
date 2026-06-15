Set-Location "D:\claude-code\c++\routes\deep_protagonist"
$exe="build\bin\deep_protagonist_train.exe"
$common=@("--policy","ppo","--steps","800000","--seed","12345","--n-envs","16","--load","runs\bc_v9.pt","--ppo-mask-atk-fire","--ppo-kl-ref","runs\bc_v9.pt","--ppo-kl-coef","1.0","--ppo-target-kl","0.02","--ent-coef","0.0003","--lr","3e-5","--critic-warmup","64","--ppo-night-shaping","1.5")
& $exe @common --ppo-day-shaping 1.0 --save-path runs\s4ppo_c4.pt --metrics runs\s4ppo_c4.jsonl *> runs\train_c4.log
"C4_DONE" | Out-File runs\done_c4.txt -Encoding ASCII
.\eval_gate_mask.ps1 -ckpt runs\s4ppo_c4.pt -tag c4 -seeds "1000000,1700000,2100000" -eps 30 -steps 240000
"C4EVAL_DONE" | Out-File runs\eval_v11\done_c4eval.txt -Encoding ASCII
