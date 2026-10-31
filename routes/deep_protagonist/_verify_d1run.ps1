$dp = 'D:\claude-code\c++\routes\deep_protagonist\logs'
function CountD1($name) {
  $p = Join-Path $dp ($name + '.log')
  if (-not (Test-Path $p)) { return "NO LOG" }
  return (Select-String -Path $p -Pattern 'discrete-head decoupling' -SimpleMatch | Measure-Object).Count
}
foreach ($a in @('ab_ctrl_v2','ab_d1','ab_mask030','ab_d1_mask030','ab_d1_s42')) {
  Write-Output ($a + ' : D1-log-count=' + (CountD1 $a))
}
