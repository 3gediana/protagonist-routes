# Training handoff — PE / DP (2026-06-08)

## Executive state

Training was stopped intentionally for cleanup and handoff. D: free space was raised from **142.67 GB** to **205.51 GB**.

Cleanup manifest:

```text
D:\claude-code\c++\CLEANUP_20260608_manifest_010239.txt
```

What was deleted:

- PE trace directories: ~15.86 GB. These were debug/visual trace payloads, not resumable checkpoints.
- PE intermediate `checkpoint_gen*` directories: kept only the latest checkpoint inside each timestamp/run directory.
- Failed DP first-launch attempts with empty metrics (`ft_hyp_{entropy,huntfocus,aggressive}.jsonl`), where the process died with the SSH/PowerShell session.

What was preserved:

- Latest checkpoint in every PE timestamp/run directory.
- PE config files and top-level logs (`onepot_sD_*.toml`, `onepot_sD_*.log`).
- PE CSV/metrics and best-genome files.
- DP successful detached `_bg` metrics/checkpoints and previous major DP checkpoints.

## Important implementation change

PE now has a lifetime MAP-Elites archive mode. The previous design rebuilt the archive from only the current generation, which caused coverage to fill and then disappear. The fix stores archive elite genome copies across generations; new candidates overwrite a cell only when fitness improves.

Relevant source files:

- `D:\claude-code\c++\src_pe\runtime\QualityDiversityArchive.h`
- `D:\claude-code\c++\src_pe\runtime\QualityDiversityArchive.cpp`
- `D:\claude-code\c++\src_pe\runtime\ProtagonistEvolution.h`
- `D:\claude-code\c++\src_pe\runtime\ProtagonistEvolution.cpp`
- `D:\claude-code\c++\src_pe\runtime\ProtagonistEcologyConfig.h`
- `D:\claude-code\c++\src_pe\runtime\ProtagonistEcologyConfig.cpp`
- `D:\claude-code\c++\src_pe\runtime\ProtagonistEcologyRuntime.cpp`

Build was successful with:

```bat
D:\claude-code\c++\build_batched.bat
```

## PE results

Baseline lifetime-archive short-run was positive on all three seeds:

| run | result |
| --- | --- |
| `onepot_sD_shortrun_50022.log` | gen310→329, `me_cov 6/64→19/64`, QD `4189→38674` |
| `onepot_sD_shortrun_50023.log` | gen338→357, `me_cov 11/64→33/64`, QD `9766→56456` |
| `onepot_sD_shortrun_50024.log` | gen330→349, `me_cov 6/64→25/64`, QD `3558→37971` |

Parallel hypothesis sweep was then launched across three seeds:

| family | params | hypothesis |
| --- | --- | --- |
| `explore` | `line_sigma=0.10`, `mutation_boost=1.50`, `hunt_max_bins=3`, `cvt=64` | more exploration |
| `huntwide` | `line_sigma=0.05`, `mutation_boost=1.00`, `hunt_max_bins=5`, `cvt=96` | wider hunt BD axis |
| `hybrid` | `line_sigma=0.08`, `mutation_boost=1.25`, `hunt_max_bins=5`, `cvt=96` | exploration + wider BD |

Final state when stopped for cleanup:

| config | final gen | coverage | QD | note |
| --- | ---: | ---: | ---: | --- |
| `explore_50022` | 381 | `14/64` | `31446` | weak |
| `explore_50023` | 413 | `31/64` | `99071` | good |
| `explore_50024` | 413 | `33/64` | `134537` | good |
| `huntwide_50022` | 380 | `24/96` | `67656` | best 50022 variant |
| `huntwide_50023` | 412 | `41/96` | `138131` | best coverage overall |
| `huntwide_50024` | 414 | `37/96` | `169500` | best QD overall |
| `hybrid_50022` | 379 | `14/96` | `43558` | weak |
| `hybrid_50023` | 412 | `40/96` | `150406` | strong |
| `hybrid_50024` | 413 | `39/96` | `146409` | strong |

Decision:

- Lifetime archive is validated. It fixes the destructive coverage oscillation.
- Best next PE direction: continue **huntwide/hybrid** rather than baseline. The 96-cell hunt axis preserved more diversity and higher QD.
- Seed 50022 is still weak; use `huntwide_50022` as its branch, not `explore` or `hybrid`.

Known quirk:

- Several parallel config runs wrote under `data\runs\onepot_sD_warm_50023\...` even when the scenario was `*_50022` or `*_50024`. Use the log file's `run_dir=` and `resumed full checkpoint from ...` lines as source of truth, not just the parent folder name.

Important preserved PE checkpoints:

```text
D:\claude-code\c++\routes\protagonist_ecology\data\runs\onepot_sD_warm_50023\20260607-222040\checkpoint_gen379  # huntwide_50022
D:\claude-code\c++\routes\protagonist_ecology\data\runs\onepot_sD_warm_50023\20260607-222049\checkpoint_gen409  # huntwide_50023
D:\claude-code\c++\routes\protagonist_ecology\data\runs\onepot_sD_warm_50023\20260607-222058\checkpoint_gen409  # huntwide_50024
D:\claude-code\c++\routes\protagonist_ecology\data\runs\onepot_sD_warm_50023\20260607-222052\checkpoint_gen409  # hybrid_50023
D:\claude-code\c++\routes\protagonist_ecology\data\runs\onepot_sD_warm_50023\20260607-222101\checkpoint_gen409  # hybrid_50024
```

## DP results

Detached low-memory PPO/PBRS hypotheses were run from:

```text
D:\claude-code\c++\routes\deep_protagonist\runs\ft_gen.pt
```

Variants:

| run | best unlocks | result |
| --- | ---: | --- |
| `ft_hyp_entropy_bg.jsonl` | 3/10 | regressed; many food deaths |
| `ft_hyp_huntfocus_bg.jsonl` | 4/10 | best DP smoke; still no unlock breakthrough |
| `ft_hyp_aggressive_bg.jsonl` | 4/10 | no breakthrough; death rate climbed |

Decision:

- Do not extend pure PPO/PBRS DP runs as-is. They re-confirm the 4/10 ceiling and death regression.
- Next DP work should be **oracle demonstrations → BC unlock → PPO stabilization**, not more PBRS-only training.

## Next recommended actions

1. PE: resume long-run from `huntwide_50023`, `huntwide_50024`, and `hybrid_50023` for 100–200 generations.
2. PE: run a smaller targeted fix for 50022 from `huntwide_50022`; do not use the weaker explore/hybrid 50022 branches.
3. PE: fix the run-dir naming quirk before the next parallel sweep so variant outputs are not all under `onepot_sD_warm_50023`.
4. DP: build scripted oracle demos for the missing mechanisms (shelter/cook/collect/mine/tech), then train BC before PPO.
5. Keep agent wakeups low: launch batches detached on the Windows machine and check every ~3000 seconds.
