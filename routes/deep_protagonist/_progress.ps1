$dp = 'D:\claude-code\c++\routes\deep_protagonist'
$arms = @('ab_treat035_floor0','ab_treat035_ent005','ab_ctrl_s42','ab_treat035_s42','ab_ctrl_locktech','ab_treat035_locktech')
$rx = [regex]'"steps"\s*:\s*(\d+)'
foreach ($a in $arms) {
  $p = Join-Path $dp ("metrics\" + $a + ".jsonl")
  if (Test-Path $p) {
    $lines = Get-Content $p
    $n = $lines.Count
    $sum = 0
    foreach ($l in $lines) { $m = $rx.Match($l); if ($m.Success) { $sum += [int]$m.Groups[1].Value } }
    $last = if ($n -gt 0) { (Get-Item $p).LastWriteTime.ToString('HH:mm:ss') } else { '-' }
    Write-Output ("{0,-24} eps={1,-6} steps~{2,-9} lastWrite={3}" -f $a, $n, $sum, $last)
  } else {
    Write-Output ("{0,-24} (no file)" -f $a)
  }
}
