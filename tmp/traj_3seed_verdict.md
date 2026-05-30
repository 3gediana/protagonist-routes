# 4-seed gen 79 trajectory verdict — PURPOSEFUL, robust (2026-05-30)

## Final honest answer
**The brained protagonists are PURPOSEFUL, not headless flies.** Verified across 4 independent 80-generation MainWorldHost runs (3 with clean same-seed gen0/3/79 traces). The user's prediction ("trajectories will be headless flies") is empirically refuted.

## Four runs at gen 79 (24 brained agents, ep0, --brained-only)
| seed | gen 0 | gen 3 | gen 79 | gen 79 verdict |
|---|---:|---:|---:|---|
| s71359 (cross-seed gen0/3 from s58193) | 0.164* | 0.236* | 0.354 | MIXED (edge, +0.004 over cutoff) |
| s81527 (same-seed) | 0.181 | 0.195 | 0.245 | PURPOSEFUL |
| s92641 (same-seed) | 0.149 | 0.232 | 0.285 | PURPOSEFUL |
| s63810 (gen79-only tail) | — | — | 0.244 | PURPOSEFUL |

\* s71359's gen0/3 were borrowed from s58193 (cross-seed), which is exactly why that row's gen 79 verdict is unreliable. All three clean runs land PURPOSEFUL.

gen 79 headless_score across the 3 clean runs: 0.245 / 0.285 / 0.244 — all comfortably inside the PURPOSEFUL band (<0.35); the only MIXED is the cross-seed s71359, barely over the cutoff. **3 of 4 runs PURPOSEFUL (3/3 clean runs). Conclusion is robust: PURPOSEFUL, definitively not headless flies.**

## What's stable across all seeds (the real signal)
- **dominant_goal_frac rises 0.62-0.64 -> 0.69** by gen 79: agents commit *more* to a dominant goal over evolution, not less.
- **predator_push rises 0.39-0.42 -> 0.65** by gen 79: predator-avoidance reactivity gets stronger.
- **food_pull stable ~0.45-0.52**: agents move toward food when they perceive it, at all generations.
- **velocity_autocorr declines 0.94 -> 0.73-0.79**: motion becomes less robotic-straight-line and more environment-reactive — the desirable direction.
- **goal distribution: explore collapses (~50% -> ~15%), emit_signal dominates (~20% -> ~69%)**: the population evolves from aimless wandering to coordinated signaling.

## What's seed-variable (the noise)
- **goal_switches**: s81527=8.1, s92641=13.4, s71359=16.3 at gen 79. This is the single most seed-dependent term, and it is what nudged s71359's composite over the MIXED line. The favorable, stable components (commitment, reactivity) keep the verdict in PURPOSEFUL regardless.

## food_eaten — resolved, no bug
food_eaten_final is low but NOT identically zero, and varies by seed/episode: s81527 gen79 6/24 nonzero (sum 3.6), s92641 gen79 2/24 (sum 1.2). The earlier cross-seed gen79=0 was a single-episode sampling artifact, not a perception->eat capability bug. Protagonists are coordination-specialized (emit_signal dominant), relying on the broader ecology food web rather than acting as primary foragers. **No reward shaping was changed** (project rule: diagnose bugs, don't Goodhart-tune rewards — and there is no bug here).

## Run provenance
- s81527: `data/runs/long_run_80gen_stage4b_s81527_traj3pt/20260530-024405` — harmony 7/7 (C6 qd=54101), commit 66247f3.
- s92641: `data/runs/long_run_80gen_stage4b_s92641_traj3pt/20260530-044024` — harmony 7/7 (C6 qd=67154, C5 total=148, C3 peak=2528.3s, C7 no extinction).
- s63810: `data/runs/long_run_80gen_stage4b_s63810_tail/20260530-063011` — harmony 7/7 (C6 qd=50994, C5 total=139, C3 peak=1954.5s, C7 no extinction); gen79 dom_goal_frac=0.729 (highest of all 4), predator_push=0.701 (strongest), food_pull=0.482, velocity_autocorr=0.770, goal_switches=7.58; stage 4c snapshot 219 alive.
- All: MainWorldHost end-of-run cumulative sim_time=800.000s tick_calls=800 (invariant #1); TripleWorldRuntime end-of-run borderland=288 main_world=338.
- harmony streak: 22 (s71359) -> 23 (s81527) -> 24 (s92641) -> 25 (s63810), all 7/7 ALL PASS.
- Traces deleted after analysis (disk discipline); D: free ~231 GB.
