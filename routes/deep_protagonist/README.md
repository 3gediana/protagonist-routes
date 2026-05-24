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

Self-test should report `SELF-TESTS PASSED` with 102 PASS lines.

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
```

## Inspect a trained checkpoint visually

```powershell
.\build\bin\deep_protagonist.exe --policy ppo --load runs\ppo_run1.pt
```

Camera & day-speed keys still work; agent-action keys are disabled
in PPO mode (network has full control).

## Plot results

```powershell
python3 scripts\summarize_metrics.py runs\ppo_run1.jsonl
python3 scripts\plot_training.py runs\ppo_run1.jsonl --window 20
```

`plot_training.py` shows an ASCII reward curve plus a four-tier
"learning verdict" tag against the wander baseline.

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
  scripts/             # PowerShell helpers
  third_party/         # libtorch goes here
  cmake/               # custom CMake modules (later)
  build/               # generated, gitignored
```
