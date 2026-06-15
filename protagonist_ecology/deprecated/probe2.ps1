$log = 'D:\claude-code\c++\routes\protagonist_ecology\sD_50001.log'
Write-Output ('procs=' + @(Get-Process -Name 'neural-eco-protagonist-batched' -ErrorAction SilentlyContinue).Count)
Write-Output ('log_size=' + (Get-Item $log).Length)
Write-Output ('overflow_errors=' + @(Select-String -Path $log -Pattern 'slice overflow').Count)
Write-Output '--- generation-boundary / fitness / verdict / extinct lines (last 12) ---'
Select-String -Path $log -Pattern 'gen=','Generation','champion','fitness','verdict','extinct','promote','living' |
  Select-Object -Last 12 -ExpandProperty Line
Write-Output '--- last 6 SandboxDiag (watch fires + health) ---'
Select-String -Path $log -Pattern 'SandboxDiag' | Select-Object -Last 6 -ExpandProperty Line
