$dp = 'D:\claude-code\c++\routes\deep_protagonist'
$procs = (Get-Process deep_protagonist_train -ErrorAction SilentlyContinue | Measure-Object).Count
Write-Output ("procs=" + $procs)
$gpu = (nvidia-smi --query-gpu=memory.used,memory.free,utilization.gpu --format=csv,noheader | Select-Object -First 1)
Write-Output ("gpu(used,free,util)= " + $gpu)
foreach ($a in @('ab_ctrl_v2','ab_d1','ab_mask030','ab_d1_mask030','ab_d1_s42')) {
  $m = Join-Path $dp ('metrics\' + $a + '.jsonl')
  if (Test-Path $m) {
    $eps = (Get-Content $m | Measure-Object).Count
    $last = Get-Content $m | Select-Object -Last 1
    $st = 0
    if ($last) { try { $st = ([int](($last | ConvertFrom-Json).steps)) } catch { $st = 0 } }
    Write-Output ($a + "  eps=" + $eps)
  } else {
    Write-Output ($a + "  (queued / no file)")
  }
}
# cumulative training steps per arm from the status/err is hard; eps is enough for health
