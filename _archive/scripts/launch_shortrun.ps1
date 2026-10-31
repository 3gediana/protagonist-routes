$ErrorActionPreference='Stop'
$pe='D:\claude-code\c++\routes\protagonist_ecology'
$exe='D:\claude-code\c++\build_ninja_cuda\bin\neural-eco-protagonist-batched.exe'
$seeds = @(50022,50023,50024)
foreach ($seed in $seeds) {
    $cfg = "onepot_sD_shortrun_$seed.toml"
    $log = Join-Path $pe "onepot_sD_shortrun_$seed.log"
    $cmd = "cmd /c `"cd /d `"$pe`" && set PATH=D:\NVIDIA\CUDA\v12.8\bin;%PATH%&& `"$exe`" --config $cfg > `"$log`" 2>&1`""
    $p = Invoke-CimMethod -ClassName Win32_Process -MethodName Create -Arguments @{ CommandLine = $cmd }
    Write-Output "launched seed=$seed pid=$($p.ProcessId)"
    Start-Sleep -Seconds 3
}
Start-Sleep -Seconds 10
Write-Output '=== running ==='
Get-CimInstance Win32_Process | Where-Object { $_.CommandLine -match 'onepot_sD_shortrun' } | ForEach-Object { Write-Output ("seed=" + [regex]::Match($_.CommandLine, 'shortrun_(\d+)').Groups[1].Value + " pid=" + $_.ProcessId) }
nvidia-smi --query-gpu=memory.used,memory.total,utilization.gpu --format=csv,noheader,nounits
