# trajectory comparison: EARLY vs LATE

- EARLY: `tmp\traj_s81527_gen0.json` (n=24, manifest_gen=0)
- LATE: `tmp\traj_s81527_gen79.json` (n=24, manifest_gen=79)

## Aggregate diagnostic

| metric | EARLY | LATE | delta |
|---|---:|---:|---:|
| mean_velocity_autocorr | 0.899 | 0.785 | -0.113 |
| mean_direction_persistence | 0.909 | 0.781 | -0.128 |
| mean_dominant_goal_frac | 0.622 | 0.690 | +0.068 |
| mean_goal_switches | 5.333 | 8.125 | +2.792 |
| mean_bbox_area | 56476.915 | 38665.617 | -17811.298 |
| mean_path_per_sample | 2.034 | 2.315 | +0.281 |
| headless_score | 0.181 | 0.245 | +0.063 |

- verdict: **EARLY**=PURPOSEFUL, **LATE**=PURPOSEFUL

## Aggregate goal distribution shift

| goal | EARLY | LATE | delta |
|---|---:|---:|---:|
| build_worksite | 0.0% | 2.4% | +2.4% |
| emit_signal | 32.6% | 67.8% | +35.2% |
| explore | 41.0% | 19.0% | -22.0% |
| find_food | 3.1% | 5.0% | +1.9% |
| gather_stick | 0.0% | 0.1% | +0.1% |
| respond_signal | 23.3% | 5.7% | -17.6% |

### Distribution: velocity_autocorr
| pct | EARLY | LATE |
|---|---:|---:|
| 5% | 0.468 | 0.660 |
| 25% | 0.917 | 0.707 |
| 50% | 0.978 | 0.770 |
| 75% | 0.989 | 0.869 |
| 95% | 0.997 | 0.974 |
| mean | 0.899 | 0.785 |

### Distribution: direction_persistence
| pct | EARLY | LATE |
|---|---:|---:|
| 5% | 0.582 | 0.662 |
| 25% | 0.893 | 0.702 |
| 50% | 0.973 | 0.759 |
| 75% | 0.993 | 0.849 |
| 95% | 0.997 | 0.970 |
| mean | 0.909 | 0.781 |

### Distribution: dominant_goal_frac
| pct | EARLY | LATE |
|---|---:|---:|
| 5% | 0.397 | 0.367 |
| 25% | 0.557 | 0.497 |
| 50% | 0.640 | 0.653 |
| 75% | 0.707 | 0.947 |
| 95% | 0.800 | 1.000 |
| mean | 0.622 | 0.690 |

### Distribution: goal_switches
| pct | EARLY | LATE |
|---|---:|---:|
| 5% | 2.000 | 0.000 |
| 25% | 4.000 | 6.000 |
| 50% | 5.000 | 9.000 |
| 75% | 7.000 | 10.000 |
| 95% | 9.000 | 16.000 |
| mean | 5.333 | 8.125 |

### Distribution: path_length
| pct | EARLY | LATE |
|---|---:|---:|
| 5% | 382.803 | 167.065 |
| 25% | 495.786 | 581.011 |
| 50% | 667.971 | 827.072 |
| 75% | 732.544 | 851.260 |
| 95% | 844.732 | 887.920 |
| mean | 610.077 | 694.452 |

### Distribution: food_eaten_final
| pct | EARLY | LATE |
|---|---:|---:|
| 5% | 0.000 | 0.000 |
| 25% | 0.000 | 0.000 |
| 50% | 0.000 | 0.000 |
| 75% | 0.000 | 0.000 |
| 95% | 0.600 | 0.600 |
| mean | 0.075 | 0.150 |

