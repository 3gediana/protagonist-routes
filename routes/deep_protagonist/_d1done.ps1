$dp = 'D:\claude-code\c++\routes\deep_protagonist'
$procs = (Get-Process deep_protagonist_train -ErrorAction SilentlyContinue | Measure-Object).Count
Write-Output ("procs=" + $procs)
$now = Get-Date
foreach ($a in @('ab_ctrl_v2','ab_d1','ab_mask030','ab_d1_mask030','ab_d1_s42')) {
  $m = Join-Path $dp ('metrics\' + $a + '.jsonl')
  if (Test-Path $m) {
    $fi = Get-Item $m
    $eps = (Get-Content $m | Measure-Object).Count
    $age = [int]($now - $fi.LastWriteTime).TotalSeconds
    $state = if ($age -le 25) { 'LIVE' } else { 'IDLE(done?)' }
    Write-Output ($a + "  eps=" + $eps + "  ageSec=" + $age + "  " + $state)
  } else {
    Write-Output ($a + "  (queued / no file)")
  }
}
