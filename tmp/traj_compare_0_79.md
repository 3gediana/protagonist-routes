# trajectory comparison: gen0 s58193 vs gen79 s71359

- gen0 s58193: `tmp\traj_gen0_ep0.json` (n=24, manifest_gen=0)
- gen79 s71359: `tmp\traj_gen79_ep0.json` (n=24, manifest_gen=79)

## Aggregate diagnostic

| metric | gen0 s58193 | gen79 s71359 | delta |
|---|---:|---:|---:|
| mean_velocity_autocorr | 0.943 | 0.660 | -0.283 |
| mean_direction_persistence | 0.947 | 0.665 | -0.282 |
| mean_dominant_goal_frac | 0.581 | 0.608 | +0.027 |
| mean_goal_switches | 5.542 | 16.333 | +10.792 |
| mean_bbox_area | 80582.508 | 19916.334 | -60666.174 |
| mean_path_per_sample | 2.111 | 2.701 | +0.590 |
| headless_score | 0.164 | 0.354 | +0.189 |

- verdict: **gen0 s58193**=PURPOSEFUL, **gen79 s71359**=MIXED

## Aggregate goal distribution shift

| goal | gen0 s58193 | gen79 s71359 | delta |
|---|---:|---:|---:|
| build_worksite | 0.3% | 2.8% | +2.5% |
| emit_signal | 29.4% | 59.5% | +30.1% |
| explore | 43.3% | 12.3% | -31.0% |
| find_food | 1.3% | 5.2% | +3.8% |
| gather_stick | 1.7% | 0.0% | -1.7% |
| respond_signal | 24.0% | 20.2% | -3.8% |

### Distribution: velocity_autocorr
| pct | gen0 s58193 | gen79 s71359 |
|---|---:|---:|
| 5% | 0.832 | 0.529 |
| 25% | 0.937 | 0.629 |
| 50% | 0.968 | 0.666 |
| 75% | 0.985 | 0.698 |
| 95% | 1.000 | 0.804 |
| mean | 0.943 | 0.660 |

### Distribution: direction_persistence
| pct | gen0 s58193 | gen79 s71359 |
|---|---:|---:|
| 5% | 0.843 | 0.562 |
| 25% | 0.946 | 0.619 |
| 50% | 0.960 | 0.666 |
| 75% | 0.987 | 0.692 |
| 95% | 1.000 | 0.789 |
| mean | 0.947 | 0.665 |

### Distribution: dominant_goal_frac
| pct | gen0 s58193 | gen79 s71359 |
|---|---:|---:|
| 5% | 0.430 | 0.447 |
| 25% | 0.517 | 0.493 |
| 50% | 0.590 | 0.590 |
| 75% | 0.647 | 0.703 |
| 95% | 0.707 | 0.813 |
| mean | 0.581 | 0.608 |

### Distribution: goal_switches
| pct | gen0 s58193 | gen79 s71359 |
|---|---:|---:|
| 5% | 2.000 | 7.000 |
| 25% | 4.000 | 13.000 |
| 50% | 6.000 | 15.000 |
| 75% | 7.000 | 19.000 |
| 95% | 9.000 | 32.000 |
| mean | 5.542 | 16.333 |

### Distribution: path_length
| pct | gen0 s58193 | gen79 s71359 |
|---|---:|---:|
| 5% | 363.962 | 696.639 |
| 25% | 488.262 | 807.968 |
| 50% | 615.734 | 826.231 |
| 75% | 781.686 | 838.186 |
| 95% | 854.052 | 862.289 |
| mean | 633.435 | 810.421 |

### Distribution: food_eaten_final
| pct | gen0 s58193 | gen79 s71359 |
|---|---:|---:|
| 5% | 0.000 | 0.000 |
| 25% | 0.000 | 0.000 |
| 50% | 0.000 | 0.000 |
| 75% | 0.000 | 0.000 |
| 95% | 0.600 | 0.000 |
| mean | 0.125 | 0.000 |

