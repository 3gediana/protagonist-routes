$ErrorActionPreference='SilentlyContinue'
Write-Output '=== Stop all active training processes ==='
$patterns = @('neural-eco-protagonist','deep_protagonist_train')
foreach ($pat in $patterns) {
  $procs = Get-CimInstance Win32_Process | Where-Object { $_.Name -match $pat -or $_.CommandLine -match $pat }
  foreach ($p in $procs) {
    Write-Output ("Stopping PID=" + $p.ProcessId + " NAME=" + $p.Name + " CMD=" + $p.CommandLine)
    Stop-Process -Id $p.ProcessId -Force
  }
}
Start-Sleep -Seconds 3
$left = Get-CimInstance Win32_Process | Where-Object { $_.Name -match 'neural-eco-protagonist|deep_protagonist_train' -or $_.CommandLine -match 'neural-eco-protagonist|deep_protagonist_train' }
Write-Output ("Remaining training processes: " + @($left).Count)
nvidia-smi --query-gpu=memory.used,memory.total,utilization.gpu --format=csv,noheader,nounits
