<#
.SYNOPSIS
  Stop ALL training (DP + PE). Checkpoints are safe: training auto-saves *.pt every 25 updates.
.DESCRIPTION
  Use this when the user wants their machine back. It does NOT delete checkpoints or logs.
  (For freeing disk, run cleanup_logs.ps1 separately.)
.EXAMPLE
  ... -File stop_and_save.ps1
#>
$ErrorActionPreference = 'SilentlyContinue'
Get-Process deep_protagonist_train | Stop-Process -Force
Get-Process neural-eco-protagonist-batched | Stop-Process -Force
Start-Sleep -Seconds 3
$dp = @(Get-Process deep_protagonist_train).Count
$pe = @(Get-Process neural-eco-protagonist-batched).Count
$free = (Get-PSDrive D).Free / 1GB
Write-Output ("STOPPED  dp_left=$dp  pe_left=$pe  D_free={0:N1}G" -f $free)
Write-Output "Checkpoints intact in runs\*.pt (auto-saved every 25 updates)."
