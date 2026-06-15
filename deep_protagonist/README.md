# deep_protagonist

Single super-agent trained with PPO + libtorch in a real 3D world.
Parallel route alongside `protagonist_ecology` (NEAT-based, group emergence).

## Stage status

| Stage | What | Status |
|---|---|---|
| S0 | Project skeleton + libtorch + CUDA smoke test | done |
| S1 | 3D world (heightmap + caves + physics) | done |
| S2 | 1024m world + 6 biomes + rivers + day-night | done |
| S3.0-3.6 | Plants + animals + vital system mechanics | done |
| S3.7 | PPO prerequisites (action / reward / episode / obs / inventory / shelter) | done |
| S3.8 | Combat / hunting / shelter dampening / fishing / mushroom poison | done |
| S3.9 | Terrain adaptation (swamp slow / desert thirst) | done |
| S3.10 | Full survival main-line (progression / building / clothing / farming / taming) | done |
| **S4-pre** | **AgentAction 19 channels / headless Environment / Metrics / wander baseline / PPO skeleton** | **done** |
| S4 | PPO long-training + curriculum learning + 5-10M model scale | next |
| S5 | Fire + 50M scale + long training (8-16 weeks) | planned |
| S6 | Visual polish + answer-defense polish | planned |

## Quick start

```powershell
# 1) Download libtorch (~2.65GB, one-time)
powershell -ExecutionPolicy Bypass -File scripts\download_libtorch.ps1

# 2) Configure
powershell -ExecutionPolicy Bypass -File scripts\configure.ps1

# 3) Build
powershell -ExecutionPolicy Bypass -File scripts\build.ps1

# 4) Run self-tests (headless, ~2s)
.\build\bin\deep_protagonist.exe --self-test

# 5) Open the interactive viewer (manual play)
.\build\bin\deep_protagonist.exe
```

Self-test should report `SELF-TESTS PASSED` with 110 PASS lines.

## Headless training (S4-pre and beyond)

```powershell
# Wander baseline (no learning, just produces the reference numbers
# in docs/BASELINE.md). 100 episodes, ~17s.
.\build\bin\deep_protagonist_train.exe --policy wander --steps 1000000 `
    --episodes 100 --seed 42 --metrics runs\wander_baseline.jsonl

# PPO smoke test (fresh network, ~10s, 4 updates).
.\build\bin\deep_protagonist_train.exe --policy ppo --steps 10000

# Full PPO run with checkpoints.
.\build\bin\deep_protagonist_train.exe --policy ppo --steps 1000000 `
    --metrics runs\ppo_run1.jsonl `
    --save-path runs\ppo_run1.pt --save-every 50

# Resume from a checkpoint.
.\build\bin\deep_protagonist_train.exe --policy ppo --steps 1000000 `
    --load runs\ppo_run1.pt --metrics runs\ppo_run1_cont.jsonl

# Pure-eval (load checkpoint, no PPO update -- D-039 diagnostic).
.\build\bin\deep_protagonist_train.exe --policy ppo --no-update `
    --load runs\ppo_run3_gru_sm.pt --steps 50000 --episodes 5 `
    --metrics runs\diag.jsonl

# Sweep an env tunable (D-040 toolkit). Requires recompile so the
# binary contains --thirst-decay / --trigger-cost flags.
.\build\bin\deep_protagonist_train.exe --policy ppo --steps 100000 `
    --thirst-decay 8.0 --trigger-cost 0.001 `
    --metrics runs\sweep_thirst_8.jsonl
```

## Inspect a trained checkpoint visually

```powershell
.\build\bin\deep_protagonist.exe --policy ppo --load runs\ppo_run1.pt
```

Camera & day-speed keys still work; agent-action keys are disabled
in PPO mode (network has full control).

## Inspect results

