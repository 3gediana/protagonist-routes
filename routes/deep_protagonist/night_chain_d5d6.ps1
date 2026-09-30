# night_chain_d5d6.ps1 — D5 (10M multi-seed replication, 4 arms) then
# D6 (40M scaling, 2 arms) back-to-back in one schtasks task. Registered with
# ExecutionTimeLimit=0 (no time limit) so the ~17.5h chain cannot be killed
# by the Task Scheduler default.
$ErrorActionPreference = 'Continue'
$proj = 'D:\claude-code\c++\routes\deep_protagonist'
Set-Location $proj

Write-Output "=== CHAIN START $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss') ==="
powershell -NoProfile -ExecutionPolicy Bypass -File night_run.ps1 `
    -Manifest night_manifest_d5_repl.txt -Steps 10000000 -NEnvs 8 `
    -Concurrency 4 -SaveEvery 50 -Tag d5_repl
Write-Output "=== D5 DONE $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss') ==="

powershell -NoProfile -ExecutionPolicy Bypass -File night_run.ps1 `
    -Manifest night_manifest_d6_scale.txt -Steps 40000000 -NEnvs 8 `
    -Concurrency 2 -SaveEvery 50 -Tag d6_scale40m
Write-Output "=== D6 DONE $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss') ==="
Write-Output "=== CHAIN COMPLETE ==="
