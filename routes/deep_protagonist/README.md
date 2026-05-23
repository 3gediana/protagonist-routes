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
| S4 | PPO loop + 5-10M model training | next |
| S5 | Tools + fire + 50M scale + long training | planned |
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

# 5) Open the interactive viewer
.\build\bin\deep_protagonist.exe
```

Self-test should report `SELF-TESTS PASSED` with 69 PASS lines.

## Viewer key bindings

| Key | What it does |
|---|---|
| WASD / Space / Shift | Fly camera (or move agent in follow mode) |
| Ctrl | Move 4x faster |
| Mouse | Look around |
| G | Toggle camera follow-agent |
| T / Y | Speed up day cycle (+5x / +20x) |
| E | Harvest nearest ripe plant (mushroom: 30% poison) |
| Q | Drink water in river/swamp; also catches a fish in arm's reach |
| R | Pick up nearby resource (wood / stone / grass) |
| C | Craft spear (consumes 1 wood + 1 stone, frees a slot) |
| B | Build shelter at agent's feet (consumes 1 wood + 1 grass) |
| F | Melee attack closest animal in front (spear adds reach + damage) |
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
