param(
  [string]$out="runs\bc_v11.pt",
  [int]$epochs=8,
  [string]$firelist=""   # comma-separated clean fire .bin paths (KEEP set)
)
Set-Location 'D:\claude-code\c++\routes\deep_protagonist'
$exe='build\bin\deep_protagonist_train.exe'

# bc_v9 base = 349 files (NO r8, per Diag-2 dilution lesson)
$dirs = @(
  'runs\demos_d102',
  'runs\demos_d102b',
  'runs\dagger_d102\r4',
  'runs\dagger_d102\r5',
  'runs\dagger_d102\r6',
  'runs\dagger_d102\r7'
)
$files = @()
foreach ($d in $dirs) { $files += (Get-ChildItem "$d\*.bin" -EA SilentlyContinue | Select-Object -Expand FullName) }
$base = $files.Count

# append clean fire DAgger files
if ($firelist -ne "") { $files += ($firelist -split ',' | Where-Object { $_ -ne "" }) }
$total = $files.Count
$fire  = $total - $base

$demos = ($files -join ',')
"START $(Get-Date -Format o) base=$base fire=$fire total=$total out=$out" | Tee-Object "runs\bc_v11_train.log"

& $exe --policy bc --bc-demos $demos --bc-out $out `
       --bc-epochs $epochs --bc-batch 16 --bc-bptt 64 --bc-lr 0.0003 `
       --bc-trigger-weight 15 --bc-trigger-oversample 20 `
       *>> "runs\bc_v11_train.log"
"EXIT $LASTEXITCODE $(Get-Date -Format o)" | Tee-Object "runs\bc_v11_train.log" -Append
