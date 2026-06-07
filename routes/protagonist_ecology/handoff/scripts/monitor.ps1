<#
.SYNOPSIS
  Show each PE seed's latest generation line, error/overflow counts, and process count.
.PARAMETER Seeds  Seeds to check. Default the current warm seeds 50021..50024.
.EXAMPLE
  ... -File monitor.ps1
  ... -File monitor.ps1 -Seeds 50031,50032
#>
param([int[]]$Seeds = @(50021,50022,50023,50024))
$pe = 'D:\claude-code\c++\routes\protagonist_ecology'
foreach ($s in $Seeds) {
  $log = "$pe\onepot_sD_warm_$s.log"
  Write-Output ("==================== seed $s ====================")
  if (-not (Test-Path $log)) { Write-Output 'no log'; continue }
  $ovf = @(Select-String -Path $log -Pattern 'slice overflow' -EA SilentlyContinue).Count
  $errs = @(Select-String -Path $log -Pattern '\[error\]','\[critical\]' -EA SilentlyContinue).Count
  $ext = @(Select-String -Path $log -Pattern 'extinct' -EA SilentlyContinue).Count
  Write-Output ("overflow=$ovf errors=$errs extinction=$ext")
  Select-String -Path $log -Pattern 'Gen \d+ \|' -EA SilentlyContinue | Select-Object -Last 2 -ExpandProperty Line
}
Write-Output ("total_PE_procs=" + @(Get-Process neural-eco-protagonist-batched -EA SilentlyContinue).Count +
              "  DP_procs=" + @(Get-Process deep_protagonist_train -EA SilentlyContinue).Count)
