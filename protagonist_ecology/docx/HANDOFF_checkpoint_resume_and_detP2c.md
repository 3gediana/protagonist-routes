# HANDOFF — Checkpoint Resume Fix + triple_world Persistence + detP2c gen→160

> Written at EOD wrap-up (budget-limited). Next executor: read this top-to-bottom,
> you can pick up immediately. Local commits only (NOT pushed). Production binary
> never touched — all builds went to the independent `-batched` exe.

---

## 1. Checkpoint-resume validation bug — FIXED + COMMITTED (commit `6908751`)

**Original bug (what John asked to fix):** full-resume aborted because the loader
validated the on-disk background genome count against the *config initial*
`slot.count`. Background species evolve (predator/prey dynamics → counts grow/shrink,
e.g. `pack_hunter` grew to 228/450), so the check falsely rejected every evolved
checkpoint.

**Fix:** validate the on-disk background genome count against the
**checkpoint-recorded `population_count`** (the evolved value) instead of the config
initial `slot.count`. Single-point change in the background load path.

**Verification:**
- Unit test `ProtagonistCheckpointResume.ResumeMatchesFullRunAtNextGeneration` (+ sibling) → **2/2 PASS**.
- Real `gen89` checkpoint (pack_hunter=228 etc.) resumes cleanly, no crash.
- Production exe untouched (mtime 05/31 00:04:47); validated only via `-batched` exe.

Files: `routes/protagonist_ecology/runtime/ProtagonistEcologyRuntime.cpp`
(background load path validation).

---

## 2. triple_world persistence — ADDED (additive, backward-compatible) — same commit `6908751`

Per-gen checkpoint now ALSO writes, under `<checkpoint_gen*>/triple_world/`:
- borderland NEAT pools + main_world NEAT pools (via `MainWorldPersistence`),
- per-pool `borderland_eval_count` (the 50-gen continuity gate ranks promotion on it),
- curriculum `archetype_progression` (5 monotonic unlocks per archetype).

On full resume these are restored:
- snapshot present → load pools + OR-merge curriculum unlock bits (unlocked stays unlocked),
- snapshot present but load fails → loud warn + fresh triple_world,
- snapshot absent (old checkpoint) → loud warn + fresh triple_world (backward compatible).

Files: `MainWorldPersistence.cpp` (added `borderland_eval_count` to JSON
serialize/parse), `ProtagonistEcologyRuntime.cpp` (resume load block ~line 4132,
checkpoint save block ~line 5275, `resume_triple_world_dir` tracking ~line 3881/3915).

---

## 3. KNOWN LIMITATION — triple_world resume is NOT bit-identical (Decision A)

**Decided with John (option A): ship the verified fix + pool persistence now; mark
triple_world resume as a warm-start, NOT a bit-exact continuation.** Resume now logs a
**loud warning** stating this, and points to deterministic replay as the real
reproducibility guarantee.

**What we proved:**
- **Non-triple_world resume IS bit-identical** (unit test passes).
- **triple_world resume restores pools+curriculum but diverges from gen6** (first
  resumed gen) in a 12-gen vs resume-from-gen5 bitcheck — large divergence
  (best fitness ~8404 vs ~22794), i.e. a *different-but-plausible* trajectory.
- **The live MainWorldHost stepping is NOT the cause.** Decisive isolation: with
  `main_world_ticks_per_generation=0` (host does not step), resume STILL diverges at
  gen6. So the divergence is not the persistent World2D sim state.

**Suspected root cause (for whoever later does option B = full bit-identity):**
The per-generation triple_world path — borderland eval / promote / cull /
extinction-rescue — PLUS in-world background reproduction (PopulationLayer births
drained into species pools) consume **runtime-level state that is not captured in
`evolution.exportState()`**. Prime suspect: a hidden runtime RNG / global counter
(e.g. a genome-id / lineage-id counter, or a member RNG used by rescue/migration/
in-world reproduction) that advances across gens 0..5 in a straight run but starts
fresh on resume. Note: the sandbox eval at gen N runs BEFORE that gen's reproduce, on
the restored populations — yet it still diverges, which is consistent with an
already-mis-positioned runtime RNG/counter at resume time (state from gens 0..5 that
was never checkpointed), not with a faulty population restore (counts restore
correctly: apex_predator:2, pack_hunter:36, … were accepted).

**Where to start option B (not started — needs user GO; ~hours, must re-verify
harmony 7/7 + determinism):**
1. Instrument: log the value/position of every runtime RNG + every global
   id/lineage/innovation counter at the end of each gen, straight vs resume; binary-
   search the first gen where they differ at resume start.
