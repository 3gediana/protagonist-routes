# Same-seed gen 0/3/79 trajectory verdict (s81527, 2026-05-30)

## TL;DR
**All three generations are PURPOSEFUL on a clean same-seed run. gen 79 headless_score = 0.245 (< 0.35 cutoff) = PURPOSEFUL.** The MIXED verdict from the earlier cross-seed run (s71359, 0.354) was a cross-seed artifact, not a real degradation. The `food_eaten=0` caveat is resolved: it was an episode/seed sampling artifact — agents do eat (6/24 nonzero at gen 79).

## Why this run
The previous verdict (gen 79 = MIXED) compared gen 0/3 from seed s58193 against gen 79 from seed s71359 — different seeds, so the trend was only indicative. This run captures gen 0, 3, AND 79 from a single seed (81527) via `trace.max_generations=4` + `trace.tail_generations=1`, giving an apples-to-apples within-run trajectory and a direct check on `food_eaten`.

## Run provenance
- toml: `tmp/layer1/long_run_80gen_stage4b_s81527_traj3pt.toml` (seed 81527, generations=80, trace.max_generations=4, trace.tail_generations=1)
- run_dir: `data/runs/long_run_80gen_stage4b_s81527_traj3pt/20260530-024405`
- wall: ~103 min (02:44->04:27 UTC), ~80s/gen
- MainWorldHost end-of-run: cumulative sim_time=800.000s tick_calls=800 (invariant #1)
- TripleWorldRuntime end-of-run: borderland=288 main_world=338
- harmony_check: 7/7 ALL PASS -> 23/23 streak. C5 worksites total=152 peak=2/gen; C6 me_coverage peak=0.440 qd=54101; C3 coop peak=3131.5s; C7 no extinction.

## Same-seed trajectory (24 brained agents, ep0, --brained-only)
| metric | gen 0 | gen 3 | gen 79 | reading |
|---|---:|---:|---:|---|
| headless_score | 0.181 | 0.195 | 0.245 | all PURPOSEFUL (<0.35) |
| verdict | PURPOSEFUL | PURPOSEFUL | PURPOSEFUL | |
| velocity_autocorr | 0.899 | 0.878 | 0.785 | gentle decline = more reactive, not rigid |
| direction_persistence | 0.909 | 0.866 | 0.781 | same |
| dominant_goal_frac | 0.622 | 0.648 | 0.690 | RISES = more committed to a dominant goal |
| goal_switches | 5.33 | 6.46 | 8.13 | modest rise (cross-seed run was 16.3 = seed-specific) |
| food_pull_fraction | 0.438 | 0.430 | 0.450 | stable ~0.44 (moves to food when perceived) |
| predator_push_fraction | 0.418 | 0.406 | 0.655 | RISES = better predator avoidance by gen 79 |
| bbox_area | 56477 | 64676 | 38666 | shrinks at gen 79 = more focused/clustered |

## food_eaten resolved
| gen | food_eaten sum | mean | nonzero agents | kills sum |
|---|---:|---:|---:|---:|
| 0 | 1.8 | 0.075 | 3/24 | 10 |
| 3 | 10.2 | 0.425 | 9/24 | 10 |
| 79 | 3.6 | 0.150 | 6/24 | 1 |

`food_eaten` is low but NOT identically zero and varies by generation (gen 3 highest). The earlier cross-seed gen 79 happening to be 0.000 was a per-episode/seed artifact, not a systemic perception->eat bug. Protagonists are coordination-specialized (emit_signal dominant), not primary foragers; they move toward food when they perceive it (food_pull 0.45) but rely on the broader ecology. No reward change made (per project rule: fix bugs, don't tune rewards) — and no bug found here.

## Goal distribution shift (same seed, gen 0 -> gen 79)
explore 41.0% -> 19.0%; emit_signal 32.6% -> 67.8%; respond_signal 23.3% -> 5.7%; find_food 3.1% -> 5.0%; build_worksite 0.0% -> 2.4%. The population evolves from aimless exploration toward signal-based coordination — the same direction as the cross-seed trend, now confirmed within a single seed.

## Bottom line
Honest verdict on agent purposefulness: **PURPOSEFUL, not headless flies.** Across an entire 80-generation MainWorldHost run, the brained protagonists keep a high commitment to dominant goals (rising to 0.69), respond to food and predators, and shift toward coordinated signaling. The trajectory metric drifts slightly toward "less rigid" (velocity_autocorr down) which is the *desirable* direction (reactive, not robotic-straight-line), while staying comfortably inside the PURPOSEFUL band.
