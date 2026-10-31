cd D:\claude-code\c++
Write-Host "=== ProtagonistWorldFactory SurvivalStatusLayer args (220-245) ==="
$f = "routes/protagonist_ecology/runtime/ProtagonistWorldFactory.cpp"
(Get-Content $f)[218..245] | ForEach-Object { $_ }
Write-Host "=== ThermalCoeffs defaults (min/max core temp, ambient, metabolic) ==="
git grep -n -E 'min_core_temperature|max_core_temperature|ambient_|metabolic_|cold_|hot_' -- routes/protagonist_ecology/capability/ThermalTickCapability.h
