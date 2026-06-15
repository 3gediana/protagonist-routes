Remove-Item 'D:\claude-code\c++\routes\protagonist_ecology\sD_50002.log' -ErrorAction SilentlyContinue
Remove-Item 'D:\claude-code\c++\routes\protagonist_ecology\sD_50003.log' -ErrorAction SilentlyContinue
$p2 = (Invoke-CimMethod -ClassName Win32_Process -MethodName Create -Arguments @{CommandLine='cmd /c D:\claude-code\c++\routes\protagonist_ecology\launch_sD_50002.bat'}).ProcessId
Start-Sleep -Seconds 2
$p3 = (Invoke-CimMethod -ClassName Win32_Process -MethodName Create -Arguments @{CommandLine='cmd /c D:\claude-code\c++\routes\protagonist_ecology\launch_sD_50003.bat'}).ProcessId
Write-Output ('pid_50002=' + $p2 + ' pid_50003=' + $p3)
