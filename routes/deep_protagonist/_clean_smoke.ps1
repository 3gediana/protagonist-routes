$dp = 'D:\claude-code\c++\routes\deep_protagonist'
$n = (Get-Process deep_protagonist_train -ErrorAction SilentlyContinue | Measure-Object).Count
Write-Output ("running train procs = " + $n)
foreach ($pat in @('metrics\_smoke_*','metrics\_div_*','checkpoints\_smoke_*','checkpoints\_div_*','runs\_smoke_*','runs\_div_*')) {
  $f = Join-Path $dp $pat
  Get-ChildItem $f -ErrorAction SilentlyContinue | ForEach-Object { Remove-Item $_.FullName -Force; Write-Output ("removed " + $_.Name) }
}
Write-Output 'smoke/diverge artifacts cleaned'
