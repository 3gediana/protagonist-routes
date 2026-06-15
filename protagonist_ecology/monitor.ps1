foreach ($s in @('50001','50002','50003')) {
  $log = 'D:\claude-code\c++\routes\protagonist_ecology\sD_' + $s + '.log'
  Write-Output ('==================== seed ' + $s + ' ====================')
  if (-not (Test-Path $log)) { Write-Output 'no log'; continue }
  $running = @(Get-Process -Name 'neural-eco-protagonist-batched' -ErrorAction SilentlyContinue).Count
  Write-Output ('overflow=' + @(Select-String -Path $log -Pattern 'slice overflow').Count + '  errors=' + @(Select-String -Path $log -Pattern '\[error\]','\[critical\]').Count)
  # latest generation summary line
  Select-String -Path $log -Pattern 'Gen \d+ \|' | Select-Object -Last 2 -ExpandProperty Line
  # any extinction event
  $ext = @(Select-String -Path $log -Pattern 'extinct').Count
  Write-Output ('extinction_mentions=' + $ext)
}
Write-Output ('total_PE_procs=' + @(Get-Process -Name 'neural-eco-protagonist-batched' -ErrorAction SilentlyContinue).Count)
Write-Output ('DP_running=' + @(Get-Process -Name 'deep_protagonist_train' -ErrorAction SilentlyContinue).Count)
