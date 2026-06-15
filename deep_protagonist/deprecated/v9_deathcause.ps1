param([string]$seeds="2100000,1000000,555000,1700000,2200102",[int]$eps=10,[int]$steps=120000)
Set-Location 'D:\claude-code\c++\routes\deep_protagonist'
$exe='build\bin\deep_protagonist_train.exe'
New-Item -ItemType Directory -Force -Path runs\fire_smoke | Out-Null
foreach ($s in ($seeds -split ',')) {
  $m = "runs\fire_smoke\v9_$s.jsonl"
  Remove-Item $m -ErrorAction SilentlyContinue
  & $exe --policy ppo --load runs\bc_v9.pt --no-update --steps $steps --episodes $eps --seed $s --metrics $m *> "runs\fire_smoke\v9_$s.log"
}
"V9_DEATHCAUSE_DONE" | Out-File "runs\fire_smoke\v9done.txt" -Encoding ASCII
