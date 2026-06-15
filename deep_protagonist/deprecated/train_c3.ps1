Set-Location 'D:\claude-code\c++\routes\deep_protagonist'
$exe='build\bin\deep_protagonist_train.exe'
$common = @('--policy','ppo','--steps','800000','--seed','12345','--n-envs','16',
  '--load','runs\bc_v9.pt','--ppo-mask-atk-fire',
  '--ppo-kl-ref','runs\bc_v9.pt','--ppo-kl-coef','1.0','--ppo-target-kl','0.02',
  '--ent-coef','0.0003','--lr','3e-5','--critic-warmup','64','--ppo-night-shaping','2.0')
# c3a: day-coef 1.0
& $exe @common --ppo-day-shaping 1.0 --save-path runs\s4ppo_c3a.pt --metrics runs\s4ppo_c3a.jsonl *> runs\train_c3a.log
"C3A_DONE $(Get-Date -Format o)" | Out-File runs\done_c3a.txt -Encoding ASCII
# c3b: day-coef 2.0
& $exe @common --ppo-day-shaping 2.0 --save-path runs\s4ppo_c3b.pt --metrics runs\s4ppo_c3b.jsonl *> runs\train_c3b.log
"C3B_DONE $(Get-Date -Format o)" | Out-File runs\done_c3b.txt -Encoding ASCII
"ALL_C3_DONE $(Get-Date -Format o)" | Out-File runs\done_c3_all.txt -Encoding ASCII
