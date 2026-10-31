cd D:\claude-code\c++
Write-Host "=== threshold member defaults ==="
git grep -n -E 'cold_temp_threshold_|hot_temp_threshold_|thermal_damage_per_second_|hunger_damage_threshold_|dehydration_threshold' -- '*.h'
Write-Host "=== config parse (getOr) for thermal/hunger thresholds ==="
git grep -n -E 'cold_temp_threshold|hot_temp_threshold|thermal_damage|hunger_damage_threshold' -- '*.cpp' | Select-String -Pattern 'getOr|= config|=\s*[0-9]'
Write-Host "=== core_temperature clamp ==="
git grep -rn -E 'core_temperature_\s*=\s*std::clamp|std::clamp.*core|clamp.*20|clamp.*50' -- '*.cpp'
Write-Host "=== ThermalTick file location ==="
git grep -l 'core_temperature_' -- '*.cpp'
