$ErrorActionPreference='Continue'
$dp='D:\claude-code\c++\routes\deep_protagonist'
$exe="$dp\build_d122_v2\bin\deep_protagonist_train.exe"
Set-Location $dp
$variants=@(
  @{tag='hyp_entropy_n1'; rep=1.5; hunt=0.3; warm=1.0; ent=0.05; lr='2.0e-4'; seed=31},
  @{tag='hyp_huntfocus_n1'; rep=1.0; hunt=0.8; warm=0.8; ent=0.02; lr='1.5e-4'; seed=32}
)
foreach($v in $variants){
  $out="$dp\runs\ft_$($v.tag).stdout.log"; $err="$dp\runs\ft_$($v.tag).stderr.log"
  Remove-Item $out,$err -ErrorAction SilentlyContinue
  $arg="--policy ppo --load runs\ft_gen.pt --weather --allow-cook --ent-coef $($v.ent) --critic-warmup 32 --lr $($v.lr) --ppo-target-kl 0.08 --ppo-hunt-shaping $($v.hunt) --ppo-repertoire-shaping $($v.rep) --ppo-warmth-shaping $($v.warm) --n-envs 1 --steps 2000000 --save-path runs\ft_$($v.tag).pt --save-every 25 --metrics runs\ft_$($v.tag).jsonl --seed $($v.seed)"
  $p=Start-Process $exe -ArgumentList $arg -RedirectStandardOutput $out -RedirectStandardError $err -PassThru -NoNewWindow
  "$((Get-Date).ToString('s')) launch lowmem tag=$($v.tag) pid=$($p.Id)" | Tee-Object -FilePath "$dp\runs\dp_lowmem_log.txt" -Append
  Start-Sleep -Seconds 5
}
Start-Sleep -Seconds 20
nvidia-smi --query-gpu=memory.used,memory.total,utilization.gpu --format=csv,noheader,nounits
Get-CimInstance Win32_Process | Where-Object { $_.Name -match 'deep_protagonist' } | ForEach-Object { "$($_.ProcessId) $($_.Name) $($_.CommandLine)" }
Get-ChildItem "$dp\runs" -Filter 'ft_hyp_*n1*.stdout.log' | % { "--- $($_.Name) ---"; Get-Content $_.FullName -Tail 8 }
Get-ChildItem "$dp\runs" -Filter 'ft_hyp_*n1*.stderr.log' | % { "--- $($_.Name) stderr len=$($_.Length) ---"; Get-Content $_.FullName -Tail 8 }
