$dp='D:\claude-code\c++\routes\deep_protagonist'; $exe="$dp\build_d122_v2\bin\deep_protagonist_train.exe"; Set-Location $dp
$tag='hyp_aggressive_n1'; $out="$dp\runs\ft_$tag.stdout.log"; $err="$dp\runs\ft_$tag.stderr.log"; Remove-Item $out,$err -ErrorAction SilentlyContinue
$arg='--policy ppo --load runs\ft_gen.pt --weather --allow-cook --ent-coef 0.03 --critic-warmup 32 --lr 1.5e-4 --ppo-target-kl 0.08 --ppo-hunt-shaping 0.6 --ppo-repertoire-shaping 2.5 --ppo-warmth-shaping 1.5 --n-envs 1 --steps 2000000 --save-path runs\ft_hyp_aggressive_n1.pt --save-every 25 --metrics runs\ft_hyp_aggressive_n1.jsonl --seed 33'
$p=Start-Process $exe -ArgumentList $arg -RedirectStandardOutput $out -RedirectStandardError $err -PassThru -NoNewWindow
"$((Get-Date).ToString('s')) launch lowmem tag=$tag pid=$($p.Id)" | Tee-Object -FilePath "$dp\runs\dp_lowmem_log.txt" -Append
Start-Sleep 15
nvidia-smi --query-gpu=memory.used,memory.total,utilization.gpu --format=csv,noheader,nounits
Get-CimInstance Win32_Process | Where-Object { $_.Name -match 'deep_protagonist' } | ForEach-Object { "$($_.ProcessId) $($_.Name) $($_.CommandLine)" }
