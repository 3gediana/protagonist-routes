Set-Location 'D:\claude-code\c++\routes\deep_protagonist'
$exe='build\bin\deep_protagonist_train.exe'
$common = @('--policy','ppo','--steps','800000','--seed','12345','--n-envs','16',
  '--load','runs\bc_v9.pt','--ppo-mask-atk-fire',
  '--ppo-kl-ref','runs\bc_v9.pt','--ppo-kl-coef','1.0','--ppo-target-kl','0.02',
  '--ent-coef','0.0003','--lr','3e-5','--critic-warmup','64','--ppo-night-shaping','2.0')
& $exe @common --ppo-day-shaping 0.5 --save-path runs\s4ppo_c3c.pt --metrics runs\s4ppo_c3c.jsonl *> runs\train_c3c.log
"C3C_DONE $(Get-Date -Format o)" | Out-File runs\done_c3c.txt -Encoding ASCII
