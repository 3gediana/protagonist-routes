"""protagonist_memory_ablation.py

Smoke-sized memory ablation eval for the protagonist route.

Runs five short training configurations back-to-back and prints a side-by-side
summary so we can sanity-check that the four memory ablation switches
(spatial / episodic / social / goal) actually move behavior:

  - baseline       : everything on (full Memory V2 + DNC)
  - no_spatial     : SpatialMemoryBank writes disabled
  - no_episodic    : EpisodicMemoryBank writes disabled
  - no_social      : SocialMemoryBank writes disabled
  - no_goal        : rule-based GoalManager disabled (goal stays at default)

We deliberately use the existing smoke config (3 generations, ~200 ticks per
episode, 8 protagonists) so the whole sweep finishes in a few minutes. The
goal here is *infrastructure verification*, not a final scientific result -
i.e. proving the switches are wired and that some metric responds to each.

Usage::

    python3 scripts/protagonist_memory_ablation.py

Output: prints a markdown table to stdout. Each variant's run directory is
preserved under data/runs/protagonist_memory_ablation/ so the trace can be
inspected by hand later.
"""

from __future__ import annotations

import csv
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
BASE_CONFIG = REPO_ROOT / "configs" / "protagonist_ecology_train_smoke.toml"
EXE = REPO_ROOT / "build" / "bin" / "Release" / "neural-eco-protagonist.exe"
OUT_ROOT = REPO_ROOT / "data" / "runs" / "protagonist_memory_ablation"


VARIANTS = [
    # name, dict of population.protagonist toggle overrides
    ("baseline",     {}),
    ("no_spatial",   {"spatial_memory_writes_enabled": False}),
    ("no_episodic",  {"episodic_memory_writes_enabled": False}),
    ("no_social",    {"social_memory_writes_enabled": False}),
    ("no_goal",      {"goal_manager_enabled": False}),
]


def make_variant_config(variant_name: str, overrides: dict, runs_dir: Path) -> Path:
    """Copy the smoke config, set runs_dir + ablation toggles, return new path.

    We do simple, line-based TOML editing here to avoid taking a hard
    dependency on tomlkit / tomli_w in this throwaway script.
    """
    src = BASE_CONFIG.read_text(encoding="utf-8").splitlines()
    out_lines: list[str] = []
    in_protagonist_section = False
    seen_keys: set[str] = set()

    for line in src:
        stripped = line.strip()

        # Replace runs_dir under [output]. Use native (Windows) path because
        # the exe is a Windows binary even when the script runs under bash.
        if stripped.startswith("runs_dir") and "=" in stripped:
            out_lines.append(f'runs_dir = "{to_native_path(runs_dir)}"')
            continue

        # Track [population.protagonist] section so we can append toggles.
        if stripped.startswith("[") and stripped.endswith("]"):
            # Flush ablation overrides at the start of *next* section.
            if in_protagonist_section:
                for k, v in overrides.items():
                    if k not in seen_keys:
                        out_lines.append(f"{k} = {str(v).lower()}")
                in_protagonist_section = False
            in_protagonist_section = (stripped == "[population.protagonist]")
            out_lines.append(line)
            continue

        # Override existing keys inside the protagonist section.
        if in_protagonist_section:
            m = re.match(r"\s*([a-zA-Z_][\w]*)\s*=", line)
            if m and m.group(1) in overrides:
                k = m.group(1)
                v = overrides[k]
                out_lines.append(f"{k} = {str(v).lower()}")
                seen_keys.add(k)
                continue

        out_lines.append(line)

    # If the protagonist section was the last in the file, flush any remaining
    # toggles now.
    if in_protagonist_section:
        for k, v in overrides.items():
            if k not in seen_keys:
                out_lines.append(f"{k} = {str(v).lower()}")

    target = runs_dir / f"{variant_name}.toml"
    runs_dir.mkdir(parents=True, exist_ok=True)
    target.write_text("\n".join(out_lines) + "\n", encoding="utf-8")
    return target


def latest_run_dir(parent: Path) -> Path | None:
    if not parent.exists():
        return None
    candidates = sorted([p for p in parent.iterdir() if p.is_dir()],
                        key=lambda p: p.name)
    return candidates[-1] if candidates else None


