$ErrorActionPreference = 'Continue'
$dp  = 'D:\claude-code\c++\routes\deep_protagonist'
$exe = Join-Path $dp 'build_devin_20260610\bin\deep_protagonist_train.exe'
Set-Location $dp

function RunOne($tag, $extra) {
  $log = Join-Path $dp ("runs\_smoke_" + $tag + ".log")
  $met = Join-Path $dp ("metrics\_smoke_" + $tag + ".jsonl")
  $ck  = Join-Path $dp ("checkpoints\_smoke_" + $tag + ".pt")
  $args = @('--steps','40000','--n-envs','8','--seed','7','--metrics',$met,'--save-path',$ck,'--save-every','100') + $extra
  Write-Output ("=== RUN " + $tag + "  exe-args: " + ($args -join ' ') + " ===")
  & $exe @args *> $log
  Write-Output ("exit=" + $LASTEXITCODE + "  metricLines=" + ((Get-Content $met -ErrorAction SilentlyContinue | Measure-Object).Count))
  Write-Output "--- log lines matching decouple/D1/disc/decoup ---"
  Select-String -Path $log -Pattern 'decoup|D1|disc head|discrete' -SimpleMatch:$false -ErrorAction SilentlyContinue | Select-Object -First 8 | ForEach-Object { Write-Output ("  " + $_.Line.Trim()) }
  Write-Output "--- log tail ---"
  Get-Content $log -Tail 6 -ErrorAction SilentlyContinue | ForEach-Object { Write-Output ("  " + $_.Trim()) }
  Write-Output ""
}

RunOne 'd1on'  @('--decouple-disc')
RunOne 'd1off' @()
