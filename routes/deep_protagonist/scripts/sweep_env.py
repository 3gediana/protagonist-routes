#!/usr/bin/env python3
"""Sweep one EnvConfig parameter across multiple values, run a short PPO
smoke for each, then print a side-by-side comparison.

Why: D-040 burned 47 minutes recompiling for each thirst_decay value.
With this script + the new CLI flags we can launch 5 sweep configs from
a single shell call and let them run unattended.

Examples:
    # Sweep thirst_decay 4/6/8/10/12 with 100k steps each
    python3 scripts/sweep_env.py --param thirst-decay --values 4 6 8 10 12 --steps 100000

    # Sweep trigger_cost
    python3 scripts/sweep_env.py --param trigger-cost --values 0.0005 0.001 0.002 0.005 --steps 100000
"""
import argparse
import os
import shutil
import subprocess
import sys
import time
from pathlib import Path

ROUTE = Path(__file__).resolve().parent.parent  # routes/deep_protagonist
EXE = ROUTE / "build" / "bin" / "deep_protagonist_train.exe"


def run_one(metrics_path: Path, param_flag: str, value: str,
            steps: int, seed: int) -> int:
    """Launch one training run and wait for it. Returns the process exit code."""
    cmd = [
        str(EXE),
        "--policy", "ppo",
        "--steps", str(steps),
        "--seed", str(seed),
        "--metrics", str(metrics_path),
        f"--{param_flag}", str(value),
    ]
    print(f"\n>>> RUN {param_flag}={value} ({steps} steps)")
    print(" ".join(cmd))
    t0 = time.time()
    proc = subprocess.run(cmd, capture_output=True, text=True)
    dt = time.time() - t0
    print(f"    exit={proc.returncode} wall={dt:.1f}s")
    if proc.returncode != 0:
        print("    STDERR (last 30):")
        for line in proc.stderr.splitlines()[-30:]:
            print(f"      {line}")
    return proc.returncode


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--param", required=True,
                   help="CLI flag without dashes (e.g. 'thirst-decay')")
    p.add_argument("--values", required=True, nargs="+",
                   help="space-separated values to sweep")
    p.add_argument("--steps", type=int, default=100_000,
                   help="steps per run (default 100k)")
    p.add_argument("--seed", type=int, default=42)
    p.add_argument("--out", default="runs/sweep",
                   help="output dir for jsonl files (default runs/sweep)")
    p.add_argument("--keep-old", action="store_true",
                   help="don't delete existing files in out dir")
    args = p.parse_args()

    if not EXE.exists():
        print(f"ERROR: missing binary {EXE}")
        sys.exit(1)

    out_dir = ROUTE / args.out
    if out_dir.exists() and not args.keep_old:
        shutil.rmtree(out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)

    metrics_files = []
    for v in args.values:
        # Filename like sweep_thirst-decay_8.jsonl
        safe = str(v).replace(".", "p")  # 0.001 -> 0p001
        path = out_dir / f"sweep_{args.param}_{safe}.jsonl"
        rc = run_one(path, args.param, v, args.steps, args.seed)
        if rc == 0 and path.exists():
            metrics_files.append(path)
        else:
            print(f"  (skipping {v} from comparison: failed or no metrics)")

    if not metrics_files:
        print("\nNo successful runs.")
        return

    print("\n" + "=" * 60)
    print("COMPARING SWEEP RESULTS")
    print("=" * 60)
    cmp_cmd = ["python3", str(ROUTE / "scripts" / "compare_runs.py"),
               *[str(p) for p in metrics_files]]
    subprocess.run(cmp_cmd, check=False)


if __name__ == "__main__":
    main()