```powershell
# Per-run summary (mean, p10/p50/p90, deaths).
python3 scripts\summarize_metrics.py runs\ppo_run3_gru_sm.jsonl

# ASCII reward curve + four-tier verdict (red/amber/yellow/orange/green
# vs wander baseline).
python3 scripts\plot_training.py runs\ppo_run3_gru_sm.jsonl --window 20

# Compare multiple runs side-by-side (last N stats + verdict per run).
python3 scripts\compare_runs.py runs\wander_baseline.jsonl runs\ppo_run3_gru_sm.jsonl runs\ppo_run6_thirst_slow.jsonl

# Per-action breakdown: shows how often each of the 16 discrete
# triggers fired (D-039 diagnostic). Optional last-N to focus on
# late-stage policy.
python3 scripts\analyze_actions.py runs\ppo_run3_gru_sm.jsonl
python3 scripts\analyze_actions.py runs\ppo_run3_gru_sm.jsonl 20

# Trend: action distribution change first 25% of episodes vs last 25%.
# Reveals "policy collapse" or "habit formation" at a glance.
python3 scripts\analyze_actions.py runs\ppo_run6_thirst_slow.jsonl trend

# Sweep one env parameter with auto-compare. Saves jsonl per value.
python3 scripts\sweep_env.py --param thirst-decay --values 4 6 8 10 12 --steps 100000
python3 scripts\sweep_env.py --param trigger-cost --values 0.0005 0.001 0.002 0.005 --steps 100000
```

## Viewer key bindings

### Movement & camera
| Key | What it does |
|---|---|
| WASD / Space / Shift | Fly camera (or move agent in follow mode) |
| Ctrl | Move 4x faster |
| Mouse | Look around |
| G | Toggle camera follow-agent |
| T / Y | Speed up day cycle (+5x / +20x) |

### Early-stage actions (always available)
| Key | What it does |
|---|---|
| E | Harvest nearest ripe plant (mushroom: 30% poison) |
| Q | Drink water in river/swamp; also catches a fish in arm's reach |
| R | Pick up nearby resource (wood / stone / grass) - or deposit if near a site |
| C | Craft spear (consumes 1 wood + 1 stone) |
| B | Build quick shelter (1 wood + 1 grass) |
| F | Melee attack (spear adds reach + damage); also drops Fur from kills |
| X | Craft a grass dress (3 grass) |
| Z | Craft a fur cloak (2 fur) |
| V | Wear best clothing in inventory (cloak > dress) |

### Mid-stage actions (unlock: 30 collected resources + 500m explored)
| Key | What it does |
|---|---|
| Tab | Cycle next building type (Shed / WoodHouse / StoneHouse / BigHouse) |
| P | Place a construction site at your feet |

### Late-stage actions (unlock: build 1 house)
| Key | What it does |
|---|---|
| N | Plant a seed from inventory |
| J | Water the nearest farm plot |
| K | Feed grass (rabbit/deer) or fur (wolf) to build trust + follower |
| Esc | Quit |

## Toolchain

- C++ 20, MSVC 2022 (Build Tools)
- CMake 3.25+, Ninja
- CUDA 12.8 (driver 13.x compatible)
- libtorch 2.11.0 + cu128

## Layout

```
deep_protagonist/
  AGENTS.md            # AI collaboration contract (read first)
  CMakeLists.txt
  README.md
  src/                 # C++ source
  include/             # C++ headers
  configs/             # *.toml runtime configs
  docs/                # PROGRESS + 4 quick-reference docs (Chinese)
                       # decisions log / Q&A / param ref / glossary
  scripts/             # build / training / analysis helpers
                       # *.ps1 = build, *.py = analysis
  third_party/         # libtorch goes here (gitignored, ~2.6GB)
  runs/                # jsonl metrics + .pt checkpoints (gitignored)
                       # naming convention: ppo_run{N}_{tag}.{jsonl,pt}
  cmake/               # custom CMake modules (later)
  build/               # generated, gitignored
```

## Decision log shortcut

| Decision | Date | TL;DR |
|---|---|---|
| D-035 | 2026-05-24 | S4-pre: action 19 + Environment + PPO skeleton + baseline |
| D-036 | 2026-05-24 | reward hacking fix: `eat/drink` return actual_delta |
| D-037 | 2026-05-24 | Memory: GRU + SpatialMemory (not DNC) |
| D-038 | 2026-05-24 | Visualisation deferred until training stabilises |
| D-039 | 2026-05-24 | Discrete head Bernoulli → Categorical (4-version sweep) |
| D-040 | 2026-05-24 | thirst_decay 12 → 4/min so drink stops dominating |

Full text in `docs/决策记录.md`.
