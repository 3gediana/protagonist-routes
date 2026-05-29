# trajectory comparison: gen3 s58193 vs gen79 s71359

- gen3 s58193: `tmp\traj_gen3_ep0.json` (n=24, manifest_gen=3)
- gen79 s71359: `tmp\traj_gen79_ep0.json` (n=24, manifest_gen=79)

## Aggregate diagnostic

| metric | gen3 s58193 | gen79 s71359 | delta |
|---|---:|---:|---:|
| mean_velocity_autocorr | 0.824 | 0.660 | -0.164 |
| mean_direction_persistence | 0.816 | 0.665 | -0.151 |
| mean_dominant_goal_frac | 0.631 | 0.608 | -0.023 |
| mean_goal_switches | 5.875 | 16.333 | +10.458 |
| mean_bbox_area | 67717.970 | 19916.334 | -47801.635 |
| mean_path_per_sample | 2.259 | 2.701 | +0.442 |
| headless_score | 0.236 | 0.354 | +0.118 |

- verdict: **gen3 s58193**=PURPOSEFUL, **gen79 s71359**=MIXED

## Aggregate goal distribution shift

| goal | gen3 s58193 | gen79 s71359 | delta |
|---|---:|---:|---:|
| build_worksite | 0.0% | 2.8% | +2.8% |
| emit_signal | 31.1% | 59.5% | +28.4% |
| explore | 45.1% | 12.3% | -32.8% |
| find_food | 1.7% | 5.2% | +3.5% |
| respond_signal | 22.1% | 20.2% | -1.9% |

### Distribution: velocity_autocorr
| pct | gen3 s58193 | gen79 s71359 |
|---|---:|---:|
| 5% | 0.557 | 0.529 |
| 25% | 0.753 | 0.629 |
| 50% | 0.888 | 0.666 |
| 75% | 0.930 | 0.698 |
| 95% | 0.988 | 0.804 |
| mean | 0.824 | 0.660 |

### Distribution: direction_persistence
| pct | gen3 s58193 | gen79 s71359 |
|---|---:|---:|
| 5% | 0.572 | 0.562 |
| 25% | 0.732 | 0.619 |
| 50% | 0.883 | 0.666 |
| 75% | 0.916 | 0.692 |
| 95% | 0.983 | 0.789 |
| mean | 0.816 | 0.665 |

### Distribution: dominant_goal_frac
| pct | gen3 s58193 | gen79 s71359 |
|---|---:|---:|
| 5% | 0.427 | 0.447 |
| 25% | 0.543 | 0.493 |
| 50% | 0.630 | 0.590 |
| 75% | 0.700 | 0.703 |
| 95% | 0.803 | 0.813 |
| mean | 0.631 | 0.608 |

### Distribution: goal_switches
| pct | gen3 s58193 | gen79 s71359 |
|---|---:|---:|
| 5% | 2.000 | 7.000 |
| 25% | 3.000 | 13.000 |
| 50% | 5.000 | 15.000 |
| 75% | 8.000 | 19.000 |
| 95% | 15.000 | 32.000 |
| mean | 5.875 | 16.333 |

### Distribution: path_length
| pct | gen3 s58193 | gen79 s71359 |
|---|---:|---:|
| 5% | 407.638 | 696.639 |
| 25% | 565.272 | 807.968 |
| 50% | 729.443 | 826.231 |
| 75% | 798.834 | 838.186 |
| 95% | 858.236 | 862.289 |
| mean | 677.849 | 810.421 |

### Distribution: food_eaten_final
| pct | gen3 s58193 | gen79 s71359 |
|---|---:|---:|
| 5% | 0.000 | 0.000 |
| 25% | 0.000 | 0.000 |
| 50% | 0.000 | 0.000 |
| 75% | 0.000 | 0.000 |
| 95% | 0.600 | 0.000 |
| mean | 0.125 | 0.000 |

