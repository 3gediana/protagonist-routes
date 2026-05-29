# trajectory comparison: EARLY vs LATE

- EARLY: `tmp\traj_s81527_gen0.json` (n=24, manifest_gen=0)
- LATE: `tmp\traj_s81527_gen3.json` (n=24, manifest_gen=3)

## Aggregate diagnostic

| metric | EARLY | LATE | delta |
|---|---:|---:|---:|
| mean_velocity_autocorr | 0.899 | 0.878 | -0.021 |
| mean_direction_persistence | 0.909 | 0.866 | -0.043 |
| mean_dominant_goal_frac | 0.622 | 0.648 | +0.027 |
| mean_goal_switches | 5.333 | 6.458 | +1.125 |
| mean_bbox_area | 56476.915 | 64676.340 | +8199.425 |
| mean_path_per_sample | 2.034 | 2.125 | +0.092 |
| headless_score | 0.181 | 0.195 | +0.013 |

- verdict: **EARLY**=PURPOSEFUL, **LATE**=PURPOSEFUL

## Aggregate goal distribution shift

| goal | EARLY | LATE | delta |
|---|---:|---:|---:|
| emit_signal | 32.6% | 42.1% | +9.5% |
| explore | 41.0% | 34.6% | -6.3% |
| find_food | 3.1% | 0.2% | -2.9% |
| respond_signal | 23.3% | 23.1% | -0.2% |

### Distribution: velocity_autocorr
| pct | EARLY | LATE |
|---|---:|---:|
| 5% | 0.468 | 0.578 |
| 25% | 0.917 | 0.847 |
| 50% | 0.978 | 0.901 |
| 75% | 0.989 | 0.955 |
| 95% | 0.997 | 0.987 |
| mean | 0.899 | 0.878 |

### Distribution: direction_persistence
| pct | EARLY | LATE |
|---|---:|---:|
| 5% | 0.582 | 0.639 |
| 25% | 0.893 | 0.813 |
| 50% | 0.973 | 0.880 |
| 75% | 0.993 | 0.953 |
| 95% | 0.997 | 0.987 |
| mean | 0.909 | 0.866 |

### Distribution: dominant_goal_frac
| pct | EARLY | LATE |
|---|---:|---:|
| 5% | 0.397 | 0.527 |
| 25% | 0.557 | 0.570 |
| 50% | 0.640 | 0.663 |
| 75% | 0.707 | 0.693 |
| 95% | 0.800 | 0.820 |
| mean | 0.622 | 0.648 |

### Distribution: goal_switches
| pct | EARLY | LATE |
|---|---:|---:|
| 5% | 2.000 | 3.000 |
| 25% | 4.000 | 5.000 |
| 50% | 5.000 | 7.000 |
| 75% | 7.000 | 8.000 |
| 95% | 9.000 | 10.000 |
| mean | 5.333 | 6.458 |

### Distribution: path_length
| pct | EARLY | LATE |
|---|---:|---:|
| 5% | 382.803 | 381.962 |
| 25% | 495.786 | 513.377 |
| 50% | 667.971 | 646.395 |
| 75% | 732.544 | 732.049 |
| 95% | 844.732 | 856.119 |
| mean | 610.077 | 637.650 |

### Distribution: food_eaten_final
| pct | EARLY | LATE |
|---|---:|---:|
| 5% | 0.000 | 0.000 |
| 25% | 0.000 | 0.000 |
| 50% | 0.000 | 0.000 |
| 75% | 0.000 | 0.600 |
| 95% | 0.600 | 0.600 |
| mean | 0.075 | 0.425 |

