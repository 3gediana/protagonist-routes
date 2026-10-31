$ErrorActionPreference = "Stop"
$cmd = 'powershell.exe -ExecutionPolicy Bypass -NoProfile -File "D:\claude-code\c++\routes\deep_protagonist\night_run.ps1" -Manifest "D:\claude-code\c++\routes\deep_protagonist\night_manifest_d1_s42.txt" -Steps 10000000 -NEnvs 8 -Concurrency 4 -Tag s42repl'
$proj = 'D:\claude-code\c++\routes\deep_protagonist'
$r = Invoke-CimMethod -ClassName Win32_Process -MethodName Create -Arguments @{ CommandLine = $cmd; CurrentDirectory = $proj }
Write-Output ("ReturnValue=" + $r.ReturnValue + " ProcessId=" + $r.ProcessId)