def parse_fitness(run_dir: Path) -> dict:
    fpath = run_dir / "fitness.csv"
    if not fpath.exists():
        return {}
    rows = list(csv.DictReader(fpath.open()))
    if not rows:
        return {}
    last = rows[-1]
    return {
        "gen": int(last["generation"]),
        "min": float(last["min_fitness"]),
        "avg": float(last["avg_fitness"]),
        "best": float(last["best_fitness"]),
    }


def parse_event_trace_counts(run_dir: Path) -> dict:
    """Walk every gen/episode event_trace.jsonl and count protagonist-route
    structured events. Returns a flat dict so the summary table can include
    these alongside fitness + behaviors.
    """
    counts = {
        "chop_started": 0, "chop_completed": 0, "chop_abandoned": 0,
        "build_started": 0, "build_completed": 0,
        "deposits": 0,
        "credit_assigned_build": 0, "credit_assigned_hunt": 0,
        "credit_total_reward": 0.0,
    }
    paths = list(run_dir.glob("trace/generation_*/episode_*/event_trace.jsonl"))
    if not paths:
        return counts
    import json
    for p in paths:
        try:
            with p.open(encoding="utf-8") as f:
                for line in f:
                    line = line.strip()
                    if not line:
                        continue
                    try:
                        evt = json.loads(line)
                    except json.JSONDecodeError:
                        continue
                    t = evt.get("type", "")
                    if t == "ChopStarted":
                        counts["chop_started"] += 1
                    elif t == "ChopCompleted":
                        counts["chop_completed"] += 1
                    elif t == "ChopAbandoned":
                        counts["chop_abandoned"] += 1
                    elif t == "BuildStarted":
                        counts["build_started"] += 1
                    elif t == "BuildCompleted":
                        counts["build_completed"] += 1
                    elif t == "WorksiteMaterialDeposited":
                        counts["deposits"] += 1
                    elif t == "CreditAssigned":
                        sk = evt.get("source_kind", -1)
                        if sk == 0:
                            counts["credit_assigned_build"] += 1
                        elif sk == 1:
                            counts["credit_assigned_hunt"] += 1
                        counts["credit_total_reward"] += float(evt.get("reward", 0.0))
        except OSError:
            continue
    return counts


def parse_map_elites_coverage(run_dir: Path) -> int:
    """Run trace_to_map_elites and return the unique-cell coverage count."""
    summary = run_dir / "map_elites_summary.txt"
    if not summary.exists():
        # Try to invoke the tool inline.
        try:
            subprocess.run(
                [sys.executable, str(REPO_ROOT / "scripts" / "trace_to_map_elites.py"),
                 str(run_dir), "--bins", "4"],
                cwd=str(REPO_ROOT),
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
                check=False,
            )
        except OSError:
            return 0
    if not summary.exists():
        return 0
    for line in summary.read_text(encoding="utf-8").splitlines():
        if line.startswith("observed_unique_cells"):
            try:
                return int(line.split("=")[1].strip())
            except (IndexError, ValueError):
                return 0
    return 0


def parse_behaviors(run_dir: Path) -> dict:
    fpath = run_dir / "protagonist_behaviors.csv"
    if not fpath.exists():
        return {}
    rows = list(csv.DictReader(fpath.open()))
    if not rows:
        return {}
    # Aggregate across generations (sum integer-ish columns).
    sum_keys = [
        "chop_successes", "deposits_total", "completed_worksites",
        "signal_emits", "signal_co_attendance",
        "hunt_successes", "throw_hits",
    ]
    out = {k: 0 for k in sum_keys}
    out["dnc_usage_mean_last"] = 0.0
    out["dnc_read_peak_last"] = 0.0
    for row in rows:
        for k in sum_keys:
            try:
                out[k] += int(float(row[k]))
            except (KeyError, ValueError):
                pass
    last = rows[-1]
    try:
        out["dnc_usage_mean_last"] = float(last.get("dnc_usage_mean_avg", 0.0))
        out["dnc_read_peak_last"] = float(last.get("dnc_read_peak_avg", 0.0))
    except ValueError:
        pass
    out["survived_last"] = int(float(last.get("survived_protagonists", 0)))
    return out


def to_native_path(p: Path) -> str:
    """Convert a Python Path (which may be MSYS / WSL POSIX) to a Windows
    path the cmake-built exe can resolve. Required because this script may
    run under bash on Windows where pathlib gives /mnt/d/... but the exe
    expects D:/..."""
    s = str(p)
    if s.startswith("/mnt/") and len(s) > 6 and s[6] == "/":
        drive = s[5].upper()
        return f"{drive}:/" + s[7:]
    return s


