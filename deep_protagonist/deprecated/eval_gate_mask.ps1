param([string]$ckpt,[string]$tag,[string]$seeds="700000,900000,1000000,1100000,1700000,1900000,2000000,2100000,2200102",[int]$eps=10,[int]$steps=120000)
Set-Location 'D:\claude-code\c++\routes\deep_protagonist'
$exe='build\bin\deep_protagonist_train.exe'
New-Item -ItemType Directory -Force -Path runs\eval_v11 | Out-Null
Remove-Item "runs\eval_v11\done_$tag.txt" -ErrorAction SilentlyContinue
foreach ($s in ($seeds -split ',')) {
  $s=$s.Trim(); if ($s -eq "") { continue }
  $m = "runs\eval_v11\${tag}_$s.jsonl"
  Remove-Item $m -ErrorAction SilentlyContinue
  & $exe --policy ppo --load $ckpt --ppo-mask-atk-fire --no-update --steps $steps --episodes $eps --seed $s --metrics $m *> "runs\eval_v11\${tag}_$s.log"
}
"EVAL_DONE tag=$tag $(Get-Date -Format o)" | Out-File "runs\eval_v11\done_$tag.txt" -Encoding ASCII
