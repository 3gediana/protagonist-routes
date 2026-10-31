cd D:\claude-code\c++
$d = Get-ChildItem -Directory data/runs/realism_pressure_short | Sort-Object Name | Select-Object -Last 1
$f = (Get-ChildItem -Recurse -Filter agent_trace.csv $d.FullName | Sort-Object FullName | Select-Object -First 1).FullName
Write-Host "FILE: $f"
$rows = Import-Csv $f
function Stat($name,$col){ $a = $rows.$col | ForEach-Object {[double]$_}; $mn=($a|Measure-Object -Minimum).Minimum; $mx=($a|Measure-Object -Maximum).Maximum; $av=($a|Measure-Object -Average).Average; Write-Host ("{0,-16} min={1:N3} max={2:N3} avg={3:N3}" -f $name,$mn,$mx,$av) }
Stat "health_ratio" "health_ratio"
Stat "core_temperature" "core_temperature"
$low = ($rows | Where-Object { [double]$_.health_ratio -lt 0.5 }).Count
$cold = ($rows | Where-Object { [double]$_.core_temperature -lt 30.0 }).Count
Write-Host ("rows total=$($rows.Count)  health<0.5: $low ($([math]::Round(100.0*$low/$rows.Count,1))%)  cold<30: $([math]::Round(100.0*$cold/$rows.Count,1))%")
