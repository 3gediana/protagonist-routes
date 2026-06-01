# PE GPU inference refactor — cohort-resident weights

Isolated copy of `protagonist_ecology` for the deferred GPU-inference architecture
work. The original `protagonist_ecology` + `build_ninja_cuda` are untouched (a PE
long-run is training there); this copy is where the refactor is written and will be
parity-tested once the GPU frees up.

## Problem

The batched-dense path (`ProtagonistBatchedDenseRunner` → `BatchedDenseGpu::forward`)
re-uploads **every agent's full weight matrices to the GPU every tick**
(`weights_in_hidden` alone is `batch · input_dim · hidden_dim` floats ≈ 14 MB at
production dims). But genome weights are **constant over an individual's lifetime** —
only inputs and plastic state (`hidden_io`/`action_io`) change per tick. So ~99% of
the H2D traffic is re-uploading invariant data, which is why PE sits ~82% CPU-bound
(marshal + PCIe) with the GPU only 18–45% utilised.

## Change

Weights become **cohort-resident**: uploaded once when the cohort changes, reused for
every tick in between.

New `BatchedDenseGpu` API (`brain/BatchedDenseGpu.{h,cpp,cu}`):
- `uploadCohort(dims, w_ih, b_h, w_ha, b_a, w_hi, b_i)` — H2D the lifetime-constant
  tensors once; records resident dims on the device handle.
- `forwardResident(dims, inputs, addend, alphas, hidden_io, outputs_io, interface_out)`
  — uploads only the small changing tensors, launches the kernels against the resident
  `d_weights_*`, downloads state. Returns `false` (caller falls back) if no matching
  cohort is resident — guards against multiplying stale weights.
- Legacy `forward()` kept for back-compat / reference; it now also clears the resident
  flag since it re-uploads weights itself.
- Kernel launch factored into a shared `launchDenseKernels()` so resident + legacy
  paths are bit-identical.

Runner (`runtime/ProtagonistBatchedDenseRunner.{h,cpp}`):
- Cohort fingerprint = per-agent `input_hidden_weights.data()` pointers + sampled
  weight bits + dims (FNV-1a). Cheap O(batch), no full-weight hash per tick.
- Same cohort ⇒ marshal only inputs/state and call `forwardResident()` (skips the
  whole weight re-marshal + re-upload). Cohort change (new generation, or membership
  change when an agent dies/spawns) ⇒ `marshalWeights()` + `uploadCohort()` + forward.
- CPU fallback unchanged in numerics (marshals full weights, calls `forwardCpu`).
- `cohort_resident_` tracks host-side residency; the device-side guard recovers from
  any host/device desync by forcing a re-upload.

## Verification

`runtime/test_resident_parity.cpp` (target `neural-eco-resident-parity`):
- **Numeric parity** — random cohort, 200 ticks, asserts
  `max|Δ|` (resident vs legacy), (resident vs CPU), (legacy vs CPU) `< 1e-4`.
- **Traffic model** — reports H2D bytes/cohort and /tick for both paths.
- Builds + runs without a GPU (CPU reference + analytical traffic only), so it is
  CI-safe; full GPU parity runs on the CUDA box.

Status:
- [x] Implementation (all 4 file pairs + test + CMake target).
- [x] CPU-path compile + run (g++ -std=c++20, no-CUDA config): **RESULT PASS**.
      Traffic at test dims (batch 48 / in 64 / h 96): legacy 399.4 MB vs resident
      12.8 MB over 200 ticks → **96.8% reduction (31×)**; ratio rises toward ~99% at
      production weight sizes.
- [ ] Full CUDA build + GPU numeric parity — **pending GPU free** (deferred until the
      PE long-run / DP v3 finish, per the no-source-change-while-training constraint).
- [ ] Merge back into `protagonist_ecology` — only after GPU parity PASS + sign-off.

---

## GPU PARITY RESOLVED (2026-06-01, same day) — runs PASS on real RTX 5060

The "blocked: no nvcc" note above is now SUPERSEDED. A full CUDA toolkit was
located on the box at **`D:\NVIDIA\CUDA\v12.8`** (nvcc V12.8.61 — the earlier
`C:\Program Files\...\CUDA\v11.8/v12.4` were stubs and `CUDA_PATH=G:\ai\nvidia`
was a dead link, which misled the first audit). CUDA 12.8 supports the RTX 5060
Laptop's Blackwell **sm_120**, so the kernels were compiled and run on the
actual GPU for the first time.

**Minimal standalone GPU build** (no full-framework CMake needed; mirrors the
CPU-path build but with nvcc + CUDA define):
```
call vcvars64.bat   (VS2022 BuildTools, MSVC 14.4x)
nvcc -std=c++20 -O2 -DNEURAL_ECO_HAS_PROTAGONIST_CUDA=1 -arch=sm_120 \
     -I<gpuarch_root> \
     runtime/test_resident_parity.cpp brain/BatchedDenseGpu.cpp brain/BatchedDenseGpu.cu \
     -o resident_parity_gpu.exe
```

**On-GPU result (RTX 5060, CUDA 12.8, 48 agents x 200 ticks):**
```
[parity] CUDA device present: yes
[parity] max|Δ| resident-vs-legacy = 0.000e+00   <- cohort-resident is BIT-IDENTICAL to legacy
[parity] max|Δ| resident-vs-cpu    = 7.153e-07   <- within float rounding (<< 1e-4 tol)
[parity] max|Δ| legacy-vs-cpu      = 7.153e-07
[traffic] H2D reduction = 96.8% (31.3x)
[parity] RESULT: PASS
```

