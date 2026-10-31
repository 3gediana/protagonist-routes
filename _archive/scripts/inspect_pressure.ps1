cd D:\claude-code\c++
Write-Host "=== last 6 SandboxStatus lines (alive/health/injured) ==="
$log = "tmp/runlogs/realism_pressure_smoke.log"
(Get-Content $log) | Select-String -Pattern 'SandboxStatus' | Select-Object -Last 6 | ForEach-Object { $_.Line }
Write-Host "=== extinction / harmony / alive markers ==="
(Get-Content $log) | Select-String -Pattern 'extinct|alive=0|HARMONY|harmony|living=|PASS|FAIL|population collapse' | Select-Object -Last 12 | ForEach-Object { $_.Line }
Write-Host ""
Write-Host "=== TRACE health & temp (pressure run) ==="
$d = Get-ChildItem -Directory data/runs/realism_pressure_smoke | Sort-Object Name | Select-Object -Last 1
$f = (Get-ChildItem -Recurse -Filter agent_trace.csv $d.FullName | Select-Object -First 1).FullName
Write-Host "FILE: $f"
$rows = Import-Csv $f
function Stat($name,$col){ $a = $rows.$col | ForEach-Object {[double]$_}; $mn=($a|Measure-Object -Minimum).Minimum; $mx=($a|Measure-Object -Maximum).Maximum; $av=($a|Measure-Object -Average).Average; Write-Host ("{0,-16} min={1:N3} max={2:N3} avg={3:N3}" -f $name,$mn,$mx,$av) }
Stat "health_ratio" "health_ratio"
Stat "hydration_ratio" "hydration_ratio"
Stat "core_temperature" "core_temperature"
Stat "hunger" "hunger"
$alive0 = ($rows | Where-Object { [double]$_.health_ratio -le 0.0 }).Count
$low = ($rows | Where-Object { [double]$_.health_ratio -lt 0.5 }).Count
Write-Host ("rows total=$($rows.Count)  rows health<=0: $alive0   rows health<0.5: $low")
$cold = ($rows | Where-Object { [double]$_.core_temperature -lt 30.0 }).Count
Write-Host ("rows core_temp<30 (cold threshold, taking dmg): $cold  ($([math]::Round(100.0*$cold/$rows.Count,1))%)")
