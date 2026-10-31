$ErrorActionPreference = "SilentlyContinue"
cd D:\claude-code\c++
# Stop current s55274 (will restart capturing early-gen traces too)
taskkill /F /IM neural-eco-protagonist.exe 2>$null | Out-Null
Start-Sleep -Seconds 2
$dst = "tmp/layer1/long_run_80gen_stage4b_s55274_tail.toml"
$c = Get-Content $dst -Raw
$c = $c -replace "max_generations = 0", "max_generations = 4"
Set-Content -Path $dst -Value $c -NoNewline
Write-Host "=== toml verify ==="
Select-String -Path $dst -Pattern "random_seed =|runs_dir =|^max_generations|tail_generations|^generations =" | ForEach-Object { $_.LineNumber.ToString() + ': ' + $_.Line }

New-Item -ItemType Directory -Force tmp\runlogs | Out-Null
Remove-Item tmp\runlogs\long_run_80gen_stage4b_s55274_tail.done -ErrorAction SilentlyContinue
Remove-Item tmp\runlogs\long_run_80gen_stage4b_s55274_tail.log -ErrorAction SilentlyContinue
$p = Invoke-CimMethod -ClassName Win32_Process -MethodName Create -Arguments @{CommandLine='cmd.exe /c D:\claude-code\c++\tmp\run_tail_s55274.bat'}
Write-Host ("relaunched PID=" + $p.ProcessId + " rc=" + $p.ReturnValue)
Start-Sleep -Seconds 6
Get-Process neural-eco-protagonist -ErrorAction SilentlyContinue | Select-Object Id,StartTime
