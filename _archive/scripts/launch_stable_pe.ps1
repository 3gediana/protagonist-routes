$ErrorActionPreference='Stop'
$pe='D:\claude-code\c++\routes\protagonist_ecology'
$exe='D:\claude-code\c++\build_ninja_cuda\bin\neural-eco-protagonist-batched.exe'
foreach ($seed in @(50022,50023,50024)) {
  $cfg="onepot_sD_stable_$seed.toml"
  $log=Join-Path $pe "onepot_sD_stable_$seed.log"
  $cmd = "cmd /c `"cd /d `"$pe`" && set PATH=D:\NVIDIA\CUDA\v12.8\bin;%PATH%&& `"$exe`" --config $cfg > `"$log`" 2>&1`""
  $p = Invoke-CimMethod -ClassName Win32_Process -MethodName Create -Arguments @{ CommandLine = $cmd }
  Write-Output "launched seed=$seed pid=$($p.ProcessId) cfg=$cfg log=$log"
}
Start-Sleep -Seconds 8
nvidia-smi --query-gpu=memory.used,memory.total,utilization.gpu --format=csv,noheader,nounits
$procs=Get-CimInstance Win32_Process | Where-Object { $_.CommandLine -match 'onepot_sD_stable_5002[2-4]\.toml' }
Write-Output ("stable process command count=" + @($procs).Count)
$procs | ForEach-Object { if ($_.CommandLine -match 'onepot_sD_stable_(5002\d)\.toml') { Write-Output ("running seed="+$Matches[1]+" pid="+$_.ProcessId) } }
