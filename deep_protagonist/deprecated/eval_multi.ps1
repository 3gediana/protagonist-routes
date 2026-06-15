param(
  [string]$ckpt="runs\bc_v11.pt",
  [string]$seeds="",
  [string]$tag="v11",
  [int]$eps=10,
  [int]$steps=120000
)
Set-Location 'D:\claude-code\c++\routes\deep_protagonist'
$exe='build\bin\deep_protagonist_train.exe'
New-Item -ItemType Directory -Force -Path runs\eval_v11 | Out-Null
foreach ($s in ($seeds -split ',')) {
  if ($s -eq "") { continue }
  $m = "runs\eval_v11\${tag}_$s.jsonl"
  Remove-Item $m -ErrorAction SilentlyContinue
  & $exe --policy ppo --load $ckpt --no-update --steps $steps --episodes $eps --seed $s --metrics $m `
         *> "runs\eval_v11\${tag}_$s.log"
}
"EVAL_DONE tag=$tag seeds=$seeds" | Out-File "runs\eval_v11\done_$tag.txt" -Encoding ASCII
