# gen 79 trajectory honesty verdict (s71359, 2026-05-30)

## TL;DR
**Verdict = MIXED (headless_score 0.354, just over the 0.35 PURPOSEFUL cutoff). The "headless flies" prediction is NOT supported — agents show strong reactive purposefulness.**

## Run provenance
- toml: `tmp/layer1/long_run_80gen_stage4b_s71359_tail.toml` (evolution.generations=80, trace.max_generations=0, trace.tail_generations=1 -> only gen 79 traced)
- run_dir: `data/runs/long_run_80gen_stage4b_s71359_tail/20260530-005217`
- wall: ~90 min (00:52->02:22 UTC), ~75s/gen
- MainWorldHost end-of-run: cumulative sim_time=800.000s tick_calls=800 (invariant #1 verified; world never reset across 80 gens)
- stage 4c snapshot: 219 alive agents (cumulative_seed_count=320)
- harmony_check: 7/7 ALL PASS -> 22/22 streak. C5 worksites total=151 peak=2/gen; C6 me_coverage peak=0.440 qd=83887; C3 coop peak=3337.0s; C7 no extinction.

## gen 79 ep0 aggregate (24 brained agents, --brained-only)
| metric | value | reading |
|---|---:|---|
| headless_score | 0.354 | MIXED (PURPOSEFUL<0.35 / MIXED 0.35-0.55 / HEADLESS>0.55) |
| mean_velocity_autocorr | 0.660 | reactive turning, not dead-straight lines |
| mean_direction_persistence | 0.665 | |
| mean_dominant_goal_frac | 0.608 | committed to a dominant goal 61% of the time |
| mean_goal_switches | 16.33 | high (vs ~5.5 at gen 0) |
| mean_food_pull_fraction | 0.540 | moves toward food 54% of the time it is perceived |
| mean_predator_push_fraction | 0.688 | flees 69% of the time a predator is detected |

## Honest interpretation
- The strong food_pull (0.54) and predator_push (0.69) fractions are the cleanest evidence of purposeful, environment-reactive behavior. This is the opposite of "headless flies".
- The headless_score rose into MIXED almost entirely because goal_switches climbed (~5.5 -> 16.3). But this co-occurs with a collapse in aimless exploration and a 4x shrink in spatial bounding box (see cross-seed trend below), so the switching looks like rapid cycling through purposeful sub-goals rather than chaos.

## Cross-seed trend (INDICATIVE ONLY: gen0/gen3 = s58193 seed 58210; gen79 = s71359 -- different seeds, not the same run)
| metric | gen0 (s58193) | gen79 (s71359) | delta |
|---|---:|---:|---:|
| velocity_autocorr | 0.943 | 0.660 | -0.283 |
| direction_persistence | 0.947 | 0.665 | -0.282 |
| dominant_goal_frac | 0.581 | 0.608 | +0.027 |
| goal_switches | 5.54 | 16.33 | +10.79 |
| bbox_area | 80582 | 19916 | -60666 |
| headless_score | 0.164 | 0.354 | +0.189 |

Goal distribution shift (gen0 -> gen79): explore 43.3% -> 12.3%; emit_signal 29.4% -> 59.5%; respond_signal 24.0% -> 20.2%; find_food 1.3% -> 5.2%; build_worksite 0.3% -> 2.8%. The population shifted from aimless exploration toward coordinated signaling.

## Honest caveat / watch item
- gen79 ep0 brained-agent `food_eaten_final` mean = 0.000. Agents move toward food (food_pull=0.54) but did not actually consume food in this episode. Flagged for follow-up on subsequent runs. (gen0 was also near-zero at 0.125 mean, so this may be an episode-length / sampling artifact rather than a regression, but it is not yet explained.)
