# NEXT SESSION — START HERE (protagonist_ecology)

> You are the next executor (fresh session). Read this top-to-bottom. Everything you
> need is committed in this local repo (commits are local-only, never pushed). The
> previous session ended on a tiny budget after committing all the code + docs below.
> The 6 detP2c gen→160 runs are finishing in the background on the Windows machine.

## 0. Machine / access
- Project root (Windows): `D:\claude-code\c++`. Build dir: `build_ninja_cuda`.
- Production exe (DO NOT overwrite): `build_ninja_cuda\bin\neural-eco-protagonist.exe`.
- Independent experiment exe: `build_ninja_cuda\bin\neural-eco-protagonist-batched.exe`
  (built via `tmp\bldbatched.bat`, targets `neural-eco-protagonist-batched test_core`).
- Analysis scripts (committed this session): `routes/protagonist_ecology/analysis/`
  (`phase_harmonic.py`, `phase_lead.py`, `build_aggs_p2c.ps1`).

## 1. What was finished last session (commits, local-only)
- `6908751` checkpoint resume **validation bug FIXED** + **triple_world persistence**
  (borderland/main pools + borderland_eval_count + curriculum) + loud "not bit-identical"
  warning. Unit test `ProtagonistCheckpointResume` 2/2 PASS. Production exe untouched.
- `00a537b` `docx/HANDOFF_checkpoint_resume_and_detP2c.md` (full story + option-B plan
  for making triple_world resume bit-identical — NOT started, needs user GO).
- `68bb586` `docx/OPS_CONVENTIONS.md` (hard rules: per-gen immediate cleanup; WMI launch;
  resume reality). FOLLOW THESE — especially: extract metrics then delete raw logs every
  gen, never pile up; never touch a running run's logs.
- Earlier: `5958f25` batched-hook math fix (brain forward bit-exact; end-to-end diverges
  by AgentLayer sync-vs-async — architectural, flag default OFF); `c9cdb35`
  `docx/SYNC_BATCHED_EVAL_PHASE_C_PREREQ.md` (Phase-C prereq design).

## 2. The experiment in flight: detP2c gen→160 (THIS is round_7's deliverable)
- **Why:** judge whether the ON-only "fire-approach pre-emption" rhythm (趋火预判) RISES
  or PLATEAUS from gen79 → gen~150. The first honest positive was weak (gen79: ON's
  fire-approach peak leads body-temp minimum by +11~12s, 3 seeds, OFF reactive +4s,
  gen0 none; but R² ≤ 0.08). round_6 decision: extend selection to ~160 gens and check
  the trend. rise → encoding suffices, keep selecting. plateau → encoding is the ceiling
  → open a new bloodline (cand1 sharpen perceptual encoding / cand2 add warmth-seeking
  target channel).
- **Config:** cold-threshold 22 + thermal_damage 0.3 (survivable, phase-locked cold
  trough; harmony 7/7, survival 24–43/48), adt=1 deterministic, 3 seed × ON/OFF.
- **Run dirs:** `data\runs\realism_detP2c_th22_5000{1,2,3}_{on,off}\`.
- **Status at handoff (UTC 2026-05-31 ~19:00):** 3 DONE at gen159 (50001_off, 50003_off,
  50003_on); 3 still running (50002_on ~154, 50001_on ~131, 50002_off ~112 slowest).
  John keeps them running in the background. If power-lost: adt=1 replay from gen0
  reproduces exactly (zero science loss). NOTE these checkpoints are triple_world =
  warm-start only; rely on replay, not bit-exact resume.

## 3. WHAT TO DO once all 6 reach gen159 (round_7 — execute in order)
1. **Confirm done:** each run's `evolution.csv` / console reaches gen159 (160 gens,
   gen0..159). Count finished = 6.
2. **Aggregate:** run `analysis\build_aggs_p2c.ps1` on Windows (it pulls gen140–159 +
   gen0/gen79 trace and builds the aggregate CSVs). Read the script header for exact
   input/output paths; point it at the 6 run dirs above.
3. **Rhythm re-test:** run `analysis\phase_harmonic.py` (fire-approach channel,
   protagonist-only) for gen0 / gen79 / gen~150, ON vs OFF, all 3 seeds. Output =
   phase-lead (s), R², amplitude per (seed, ON/OFF, gen). `phase_lead.py` is the
   simpler phase-lead-only sanity check.
4. **Verdict (the standard ruler = phase-LEAD pre-emption, not same-period correlation):**
   compare gen79 → gen~150 on the fire-approach channel:
   - **RISE** (R²/amplitude/lead grows, ON stays ahead of OFF, lead stays negative=leads
     the temp-min) → encoding is sufficient → recommend: continue selection (maybe push
     further / more seeds). NO new bloodline.
   - **PLATEAU** (no growth gen79→150) → encoding is the ceiling → recommend: open a NEW
     BLOODLINE — cand1 sharpen perceptual encoding, cand2 add a warmth-seeking target
     channel. Changing input dims = new bloodline (red line), so this is a structural step.
5. **Write `round_7__<UTCyyyymmdd-HHMMSS>.md` into `routes/protagonist_ecology/inbox/`**
   per the inbox/outbox protocol: one-line what+params, real metrics vs gen79 (from CSV,
   verified), rise/plateau verdict, 2–3 candidate next steps. Then poll `outbox/` ≤10min
   for the decision; if empty, self-decide and log "本轮自决策(决策者超时)", continue.
6. **Update** `docx/PROGRESS.md` + `docx/STATUS.md`; **commit locally** (prefix
   `protagonist_ecology:`, do NOT push). Then immediately delete the gen140–159 raw
   traces you pulled (keep only aggregate CSVs) per OPS_CONVENTIONS rule 1.

## 4. Red lines (do not cross)
- Rhythm must NOT be forced via reward (Goodhart). Judge with phase-lead pre-emption.
- Changing input dims = a new bloodline (structural; smoke-test first).
- C7 (no-extinction) is a floor; harmony must stay 7/7; adt=1 determinism preserved;
  fitness is appendix-only evidence.
- Production exe never overwritten; deep_protagonist never touched; commits local-only.
- env-perc is always ON; OFF is only the negative control.

## 5. Optional follow-ups (only if asked)
- Make triple_world resume bit-identical (option B): instrument runtime RNGs / global
  id/lineage counters, binary-search first diverging gen, checkpoint the offender,
  re-run the bitcheck harness (see HANDOFF doc §3–4). ~hours; re-verify harmony 7/7.
- Phase-C prereq: synchronous batched GPU eval (see SYNC_BATCHED_EVAL_PHASE_C_PREREQ.md);
  do BEFORE Phase C; re-establishes baselines; needs working CUDA (nvcc at
  `G:\ai\nvidia\bin\nvcc.exe`, CUDA 12.4; the build_ninja_cuda cache had
  CUDA_COMPILER=NOTFOUND — configure a separate CUDA build dir, do not disturb the
  production cache while runs hold it).
