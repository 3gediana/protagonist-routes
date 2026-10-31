cd D:\claude-code\c++
Write-Host "=== SurvivalStatusLayer construction (threshold values) ==="
git grep -n -E 'cold_temp_threshold_|hot_temp_threshold_|thermal_damage_per_second_|hunger_damage_threshold_' -- routes/protagonist_ecology/world/SurvivalStatusLayer.cpp
Write-Host "=== make_unique<SurvivalStatusLayer> / SurvivalStatusLayer( call site ==="
git grep -n 'SurvivalStatusLayer' -- routes/protagonist_ecology/runtime/BootstrapPopulation.cpp
Write-Host "=== ThermalTick file + clamp/setCoreTemperature ==="
git grep -ln 'ThermalTick' -- routes/protagonist_ecology
git grep -n -E 'setCoreTemperature|clamp|core_temp|kMin|kMax|20.0f|50.0f' -- routes/protagonist_ecology/world/ThermalTickCapability.cpp
