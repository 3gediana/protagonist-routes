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
