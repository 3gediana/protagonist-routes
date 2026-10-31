$ErrorActionPreference='Stop'
$pe='D:\claude-code\c++\routes\protagonist_ecology'
$exe='D:\claude-code\c++\build_ninja_cuda\bin\neural-eco-protagonist-batched.exe'
$cfg='onepot_sD_persist_ckpt_50023.toml'
$log=Join-Path $pe 'onepot_sD_persist_ckpt_50023.log'
$cmd = "cmd /c `"cd /d `"$pe`" && set PATH=D:\NVIDIA\CUDA\v12.8\bin;%PATH%&& `"$exe`" --config $cfg > `"$log`" 2>&1`""
$p = Invoke-CimMethod -ClassName Win32_Process -MethodName Create -Arguments @{ CommandLine = $cmd }
Write-Output "launched pid=$($p.ProcessId) cfg=$cfg log=$log"
Start-Sleep -Seconds 8
Get-CimInstance Win32_Process | Where-Object { $_.CommandLine -match 'onepot_sD_persist_ckpt_50023\.toml' } | ForEach-Object { Write-Output ("running pid="+$_.ProcessId) }
nvidia-smi --query-gpu=memory.used,memory.total,utilization.gpu --format=csv,noheader,nounits
if (Test-Path $log) { Get-Content $log -Tail 8 }
