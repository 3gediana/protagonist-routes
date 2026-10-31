cd D:\claude-code\c++
$log = "tmp/runlogs/realism_pressure_short.log"
if (Test-Path $log) {
    Write-Host "=== last generation markers ==="
    $lines = Get-Content $log | Select-String -Pattern 'Generation|generation'
    $lines | Select-Object -Last 5 | ForEach-Object { Write-Output $_.Line }
    Write-Host "=== last SandboxDiag ==="
    $sd = Get-Content $log | Select-String -Pattern 'SandboxDiag'
    $sd | Select-Object -Last 2 | ForEach-Object { Write-Output $_.Line }
} else { Write-Host "NOFILE" }
if (Test-Path "tmp/runlogs/realism_pressure_short.done") { Write-Host "STATUS=DONE" } else { Write-Host "STATUS=RUNNING" }
