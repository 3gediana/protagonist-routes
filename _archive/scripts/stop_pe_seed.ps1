param([int]$Seed)
$ErrorActionPreference='Stop'
$pat = "onepot_sD_warm_$Seed\.toml"
Write-Output "=== Stop PE seed $Seed only ==="
$procs = Get-CimInstance Win32_Process | Where-Object { $_.CommandLine -match $pat }
if (-not $procs) { Write-Output "No process found for seed $Seed"; exit 0 }
foreach ($p in $procs) {
  Write-Output ("Stopping PID=" + $p.ProcessId + " CMD=" + $p.CommandLine)
  Stop-Process -Id $p.ProcessId -Force -ErrorAction SilentlyContinue
}
Start-Sleep -Seconds 2
$left = Get-CimInstance Win32_Process | Where-Object { $_.CommandLine -match $pat }
Write-Output ("Remaining seed $Seed processes: " + @($left).Count)
$all = Get-CimInstance Win32_Process | Where-Object { $_.CommandLine -match 'onepot_sD_warm_5002[1-5]\.toml' }
Write-Output ("Remaining PE seed command processes: " + @($all).Count)
$all | ForEach-Object { if ($_.CommandLine -match 'onepot_sD_warm_(5002\d)\.toml') { Write-Output ("  seed=" + $Matches[1] + " PID=" + $_.ProcessId) } }
