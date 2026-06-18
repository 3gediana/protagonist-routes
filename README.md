# protagonist-routes

C++ monorepo for two AI ecology / survival research projects.

## Projects

| Directory | What it is |
|---|---|
| `deep_protagonist` | Single-agent PPO survival in a 3D natural world (foraging, crafting, building, fire, mining...) |
| `protagonist_ecology` | Multi-agent co-evolution with NEAT + CTRNN -- ecological niche differentiation, cooperation, and communication |
| `training_panel` | Web-based training management dashboard (FastAPI + vanilla JS) |

## Tech Stack

- **C++17** with CMake / Ninja
- **libtorch** (PyTorch C++) for PPO policy networks
- **NEAT** neuroevolution with CTRNN, neuromodulation, and intrinsic motivation
- **CUDA** support for batched dense GPU inference
- **raylib** for 3D visualization
- **Godot** for advanced scene rendering

## Status

Both systems have converged to ~1/3 of their target mechanism width. The root cause is "scalar hill-climbing -> single-strategy collapse". P0 emergence-spectrum baselines (profiling how narrow the spectrum is) are complete. P1 (how to fix) is pending.

## Build

```bash
cmake -B build -G Ninja
cmake --build build
```

## License

Academic / course project -- not currently licensed for distribution.
