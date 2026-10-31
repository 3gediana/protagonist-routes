cd D:\claude-code\c++
$bat = "tmp/run_realism_envperc_L1_off.bat"
$batc = "@echo off`r`ncd /d D:\claude-code\c++`r`nbuild_ninja_cuda\bin\neural-eco-protagonist.exe --config tmp\layer1\realism_envperc_L1_off.toml > tmp\runlogs\realism_envperc_L1_off.log 2>&1`r`necho done > tmp\runlogs\realism_envperc_L1_off.done`r`n"
Set-Content -Path $bat -Value $batc -NoNewline
taskkill /F /IM neural-eco-protagonist.exe 2>$null | Out-Null
New-Item -ItemType Directory -Force tmp\runlogs | Out-Null
Remove-Item tmp\runlogs\realism_envperc_L1_off.done -ErrorAction SilentlyContinue
Remove-Item tmp\runlogs\realism_envperc_L1_off.log -ErrorAction SilentlyContinue
$p = Invoke-CimMethod -ClassName Win32_Process -MethodName Create -Arguments @{CommandLine='cmd.exe /c D:\claude-code\c++\tmp\run_realism_envperc_L1_off.bat'}
Write-Host ("launched PID=" + $p.ProcessId + " rc=" + $p.ReturnValue)
Start-Sleep -Seconds 12
Get-Process neural-eco-protagonist -ErrorAction SilentlyContinue | Select-Object Id,StartTime
Get-Content tmp\runlogs\realism_envperc_L1_off.log -Tail 2 -ErrorAction SilentlyContinue
