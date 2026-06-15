<#
.SYNOPSIS
  Free disk by deleting ONLY logs/jsonl/trace dumps. KEEPS *.pt checkpoints and *.csv metrics.
.DESCRIPTION
  Cleans logs under the DP runs dir, the PE project, and c++\data. Reports D: free before/after.
  Does NOT stop training (run stop_and_save.ps1 first if needed).
.EXAMPLE
  ... -File cleanup_logs.ps1
.NOTES
  NEVER delete *.pt / *.opt checkpoints here -- see PITFALLS.md (PE warm-base deletion incident).
#>
$ErrorActionPreference = 'SilentlyContinue'
$before = (Get-PSDrive D).Free / 1GB
$roots = @('D:\claude-code\c++\routes\deep_protagonist\runs',
           'D:\claude-code\c++\routes\protagonist_ecology',
           'D:\claude-code\c++\data')
foreach ($r in $roots) {
  if (Test-Path $r) {
    Get-ChildItem -Path $r -Recurse -File -Include *.log,*.stdout.log,*.stderr.log,*.jsonl -EA SilentlyContinue |
      Remove-Item -Force -EA SilentlyContinue
    Get-ChildItem -Path $r -Recurse -Directory -Filter 'trace' -EA SilentlyContinue |
      Remove-Item -Recurse -Force -EA SilentlyContinue
  }
}
$after = (Get-PSDrive D).Free / 1GB
Write-Output ("D_free_before={0:N1}G  after={1:N1}G  freed={2:N1}G" -f $before,$after,($after-$before))
