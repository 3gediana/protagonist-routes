# PE multi-behavior early-generation predictor (D-123)

`analysis/pe_early_predictor.py` — protagonist_ecology's analogue of
deep_protagonist's few-minutes early-warning dashboard. A full 80-gen long-run
takes hours; this reads only the **early generations (0..15)** of a run and
forecasts the **gen-79 verdict for every behavior channel at once**, so a
parameter change can be screened in minutes before committing to a full run.

## What it reads (per generation, all from existing run artifacts)
- **Rhythm** (the Phase-B bottleneck), harmonic regression on per-tick traces
  `trace/generation_N/episode_0/agent_trace.csv` (same math as
  `analysis/phase_harmonic.py` + `rhythm_agg.ps1`):
  `fire_amp` (daily fire-seeking amplitude; owner ceiling ~0.009),
  `fire_r2` (day-locked-ness; gate >0.05), `fire_lead` (seconds the fire peak
  LEADS the cold trough; >+6s = anticipation), and `speed_*` (activity vs light).
- **Harmony C1-C7 floors** from per-gen CSVs (`fitness.csv`,
  `protagonist_behaviors.csv`, `species_metrics.csv`): C1 hunters active,
  C2 herbivore attrition, C3 coop co-presence, C4 scavengers fed,
  C5 worksites, C6 me_coverage>=0.30, C7 no-extinction.
- **Raw behavior rates**: hunt_successes, worksites, coop seconds, signal resp.

## Forecasters
- `persistence` = mean of the window's last 5 early gens (gen 11-15).
- `linear` = LS line over gen 0..15, evaluated at gen 79.
- `LOSO-affine` = leave-one-seed-out affine calibration `final ~ A*early + B`
  fit on the other seeds (used only in `validate`; quantifies how much a small
  set of existing runs improves the early->final mapping).

## Usage
```
G:\ai\python11\python.exe analysis\pe_early_predictor.py report   <run_dir>
G:\ai\python11\python.exe analysis\pe_early_predictor.py validate <run_dir> [<run_dir> ...]
```
`report` prints the early->gen79 forecast table + a one-line RHYTHM VERDICT and
the harmony floors at early vs gen79. `validate` forecasts gen79 from gen0..15
and compares to the actual gen79 we already have, printing per-channel MAE.

## Validation (3 sC long-runs: onepot_sC_long_50001/2/3, gen0..15 -> gen79)
| channel | actual g79 (mean) | MAE persistence | MAE linear | MAE LOSO-affine | verdict |
|---|---|---|---|---|---|
| fire_amp  | 0.0052 | **0.0020** | 0.0037 | **0.0006** | trust persistence; LOSO tightens to ~0.0006 |
| fire_r2   | 0.071  | 0.028 | 0.061 | 0.051 | persistence ok (present/absent reliable) |
| fire_lead | 17.7s  | 11.2s | 67.9s | 21.0s | magnitude noisy, **lead/lag SIGN correct on all 3 seeds** |
| coverage  | 0.307  | 0.083 | 0.695 | 0.133 | early UNDERESTIMATES (rises w/ convergence) |
| hunt_succ | 2.3    | 6.5   | 43.7  | 8.6   | **noisy early — do not trust** |
| worksites | 2.0    | **0.1** | 6.8 | **0.0** | rock-solid |

Harmony pass-count early(15) vs gen79: matches within +/-1 on all seeds; the
only flapping criterion is **C6 coverage** (50002 early 6/7->gen79 7/7, 50003
early 7/7->gen79 6/7), consistent with the known "short runs haven't converged
me_coverage yet" effect. C1-C5,C7 (the behavior-emergence floors) are stable.

## How to use it (the point)
1. Run a short smoke of a candidate config to ~gen15 (minutes).
2. `report` it. Read the **RHYTHM VERDICT** (present? anticipation lead/lag?
   amp approaching the 0.009 ceiling?) and the persistence forecasts.
3. **Trust**: rhythm present/absent, fire_lead LEAD/LAG sign, worksites,
   qd_score direction. **Distrust early**: absolute fire_lead magnitude,
   hunt_successes, and a C6/coverage *fail* (it under-reads before convergence).
4. Decide go/no-go before spending hours on the 80-gen run. To break the
   amplitude ceiling, the early persistence amp must already be trending toward
   ~0.009 — none of the current sC seeds do (they plateau ~0.004-0.008), which
   is exactly the bottleneck the A(scale)+E(pre-build) co-tune must move.

LINEAR extrapolation is unreliable over a 64-gen horizon (overshoots wildly) and
is shown for diagnostics only; the verdict uses persistence.
