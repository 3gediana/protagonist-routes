$ErrorActionPreference = 'Continue'
try { Stop-ScheduledTask -TaskName dp_night; Write-Output 'Stop-ScheduledTask dp_night: issued' } catch { Write-Output ('Stop-ScheduledTask err: ' + $_.Exception.Message) }
try { Disable-ScheduledTask -TaskName dp_night | Out-Null; Write-Output 'dp_night disabled (prevents re-trigger)' } catch { Write-Output ('Disable err: ' + $_.Exception.Message) }
Start-Sleep -Seconds 1
taskkill /IM deep_protagonist_train.exe /F 2>&1 | ForEach-Object { Write-Output $_ }
Start-Sleep -Seconds 2
$n = (Get-Process deep_protagonist_train -ErrorAction SilentlyContinue | Measure-Object).Count
Write-Output ('remaining train procs = ' + $n)
$ts = Get-ScheduledTask -TaskName dp_night -ErrorAction SilentlyContinue
if ($ts) { Write-Output ('dp_night state = ' + $ts.State) }
