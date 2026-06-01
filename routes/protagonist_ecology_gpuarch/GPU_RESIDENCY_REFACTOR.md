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

## STATUS CORRECTION (2026-06-01, wind-down code pass)

Two updates from a careful re-audit of this isolated copy:

1. **Legacy `forward()` now shares `launchDenseKernels()`.** Previously the legacy
   full-upload path carried its OWN inline copy of the three kernel launches
   (block_size/grid math/kernel args) 鈥?byte-identical to `launchDenseKernels()`
   but duplicated, so the two paths *could* silently drift. `forward()` now calls
   `launchDenseKernels(handle, ..., hidden_addend != nullptr)` exactly like
   `forwardResident()` does, so resident-vs-legacy are now provably the same launch.
   Pure dedup, no numeric change.

2. **The real blocker for GPU parity is a MISSING CUDA TOOLCHAIN, not "GPU busy".**
   Re-audited the box: `nvcc` is not on PATH and not present under
   `C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\{v11.8,v12.4}` (only an
   `extras` stub dir, no `bin`, no `cudart`). The main `build_ninja_cuda` cache
   confirms it: `CMAKE_CUDA_COMPILER:FILEPATH=NOTFOUND` 鈥?i.e. CUDA was never
   actually compiled on this machine; PE/DP run the CPU path. Also no g++/clang/cl
   on PATH outside a VS dev shell. The Linux side (Devin box) has g++ but NO GPU.
   So neither machine I can reach has *both* a CUDA compiler and the GPU.
   - GPU = RTX 5060 Laptop (Blackwell, **sm_120**) 鈫?needs **CUDA >= 12.8** to compile
     `CUDA_ARCHITECTURES native`; the stub v12.4/v11.8 would not support sm_120 even
     if a compiler were present.
   - **To finish true on-GPU numeric parity**: install CUDA Toolkit >= 12.8 (with
     nvcc) on the Windows box, then configure a separate build dir for this copy with
     `-DCMAKE_CUDA_COMPILER=<...>/bin/nvcc.exe -DNEURAL_ECO_ENABLE_PROTAGONIST_CUDA=ON`
     (Ninja + the same MSVC 14.44 toolset as `build_ninja_cuda`) and run target
     `neural-eco-resident-parity` 鈫?it auto-detects the device via
     `BatchedDenseGpu::supported()` and runs the resident-vs-legacy-vs-CPU asserts.

CPU-path parity **RE-CONFIRMED PASS** this pass (g++ 11.4, -std=c++20): max|螖| within
tol; traffic legacy 399.4 MB vs resident 12.8 MB / 200 ticks = **96.8% (31.3x)**.

Revised checklist:
- [x] Implementation (4 file pairs + test + CMake target).
- [x] Legacy forward de-duplicated onto shared `launchDenseKernels()`.
- [x] CPU-path compile + run PASS (re-confirmed 2026-06-01).
- [ ] Full CUDA build + GPU numeric parity 鈥?**blocked: no nvcc on box; needs CUDA>=12.8 for sm_120** (was mis-attributed to "GPU busy").
- [ ] Merge back into `protagonist_ecology` 鈥?only after GPU parity PASS + sign-off.


---

## GPU PARITY RESOLVED (2026-06-01, same day) 鈥?runs PASS on real RTX 5060

The "blocked: no nvcc" note above is now SUPERSEDED. A full CUDA toolkit was
located on the box at **`D:\NVIDIA\CUDA\v12.8`** (nvcc V12.8.61 鈥?the earlier
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
[parity] max|螖| resident-vs-legacy = 0.000e+00   <- cohort-resident is BIT-IDENTICAL to legacy
[parity] max|螖| resident-vs-cpu    = 7.153e-07   <- within float rounding (<< 1e-4 tol)
[parity] max|螖| legacy-vs-cpu      = 7.153e-07
[traffic] H2D reduction = 96.8% (31.3x)
[parity] RESULT: PASS
```

### Test-harness bug found and fixed during this validation
The first GPU run reported a spurious `max|螖| = 1.748` vs CPU (while
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
block never executed 鈥?only the traffic model + a CPU finite-check ran. The real
numeric parity is the GPU run above.

### Revised checklist (final)
- [x] Implementation (4 file pairs + test + CMake target).
- [x] Legacy forward de-duplicated onto shared `launchDenseKernels()`.
- [x] Test-harness lockstep-comparison bug fixed.
- [x] **Full CUDA build (nvcc 12.8, sm_120) + on-GPU numeric parity = PASS**
      (resident==legacy exactly; both vs CPU = 7.15e-7; H2D -96.8%/31.3x).
- [ ] Merge back into `protagonist_ecology` 鈥?pending user sign-off (still an
      isolated copy; mainline untouched).

