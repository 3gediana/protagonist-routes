$ErrorActionPreference='Continue'
$dp='D:\claude-code\c++\routes\deep_protagonist'
$exe="$dp\build_d122_v2\bin\deep_protagonist_train.exe"
$variants=@(
  @{tag='hyp_entropy_bg'; rep='1.5'; hunt='0.3'; warm='1.0'; ent='0.05'; lr='2.0e-4'; seed='41'},
  @{tag='hyp_huntfocus_bg'; rep='1.0'; hunt='0.8'; warm='0.8'; ent='0.02'; lr='1.5e-4'; seed='42'},
  @{tag='hyp_aggressive_bg'; rep='2.5'; hunt='0.6'; warm='1.5'; ent='0.03'; lr='1.5e-4'; seed='43'}
)
foreach($v in $variants){
  $out="$dp\runs\ft_$($v.tag).stdout.log"; $err="$dp\runs\ft_$($v.tag).stderr.log"
  Remove-Item $out,$err -ErrorAction SilentlyContinue
  $arg="--policy ppo --load runs\ft_gen.pt --weather --allow-cook --ent-coef $($v.ent) --critic-warmup 32 --lr $($v.lr) --ppo-target-kl 0.08 --ppo-hunt-shaping $($v.hunt) --ppo-repertoire-shaping $($v.rep) --ppo-warmth-shaping $($v.warm) --n-envs 1 --steps 2000000 --save-path runs\ft_$($v.tag).pt --save-every 25 --metrics runs\ft_$($v.tag).jsonl --seed $($v.seed)"
  $cmd="cmd /c `"cd /d `"$dp`" && `"$exe`" $arg > `"$out`" 2> `"$err`"`""
  $p=Invoke-CimMethod -ClassName Win32_Process -MethodName Create -Arguments @{CommandLine=$cmd}
  "$((Get-Date).ToString('s')) detached_launch tag=$($v.tag) pid=$($p.ProcessId)" | Tee-Object -FilePath "$dp\runs\dp_detached_log.txt" -Append
  Start-Sleep -Seconds 5
}
Start-Sleep -Seconds 20
nvidia-smi --query-gpu=memory.used,memory.total,utilization.gpu --format=csv,noheader,nounits
Get-CimInstance Win32_Process | Where-Object { $_.CommandLine -match 'ft_hyp_.*_bg' } | ForEach-Object { "$($_.ProcessId) $($_.Name) $($_.CommandLine)" }
