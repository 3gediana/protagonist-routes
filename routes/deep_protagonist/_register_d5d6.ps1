$ErrorActionPreference = 'Stop'
$proj = 'D:\claude-code\c++\routes\deep_protagonist'
Unregister-ScheduledTask -TaskName 'dp_night_d5d6' -Confirm:$false -ErrorAction SilentlyContinue
$action = New-ScheduledTaskAction -Execute 'powershell.exe' `
    -Argument '-NoProfile -ExecutionPolicy Bypass -File "D:\claude-code\c++\routes\deep_protagonist\night_chain_d5d6.ps1"' `
    -WorkingDirectory $proj
$settings = New-ScheduledTaskSettingsSet -AllowStartIfOnBatteries -DontStopIfGoingOnBatteries -ExecutionTimeLimit ([TimeSpan]::Zero)
Register-ScheduledTask -TaskName 'dp_night_d5d6' -Action $action -Settings $settings -Force | Out-Null
Start-ScheduledTask -TaskName 'dp_night_d5d6'
Start-Sleep 5
$t = Get-ScheduledTask -TaskName 'dp_night_d5d6'
$info = Get-ScheduledTaskInfo -TaskName 'dp_night_d5d6'
Write-Output "State=$($t.State) LastRun=$($info.LastRunTime) Result=$($info.LastTaskResult)"
