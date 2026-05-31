param([string]$seeds="2100000,1000000,555000,2200101,2200102",[int]$eps=10,[int]$steps=150000)
Set-Location 'D:\claude-code\c++\routes\deep_protagonist'
$exe='build\bin\deep_protagonist_train.exe'
New-Item -ItemType Directory -Force -Path runs\fire_smoke | Out-Null
foreach ($s in ($seeds -split ',')) {
  $m = "runs\fire_smoke\oracle_$s.jsonl"
  Remove-Item $m -ErrorAction SilentlyContinue
  & $exe --policy settler --no-update --steps $steps --episodes $eps --seed $s --metrics $m *> "runs\fire_smoke\oracle_$s.log"
}
"ORACLE_FIRE_SMOKE_DONE" | Out-File "runs\fire_smoke\done.txt" -Encoding ASCII