2. Add the offending RNG/counter(s) to the checkpoint (export/import alongside
   `evolution.exportState()`), keep additive + backward-compatible.
3. Re-run the bitcheck below until gen6–11 are byte-identical.

---

## 4. How to reproduce the bitcheck (determinism test harness)

Independent `-batched` exe: `build_ninja_cuda\bin\neural-eco-protagonist-batched.exe`
(build via `tmp\bldbatched.bat` → targets `neural-eco-protagonist-batched test_core`
only; production exe NOT relinked).

Configs (in `tmp\layer1\`): `BITCHECK_straight.toml` / `BITCHECK_resume.toml`
(ticks=10) and `BITCHECK_straight0.toml` / `BITCHECK_resume0.toml` (ticks=0 isolation).
Run dirs: `data/runs/_resume_bitcheck_{straight,resume,straight0,resume0}`.

Procedure: straight = 12 gens, `checkpoint_every=6` (writes gen5 + gen11). resume =
resume from straight's `checkpoint_gen5`, run gen6–11. Compare gen6–11 rows of
`evolution.csv` byte-for-byte. Acceptance for option B: identical. Current state
(option A): they diverge at gen6 → warm-start only.

PowerShell gotcha: `"gen $g: DIFF"` breaks ($g: = drive ref); use `("gen {0}: DIFF" -f $g)`.

---

## 5. detP2c gen→160 runs — STATUS + how to finish the analysis

**Why running:** decision round_6 — extend the 6-run deterministic matrix (3 seed ×
ON/OFF, cold-threshold 22 + thermal_damage 0.3, adt=1) from 80 → ~160 gens to judge
whether the ON-only "趋火预判" (fire-approach pre-emption) rhythm **rises** (encoding
suffices, keep selecting) or **plateaus** (encoding is the ceiling → open a new
bloodline, candidates 1+2 = sharpen perceptual encoding / add a warmth-seeking target
channel).

**Run dirs:** `data/runs/realism_detP2c_th22_5000{1,2,3}_{on,off}/…`
**Progress at wrap-up (gen / 160):** 50001_off ~153, 50001_on ~122, 50002_off ~108
(slowest), 50002_on ~145, 50003_off ~149, 50003_on ~155. All 6 alive, production exe.
**Launched via WMI `Win32_Process.Create`** so they are detached from the SSH job
object — they survive session end / SSH disconnect. (Do NOT launch long runs from a
script that returns; Windows OpenSSH kills the child process tree when the session
ends. detP2b survived only because its launcher used a blocking `Wait-Process`.)
Power loss is also non-fatal: adt=1 + fixed seed → deterministic replay reproduces
gen0..N exactly (zero science loss, only recompute time). NOTE: these checkpoints
predate the gen159 finish and (being triple_world) are warm-start-only for resume —
rely on replay, not on bit-exact resume.

**To finish (next executor, once runs reach ~gen159):**
1. Pull gen140–159 traces, then `pe_scripts/build_aggs_p2c.ps1` → aggregate CSVs.
2. `pe_analysis/phase_harmonic.py` → fit harmonic phase-lead on the fire-approach
   channel, protagonist-only, for gen0 / gen79 / gen~150, ON vs OFF.
3. Verdict: compare gen79 vs gen~150 趋火 R²/amplitude/phase-lead.
   - **rise** → encoding sufficient, continue selection.
   - **plateau** → encoding ceiling → open new bloodline (cand 1 sharpen encoding,
     cand 2 add warmth-seeking target channel). Red lines: rhythm NOT forced via
     reward, changing input dims = new bloodline, C7 (no-extinction) floor, harmony
     7/7, adt=1 determinism, fitness is appendix-only.
4. Write the verdict as `round_7__<UTC>.md` into `routes/protagonist_ecology/inbox/`.

---

## 6. Red lines / constraints honored this session

- Production exe (`build_ninja_cuda\bin\neural-eco-protagonist.exe`) never touched.
- deep_protagonist untouched.
- Local commits only, never pushed.
- 6 detP2c runs never interrupted.
- All my edits compiled into the separate `-batched` exe.

## 7. Phase-C prerequisite (separate, already documented)

See `docx/SYNC_BATCHED_EVAL_PHASE_C_PREREQ.md` (commit `c9cdb35`): synchronous batched
GPU eval is a Phase-C-prereq (route-private, AgentLayer additive flag-gated, default
OFF, must re-establish baselines + re-verify harmony/rhythm + needs working CUDA).
The batched-hook math fix is commit `5958f25` (brain forward bit-exact; end-to-end
diverges due to AgentLayer sync-vs-async update order — architectural, GPU-irrelevant;
flag default OFF).