def run_variant(variant_name: str, config_path: Path) -> tuple[dict, dict]:
    print(f"[ablation] running {variant_name} ...", flush=True)
    env = os.environ.copy()
    # For subprocess.run we use the POSIX path that this script can see (WSL
    # mount or native). The exe itself only cares about the runs_dir string
    # inside the TOML, which is rewritten to native paths separately.
    # Executable path: pass the POSIX/native string Python sees so subprocess
    # can locate it. Argument paths: the spawned Windows exe parses its own
    # CLI strings - it cannot resolve /mnt/d/... so we convert to native.
    res = subprocess.run(
        [str(EXE), "--config", to_native_path(config_path)],
        cwd=str(REPO_ROOT),
        env=env,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        check=False,
    )
    if res.returncode != 0:
        sys.stdout.buffer.write(res.stdout[-2000:] if res.stdout else b"")
        raise SystemExit(f"[ablation] variant {variant_name} failed exit={res.returncode}")

    runs_dir = OUT_ROOT / variant_name
    run_dir = latest_run_dir(runs_dir)
    if run_dir is None:
        raise SystemExit(f"[ablation] no run dir produced for {variant_name}")
    fit = parse_fitness(run_dir)
    beh = parse_behaviors(run_dir)
    evt = parse_event_trace_counts(run_dir)
    map_e = parse_map_elites_coverage(run_dir)
    beh["_events"] = evt
    beh["_map_elites_cells"] = map_e
    print(f"[ablation] {variant_name} -> avg={fit.get('avg', 'na'):.1f} "
          f"best={fit.get('best', 'na'):.1f} chop={beh.get('chop_successes', 0)} "
          f"signal={beh.get('signal_emits', 0)} "
          f"credit_b={evt['credit_assigned_build']} credit_h={evt['credit_assigned_hunt']} "
          f"map_cells={map_e}", flush=True)
    return fit, beh


def main() -> int:
    if not EXE.exists():
        raise SystemExit(f"[ablation] executable not found: {EXE}")
    if OUT_ROOT.exists():
        shutil.rmtree(OUT_ROOT)
    OUT_ROOT.mkdir(parents=True)

    results: dict[str, tuple[dict, dict]] = {}
    for name, overrides in VARIANTS:
        runs_dir = OUT_ROOT / name
        cfg = make_variant_config(name, overrides, runs_dir)
        results[name] = run_variant(name, cfg)

    # Print markdown summary.
    print()
    print("## Memory Ablation Smoke Summary")
    print()
    print("| variant | avg fitness (last gen) | best | chop_compl | "
          "build_compl | deposits | credit_build | credit_hunt | "
          "credit_total_reward | signal_emits | hunt_succ | "
          "survived | map_cells | dnc_usage |")
    print("|---|---|---|---|---|---|---|---|---|---|---|---|---|---|")
    base_fit, base_beh = results["baseline"]
    for name, _ in VARIANTS:
        fit, beh = results[name]
        evt = beh.get("_events", {})
        avg = fit.get("avg", float("nan"))
        best = fit.get("best", float("nan"))
        delta = ""
        if name != "baseline" and "avg" in fit and "avg" in base_fit:
            d = avg - base_fit["avg"]
            delta = f" ({'+' if d >= 0 else ''}{d:.1f})"
        print(
            f"| {name} | {avg:.1f}{delta} | {best:.1f} | "
            f"{evt.get('chop_completed', 0)} | {evt.get('build_completed', 0)} | "
            f"{evt.get('deposits', 0)} | {evt.get('credit_assigned_build', 0)} | "
            f"{evt.get('credit_assigned_hunt', 0)} | "
            f"{evt.get('credit_total_reward', 0):.1f} | "
            f"{beh.get('signal_emits', 0)} | "
            f"{beh.get('hunt_successes', 0)} | {beh.get('survived_last', 0)} | "
            f"{beh.get('_map_elites_cells', 0)} | "
            f"{beh.get('dnc_usage_mean_last', 0):.3f} |"
        )
    print()
    print("> Note: smoke-sized run (3 generations, ~200 simulated seconds, 8 protagonists).")
    print("> The point is to verify the four ablation switches actually move metrics.")
    print("> Real Phase A scientific results will need full-length training, which is")
    print("> a separate, user-gated decision.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
