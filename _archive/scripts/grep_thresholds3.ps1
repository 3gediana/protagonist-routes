cd D:\claude-code\c++
Write-Host "=== SurvivalStatusLayer construction call site (repo-wide) ==="
git grep -n -E 'SurvivalStatusLayer>\(|SurvivalStatusLayer\(' -- '*.cpp'
Write-Host "=== config defaults: cold/hot temp threshold, thermal damage ==="
git grep -n -E 'cold_temp|hot_temp|thermal_damage|hunger_damage' -- routes/protagonist_ecology/runtime/ProtagonistEcologyConfig.h routes/protagonist_ecology/runtime/ProtagonistEcologyConfig.cpp
Write-Host "=== ThermalTick clamp range ==="
git grep -n -E 'clamp|core_temp|setCoreTemperature|[0-9][0-9]\.0f' -- routes/protagonist_ecology/capability/ThermalTickCapability.cpp
