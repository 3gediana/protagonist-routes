$d = Get-ChildItem -Directory D:/claude-code/c++/data/runs/realism_envperc_smoke | Sort-Object Name | Select-Object -Last 1
$f = (Get-ChildItem -Recurse -Filter agent_trace.csv $d.FullName | Select-Object -First 1).FullName
Write-Host "FILE: $f"
$rows = Import-Csv $f
Write-Host "rows: $($rows.Count)"
Write-Host "--- distinct env values across episode ---"
Write-Host ("season:        " + (($rows.season | Sort-Object -Unique) -join ' '))
Write-Host ("weather:       " + (($rows.weather | Sort-Object -Unique) -join ' '))
$tp = $rows.time_of_day | ForEach-Object { [double]$_ }
$ll = $rows.light_level | ForEach-Object { [double]$_ }
$sp = $rows.season_progress | ForEach-Object { [double]$_ }
$ct = $rows.core_temperature | ForEach-Object { [double]$_ }
$hu = $rows.hunger | ForEach-Object { [double]$_ }
function Stat($name,$a){ $mn=($a|Measure-Object -Minimum).Minimum; $mx=($a|Measure-Object -Maximum).Maximum; $av=($a|Measure-Object -Average).Average; Write-Host ("{0,-16} min={1:N3} max={2:N3} avg={3:N3}" -f $name,$mn,$mx,$av) }
Stat "time_of_day" $tp
Stat "light_level" $ll
Stat "season_progress" $sp
Stat "core_temperature" $ct
Stat "hunger" $hu
Write-Host "--- health/hydration (does pressure bite?) ---"
$hr = $rows.health_ratio | ForEach-Object { [double]$_ }
$hy = $rows.hydration_ratio | ForEach-Object { [double]$_ }
Stat "health_ratio" $hr
Stat "hydration_ratio" $hy
