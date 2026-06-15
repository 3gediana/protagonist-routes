# OPS CONVENTIONS — protagonist_ecology (standing rules)

> Operational rules for any executor working this route. These are hard rules,
> not suggestions. Keep them satisfied without being reminded.

## 1. Immediate log/artifact cleanup (HARD RULE — do not let it pile up)

After EACH generation / batch finishes:
1. **Extract first** the high-signal results into the conclusion doc / PROGRESS /
   round_<N>.md: key metrics (score, harmony pass/fail, survival, the aggregate CSV
   paths, build/test RC). Do NOT keep raw logs "just in case" once the numbers are
   captured.
2. **Then delete immediately** that generation's/batch's raw intermediate artifacts:
   - console dumps (`tmp\*_out.txt`, `tmp\*_log.txt`),
   - per-gen trace files once aggregated,
   - throwaway / determinism-test run dirs (e.g. `data\runs\_resume_bitcheck*`),
   - scratch `tmp\*.txt`.
3. **Never batch up** raw logs and wait for a human to clean. During long polling
   loops, trim your own previous-iteration dumps as you go.

**Never delete** (whitelist): the active/running run's live logs + run dir, final
aggregate CSVs, checkpoints, and conclusion docs. Before deleting, re-check the target
is not inside a currently-running run; after deleting, re-check the relevant processes
are still alive.

Rationale: D: fills up fast (each detP2c run dir is multi-GB; bitcheck dirs were ~8 GB).
Disk pressure can break training; piled-up logs waste the user's review time.

## 2. Launch long runs detached (WMI), never from a returning script

Start long-running training via WMI `Win32_Process.Create` so the process detaches
from the SSH job object and survives session end / SSH disconnect. A launcher that
returns immediately will have its child process tree killed by Windows OpenSSH when the
session ends (detP2b survived only because its launcher used a blocking `Wait-Process`).

## 3. Build/binary safety

All experimental edits compile into the independent `-batched` exe
(`build_ninja_cuda\bin\neural-eco-protagonist-batched.exe`, via `tmp\bldbatched.bat`
which targets `neural-eco-protagonist-batched test_core` only). The production exe
`build_ninja_cuda\bin\neural-eco-protagonist.exe` is NEVER relinked/overwritten while
runs hold it. deep_protagonist is never touched. Commits are local only (never pushed).

## 4. checkpoint resume — current reality

Validation bug fixed (validates against checkpoint-recorded evolved population_count,
not config initial slot.count). Non-triple_world resume is bit-identical. triple_world
resume (e.g. detP2c) restores pools+curriculum but is a **warm-start, NOT bit-identical**
— exact reproducibility relies on deterministic replay (adt=1, fixed seed) from gen0.
See `HANDOFF_checkpoint_resume_and_detP2c.md` for the full story and the option-B plan.
