$ErrorActionPreference = "SilentlyContinue"
cd D:\claude-code\c++
$src = "tmp/layer1/long_run_80gen_stage4b_s63810_tail.toml"
$dst = "tmp/layer1/long_run_80gen_stage4b_s55274_tail.toml"
$c = Get-Content $src -Raw
$c = $c -replace "random_seed = 63810", "random_seed = 55274"
$c = $c -replace 'runs_dir = "data/runs/long_run_80gen_stage4b_s63810_tail"', 'runs_dir = "data/runs/long_run_80gen_stage4b_s55274_tail"'
Set-Content -Path $dst -Value $c -NoNewline
Write-Host "=== toml verify ==="
Select-String -Path $dst -Pattern "random_seed =|runs_dir =|^max_generations|tail_generations|^generations =" | ForEach-Object { $_.LineNumber.ToString() + ': ' + $_.Line }

$bat = "tmp/run_tail_s55274.bat"
$batc = "@echo off`r`ncd /d D:\claude-code\c++`r`nbuild_ninja_cuda\bin\neural-eco-protagonist.exe --config tmp\layer1\long_run_80gen_stage4b_s55274_tail.toml > tmp\runlogs\long_run_80gen_stage4b_s55274_tail.log 2>&1`r`necho done > tmp\runlogs\long_run_80gen_stage4b_s55274_tail.done`r`n"
Set-Content -Path $bat -Value $batc -NoNewline

taskkill /F /IM neural-eco-protagonist.exe 2>$null | Out-Null
New-Item -ItemType Directory -Force tmp\runlogs | Out-Null
Remove-Item tmp\runlogs\long_run_80gen_stage4b_s55274_tail.done -ErrorAction SilentlyContinue
Remove-Item tmp\runlogs\long_run_80gen_stage4b_s55274_tail.log -ErrorAction SilentlyContinue
$p = Invoke-CimMethod -ClassName Win32_Process -MethodName Create -Arguments @{CommandLine='cmd.exe /c D:\claude-code\c++\tmp\run_tail_s55274.bat'}
Write-Host ("launched PID=" + $p.ProcessId + " rc=" + $p.ReturnValue)
Start-Sleep -Seconds 10
Get-Process neural-eco-protagonist -ErrorAction SilentlyContinue | Select-Object Id,StartTime
Write-Host "=== log tail ==="
Get-Content tmp\runlogs\long_run_80gen_stage4b_s55274_tail.log -Tail 4 -ErrorAction SilentlyContinue
