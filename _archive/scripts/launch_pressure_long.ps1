cd D:\claude-code\c++
$bat = "tmp/run_pressure_long.bat"
$batc = "@echo off`r`ncd /d D:\claude-code\c++`r`nbuild_ninja_cuda\bin\neural-eco-protagonist.exe --config tmp\layer1\realism_pressure_long.toml > tmp\runlogs\realism_pressure_long.log 2>&1`r`necho done > tmp\runlogs\realism_pressure_long.done`r`n"
Set-Content -Path $bat -Value $batc -NoNewline
taskkill /F /IM neural-eco-protagonist.exe 2>$null | Out-Null
New-Item -ItemType Directory -Force tmp\runlogs | Out-Null
Remove-Item tmp\runlogs\realism_pressure_long.done -ErrorAction SilentlyContinue
Remove-Item tmp\runlogs\realism_pressure_long.log -ErrorAction SilentlyContinue
$p = Invoke-CimMethod -ClassName Win32_Process -MethodName Create -Arguments @{CommandLine='cmd.exe /c D:\claude-code\c++\tmp\run_pressure_long.bat'}
Write-Host ("launched PID=" + $p.ProcessId + " rc=" + $p.ReturnValue)
