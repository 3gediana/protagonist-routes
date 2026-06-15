foreach ($s in @('50002','50003')) {
  $log = 'D:\claude-code\c++\routes\protagonist_ecology\sD_' + $s + '.log'
  Write-Output ('==================== seed ' + $s + ' ====================')
  if (-not (Test-Path $log)) { Write-Output 'no log yet'; continue }
  Write-Output ('log_size=' + (Get-Item $log).Length + '  overflow=' + @(Select-String -Path $log -Pattern 'slice overflow').Count + '  errors=' + @(Select-String -Path $log -Pattern '\[error\]','\[critical\]').Count)
  Select-String -Path $log -Pattern 'resumed full checkpoint','resumed triple_world','One-pot warmstart','Gen \d+ \|' | Select-Object -Last 5 -ExpandProperty Line
}
$procs = @(Get-Process -Name 'neural-eco-protagonist-batched' -ErrorAction SilentlyContinue).Count
Write-Output ('total_PE_procs=' + $procs)
