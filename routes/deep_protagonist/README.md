# deep_protagonist

Single super-agent trained with PPO + libtorch in a real 3D world.
Parallel route alongside `protagonist_ecology` (NEAT-based, group emergence).

## Stage status

| Stage | What | Status |
|---|---|---|
| S0 | Project skeleton + libtorch + CUDA smoke test | in progress |
| S1 | 3D world (heightmap + caves + physics) | planned |
| S2 | PPO + small model on a simple task | planned |
| S3 | Multi-task / POMDP / complex 3D world | planned |
| S4 | Scale model to 50-60M, long training | planned |
| S5 | Companion world + visualization | planned |
| S6 | Behavior observation + tuning | planned |

## Quick start (S0)

```powershell
# 1) Download libtorch (~2.65GB, one-time)
powershell -ExecutionPolicy Bypass -File scripts\download_libtorch.ps1

# 2) Configure
powershell -ExecutionPolicy Bypass -File scripts\configure.ps1

# 3) Build
powershell -ExecutionPolicy Bypass -File scripts\build.ps1

# 4) Run smoke test
.\build\bin\deep_protagonist.exe
```

Expected output: GPU info, a matmul benchmark, config parse, `S0 SMOKE TEST PASSED`.

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
