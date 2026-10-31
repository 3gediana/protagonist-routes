$ErrorActionPreference='Stop'
$pe='D:\claude-code\c++\routes\protagonist_ecology'
$cfg = Join-Path $pe 'onepot_sD_shortrun_50024.toml'
$text = Get-Content $cfg -Raw
$text = $text -replace 'resume_checkpoint_path = "[^"]+"', 'resume_checkpoint_path = "D:/claude-code/c++/routes/protagonist_ecology/data/runs/onepot_sD_warm_50024/20260607-184226/checkpoint_gen329"'
$text = $text -replace '(?m)^generations = \d+', 'generations = 350'
Set-Content -Path $cfg -Value $text -Encoding UTF8
Select-String -Path $cfg -Pattern 'scenario_name|resume_checkpoint_path|generations =' | ForEach-Object { $_.Line }
$exe='D:\claude-code\c++\build_ninja_cuda\bin\neural-eco-protagonist-batched.exe'
$log = Join-Path $pe 'onepot_sD_shortrun_50024.log'
$cmd = "cmd /c `"cd /d `"$pe`" && set PATH=D:\NVIDIA\CUDA\v12.8\bin;%PATH%&& `"$exe`" --config onepot_sD_shortrun_50024.toml > `"$log`" 2>&1`""
$p = Invoke-CimMethod -ClassName Win32_Process -MethodName Create -Arguments @{ CommandLine = $cmd }
Write-Output "launched 50024 pid=$($p.ProcessId)"
Start-Sleep -Seconds 8
Get-CimInstance Win32_Process | Where-Object { $_.CommandLine -match 'shortrun_50024' } | ForEach-Object { Write-Output ("running pid="+$_.ProcessId) }
nvidia-smi --query-gpu=memory.used,memory.total,utilization.gpu --format=csv,noheader,nounits
