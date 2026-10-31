$ErrorActionPreference = 'Continue'
$dp  = 'D:\claude-code\c++\routes\deep_protagonist'
$exe = Join-Path $dp 'build_devin_20260610\bin\deep_protagonist_train.exe'
Set-Location $dp
$steps = '250000'

function RunOne($tag, $extra) {
  $log = Join-Path $dp ("runs\_div_" + $tag + ".log")
  $met = Join-Path $dp ("metrics\_div_" + $tag + ".jsonl")
  $ck  = Join-Path $dp ("checkpoints\_div_" + $tag + ".pt")
  if (Test-Path $met) { Remove-Item $met -Force }
  $a = @('--policy','ppo','--steps',$steps,'--n-envs','8','--seed','7','--metrics',$met,'--save-path',$ck,'--save-every','100000') + $extra
  & $exe @a *> $log
  Write-Output ("RUN " + $tag + " exit=" + $LASTEXITCODE + " metricLines=" + ((Get-Content $met -ErrorAction SilentlyContinue | Measure-Object).Count))
  $hit = Select-String -Path $log -Pattern 'discrete-head decoupling' -SimpleMatch -List
  if ($hit) { Write-Output ("  LOG: D1 decoupling line PRESENT") } else { Write-Output ("  LOG: D1 decoupling line ABSENT") }
}

RunOne 'on'  @('--decouple-disc')
RunOne 'off' @()
Write-Output 'DIVERGE_TEST_DONE'

$on  = Join-Path $dp 'metrics\_div_on.jsonl'
$off = Join-Path $dp 'metrics\_div_off.jsonl'
$h1 = (Get-FileHash $on  -Algorithm MD5).Hash
$h2 = (Get-FileHash $off -Algorithm MD5).Hash
Write-Output ("MD5 on  = " + $h1)
Write-Output ("MD5 off = " + $h2)
if ($h1 -eq $h2) { Write-Output 'RESULT: IDENTICAL -> flag is a NO-OP (BAD)' } else { Write-Output 'RESULT: DIVERGED -> --decouple-disc changes computation (GOOD)' }

# show last episode of each for a human-readable diff
Write-Output '--- ON  last metric line (R/score) ---'
$la = Get-Content $on  | Select-Object -Last 1
if ($la) { ($la | ConvertFrom-Json | Select-Object steps,score,reward,deaths_food | Out-String).Trim() | Write-Output }
Write-Output '--- OFF last metric line (R/score) ---'
$lb = Get-Content $off | Select-Object -Last 1
if ($lb) { ($lb | ConvertFrom-Json | Select-Object steps,score,reward,deaths_food | Out-String).Trim() | Write-Output }
