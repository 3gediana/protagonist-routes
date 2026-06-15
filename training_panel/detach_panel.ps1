$d = 'D:\claude-code\c++\routes\training_panel'
$cmd = 'cmd.exe /c "cd /d ' + $d + ' && .venv\Scripts\python.exe -m uvicorn server:app --host 127.0.0.1 --port 8070 > panel.log 2>&1"'
$r = Invoke-CimMethod -ClassName Win32_Process -MethodName Create -Arguments @{ CommandLine = $cmd }
Write-Output ('create rc=' + $r.ReturnValue + ' pid=' + $r.ProcessId)
Start-Sleep -Seconds 9
if (Get-NetTCPConnection -LocalPort 8070 -State Listen -ErrorAction SilentlyContinue) { Write-Output 'WIN LISTEN 8070' } else { Write-Output 'WIN NOT listening' }
Get-Content ($d + '\panel.log') -Tail 6 -ErrorAction SilentlyContinue
