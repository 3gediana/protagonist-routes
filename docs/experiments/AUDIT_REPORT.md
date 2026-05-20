# Emergence Audit Report

## Metrics (last 50 generations average)

| Metric | Value | Threshold | Classification |
|--------|-------|-----------|----------------|
| signal_behavior_mi_avg | 0.1176 | 0.3 | ⚠️弱信号 |
| modulatory_usage_ratio | 0.0000 | 0.3 | ❌空管道 |
| modulatory_corr | 0.0000 | 0.3 | ❌空管道 |
| pred_prey_fitness_corr | 0.0916 | -0.2 | ❌空管道 |
| heavy_tasks_completed | 0.0000 | 5 | ❌空管道 |
| host_immunity_pathogen_virulence_corr | 0.0000 | 0.3 | ❌空管道 |
| species_dig_preference_var | 0.0000 | 0.1 | ❌空管道 |
| cave_occupancy_ratio | 0.0000 | 0.01 | ❌空管道 |
| hunt_success_rate | 0.0048 | 0.1 | ⚠️弱信号 |

## Module-Level Assessment

| Module | Metrics | Assessment |
|--------|---------|------------|
| F-COEVO | ❌空管道 | ❌ 空管道 |
| F-COMM | ⚠️弱信号 | ⚠️ 降级 |
| F-COOP | ❌空管道 | ❌ 空管道 |
| F-DIG | ❌空管道, ❌空管道 | ❌ 空管道 |
| F-DISEASE | ❌空管道 | ❌ 空管道 |
| F-NEUROMOD | ❌空管道, ❌空管道 | ❌ 空管道 |
| F-PRED | ⚠️弱信号 | ⚠️ 降级 |

## Recommendations

- **signal_behavior_mi_avg**: Current value 0.1176 below threshold 0.3. Consider increasing backprop_factor or backprop_window.
- **modulatory_usage_ratio**: Current value 0.0000 below threshold 0.3. Consider increasing add_modulatory_rate in mutation config.
- **modulatory_corr**: Current value 0.0000 below threshold 0.3. May need parameter tuning or longer evolution.
- **pred_prey_fitness_corr**: Current value 0.0916 below threshold -0.2. May need parameter tuning or longer evolution.
- **heavy_tasks_completed**: Current value 0.0000 below threshold 5. Consider reducing required_workers or required_ticks.
- **host_immunity_pathogen_virulence_corr**: Current value 0.0000 below threshold 0.3. May need parameter tuning or longer evolution.
- **species_dig_preference_var**: Current value 0.0000 below threshold 0.1. May need parameter tuning or longer evolution.
- **cave_occupancy_ratio**: Current value 0.0000 below threshold 0.01. May need parameter tuning or longer evolution.
- **hunt_success_rate**: Current value 0.0048 below threshold 0.1. May need parameter tuning or longer evolution.
