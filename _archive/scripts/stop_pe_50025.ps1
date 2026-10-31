$ErrorActionPreference='Stop'
Write-Output '=== Stop PE seed 50025 only ==='
$procs = Get-CimInstance Win32_Process | Where-Object { $_.CommandLine -match 'onepot_sD_warm_50025\.toml' }
if (-not $procs) { Write-Output 'No 50025 process found'; exit 0 }
foreach ($p in $procs) {
  Write-Output ("Stopping PID=" + $p.ProcessId + " CMD=" + $p.CommandLine)
  Stop-Process -Id $p.ProcessId -Force -ErrorAction SilentlyContinue
}
Start-Sleep -Seconds 2
$left = Get-CimInstance Win32_Process | Where-Object { $_.CommandLine -match 'onepot_sD_warm_50025\.toml' }
Write-Output ("Remaining 50025 processes: " + @($left).Count)
$all = Get-CimInstance Win32_Process | Where-Object { $_.CommandLine -match 'onepot_sD_warm_5002[1-5]\.toml' }
Write-Output ("Remaining PE seeds 50021-25 processes: " + @($all).Count)
$all | ForEach-Object { if ($_.CommandLine -match 'onepot_sD_warm_(5002\d)\.toml') { Write-Output ("  seed=" + $Matches[1] + " PID=" + $_.ProcessId) } }