### Test-harness bug found and fixed during this validation
The first GPU run reported a spurious `max|Δ| = 1.748` vs CPU (while
resident-vs-legacy was exactly 0). Root cause was in the TEST, not the kernels:
`test_resident_parity.cpp` pre-ran the **entire** CPU trajectory to completion
(so `hC/aC/iC` held the tick-(T-1) state), then in the GPU loop compared the
GPU's **per-tick** state against that **final** CPU snapshot. Same-tick GPU-vs-GPU
gave 0; cross-tick GPU-vs-stale-CPU-final gave 1.748. A lockstep diagnostic
(advancing CPU and GPU together) confirmed true per-tick GPU-vs-CPU is <=4.8e-7
at every tick. Fix: advance `forwardCpu()` **inside** the tick loop so all three
paths are compared at the same tick (no-GPU branch still runs the full CPU loop
for its finite-output sanity check).

Note this also means the earlier "CPU-path parity PASS" was effectively a
no-op comparison: with CUDA undefined, `supported()` is false, so the GPU/compare
block never executed — only the traffic model + a CPU finite-check ran. The real
numeric parity is the GPU run above.

### Revised checklist (final)
- [x] Implementation (4 file pairs + test + CMake target).
- [x] Legacy forward de-duplicated onto shared `launchDenseKernels()`.
- [x] Test-harness lockstep-comparison bug fixed.
- [x] **Full CUDA build (nvcc 12.8, sm_120) + on-GPU numeric parity = PASS**
      (resident==legacy exactly; both vs CPU = 7.15e-7; H2D -96.8%/31.3x).
- [ ] Merge back into `protagonist_ecology` — pending user sign-off (still an
      isolated copy; mainline untouched).

### Measured wall-clock speedup (RTX 5060, CUDA 12.8, forward hot path, T=1000 ticks)
Beyond the -96.8% H2D byte reduction, actual end-to-end forward timing:
```
parity-dims    (weights 1.9MB):  legacy 1.0565 ms/tick -> resident 0.3359 ms/tick = 3.15x
production-ish (weights 21.3MB): legacy 3.6828 ms/tick -> resident 0.7304 ms/tick = 5.04x
```
Speedup scales with genome size: the larger the per-cohort weights, the more the
legacy per-tick re-upload dominates and the more cohort-residency wins. (Micro-bench
of the inference path only; whole-sim speedup depends on inference vs CPU-sim share.)

### CPU-vs-resident at REAL protagonist dims + per-generation caveat
The running PE long-run used the CPU forward path (CUDA was never compiled into
that exe), so the production "before" baseline is CPU, not GPU-legacy. Bench at
real protagonist dims (count=24, hidden=512, action=13, interface~360, in~272;
weights 31.8MB, T=2000 ticks):
```
CPU       = 9.0985 ms/tick   <- production baseline
GPU legacy= 5.0562 ms/tick
GPU resid = 0.6163 ms/tick
CPU->resident = 14.76x ;  legacy->resident = 8.20x
```
(At tiny parity dims the GPU loses to CPU 0.77x -- kernel launch overhead beats
trivial work; the win only appears at the real hidden_dim=512 scale.)

PER-GENERATION CAVEAT (important, not yet profiled): this is the dense-forward
hot path ONLY. With episode_ticks=600 x episodes_per_generation=2 = ~1200
ticks/gen, protagonist dense inference is ~1200*9.1ms ~= 10.9 s/gen on CPU vs
~0.74 s/gen resident (saves ~10 s/gen). Observed per-gen wall time is ~2.5-4 min
(150-240 s), so dense inference is only ~5-7% of a generation -> this refactor
alone trims per-gen by ~10 s (a few %), NOT a 14x per-gen speedup. The forward
got 8-15x faster; per-gen benefit is bounded by inference's share. NOTE the
bench excludes DNC memory ops, per-agent marshalling, and background-species
inference -- if those are significant the share (and savings) could be larger.
An exact per-gen number requires profiling the live sim (% time in forward).

### Takeover TODO (for the agent picking this up)
1. PROFILE (was the next planned step): instrument the dense-forward call in
   `runtime/ProtagonistBatchedDenseRunner.cpp` + the evolution tick loop with
   std::chrono, CPU-build the eco (exe neural-eco-protagonist-rhythm), run a 2-gen
   smoke (copy onepot_sC_long_50001.toml, set generations=2), print forward_ms/total_ms
   to get the real forward share; then per-gen saving = share*(1-1/14.76)*per_gen.
   `git restore` the instrumentation afterward. CPU build suffices (no GPU needed).
2. MERGE (needs user sign-off): port file-by-file (NOT whole-dir copy, mainline has
   one-pot/FOV/Phase C changes the gpuarch copy lacks): brain/BatchedDenseGpu.{cpp,cu,h},
   runtime/ProtagonistBatchedDenseRunner.{cpp,h}, CMakeLists.txt (GPU-arch deltas only),
   runtime/test_resident_parity.cpp. Re-run test_resident_parity (CUDA build) to confirm PASS.
   Confirmed safe for DP: deep_protagonist does NOT reference BatchedDenseGpu.
3. OPTIONAL 3-seed PE re-check (onepot 50002/50003) to confirm the +5..+17s phase lead
   is not a single-seed artifact.
