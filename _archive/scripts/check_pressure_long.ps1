cd D:\claude-code\c++
$log = "tmp\runlogs\realism_pressure_long.log"
$done = "tmp\runlogs\realism_pressure_long.done"
$proc = Get-Process neural-eco-protagonist -ErrorAction SilentlyContinue
Write-Host ("PROC_RUNNING=" + [bool]$proc)
if (Test-Path $log) {
  $gens = Select-String -Path $log -Pattern "generation (\d+) complete|Generation (\d+)\b|gen (\d+)" -AllMatches 2>$null | Select-Object -Last 1
  $lastinit = Select-String -Path $log -Pattern "training initialize" | Select-Object -Last 1
  Write-Host ("--- last 6 non-SandboxDiag lines ---")
  Get-Content $log | Where-Object { $_ -notmatch "SandboxDiag" } | Select-Object -Last 6
  Write-Host ("--- last SandboxDiag ---")
  Get-Content $log | Where-Object { $_ -match "SandboxDiag" } | Select-Object -Last 1
}
Write-Host ("STATUS=" + $(if (Test-Path $done) {"DONE"} else {"RUNNING"}))
