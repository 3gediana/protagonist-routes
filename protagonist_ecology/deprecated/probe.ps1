$log = 'D:\claude-code\c++\routes\protagonist_ecology\sD_50001.log'
Write-Output ('procs=' + @(Get-Process -Name 'neural-eco-protagonist-batched' -ErrorAction SilentlyContinue).Count)
Write-Output ('log_size=' + (Get-Item $log).Length)
Write-Output ('overflow_errors=' + @(Select-String -Path $log -Pattern 'slice overflow').Count)
Write-Output ('error_lines=' + @(Select-String -Path $log -Pattern '\[error\]','\[critical\]').Count)
Write-Output '--- warmstart / resume / extend / generation ---'
Select-String -Path $log -Pattern 'One-pot warmstart','resumed full checkpoint','resumed triple_world','brain parameters','generation','extinct','fitness' |
  Select-Object -Last 14 -ExpandProperty Line
Write-Output '--- last SandboxDiag ---'
Select-String -Path $log -Pattern 'SandboxDiag' | Select-Object -Last 1 -ExpandProperty Line
