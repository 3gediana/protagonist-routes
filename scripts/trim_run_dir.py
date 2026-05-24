"""trim_run_dir.py

Strip a protagonist_ecology run_dir down to its essential artifacts
(small CSVs + final genome) and delete the heavy stuff that's not
needed for harmony_check / borderland_metrics_summary / final eval.

Keeps:
  *.csv                     (~kB each: evolution / fitness /
                             protagonist_behaviors / species_metrics /
                             borderland_metrics + memory_ablation if
                             present)
  *.md                      (summary docs)
  best_genome_latest.bin    (~1.4 MB: rolling latest)
  best_genome_overall.bin   (~1.4 MB: best ever seen across run)
  metadata.json             (resume metadata)
  checkpoint_gen{N}/        (LAST one only; the rest are deleted)

Deletes:
  best_genome_gen{N}.bin    (~1.4 MB each, ~85 MB / run)
  trace/                    (~1-8 GB / run, biggest item)
  ALL but the last checkpoint_gen{N}/ (~30-200 MB each)

Usage::

    python3 scripts/trim_run_dir.py <run_dir>
    python3 scripts/trim_run_dir.py --all data/runs

The --all variant recurses one level under the given root (e.g.
data/runs) and trims every run_dir it finds. Operates IN-PLACE; no
confirmation prompt. Use --dry-run to preview deletions.

Designed to be safe to re-run: only files that match the delete
allow-list are removed, never the keep list.
"""

from __future__ import annotations

import re
import shutil
import sys
from pathlib import Path


BEST_GEN_RE = re.compile(r"^best_genome_gen\d+\.bin$")
CHECKPOINT_RE = re.compile(r"^checkpoint_gen(\d+)$")


def fmt_bytes(n: int) -> str:
    units = ["B", "KB", "MB", "GB", "TB"]
    f = float(n)
    i = 0
    while f >= 1024.0 and i < len(units) - 1:
        f /= 1024.0
        i += 1
    return f"{f:.1f}{units[i]}"


def dir_size(p: Path) -> int:
    total = 0
    for root, _, files in p.walk() if hasattr(p, "walk") else _legacy_walk(p):
        for f in files:
            try:
                total += (root / f).stat().st_size
            except OSError:
                pass
    return total


def _legacy_walk(p: Path):
    import os
    for root, dirs, files in os.walk(p):
        yield Path(root), dirs, files


def trim_one(run_dir: Path, dry_run: bool) -> tuple[int, list[str]]:
    """Return (bytes_freed, actions)."""
    if not run_dir.is_dir():
        return (0, [f"SKIP (not a directory): {run_dir}"])

    freed = 0
    actions: list[str] = []

    # 1) best_genome_gen{N}.bin
    for entry in run_dir.iterdir():
        if entry.is_file() and BEST_GEN_RE.match(entry.name):
            sz = entry.stat().st_size
            actions.append(f"  delete {entry.name} ({fmt_bytes(sz)})")
            if not dry_run:
                entry.unlink()
            freed += sz

    # 2) trace/
    trace_dir = run_dir / "trace"
    if trace_dir.is_dir():
        sz = dir_size(trace_dir)
        actions.append(f"  delete trace/ ({fmt_bytes(sz)})")
        if not dry_run:
            shutil.rmtree(trace_dir)
        freed += sz

    # 3) all but the last checkpoint_gen{N}/
    checkpoints = []
    for entry in run_dir.iterdir():
        if entry.is_dir():
            m = CHECKPOINT_RE.match(entry.name)
            if m:
                checkpoints.append((int(m.group(1)), entry))
    if checkpoints:
        checkpoints.sort(key=lambda t: t[0])
        # keep the last (highest gen), delete the rest
        for _, cp_dir in checkpoints[:-1]:
            sz = dir_size(cp_dir)
            actions.append(f"  delete {cp_dir.name}/ ({fmt_bytes(sz)})")
            if not dry_run:
                shutil.rmtree(cp_dir)
            freed += sz
        kept = checkpoints[-1][1].name
        actions.append(f"  keep {kept}/ (last checkpoint)")

    return freed, actions


def main(argv: list[str]) -> int:
    args = list(argv[1:])
    dry_run = False
    if "--dry-run" in args:
        dry_run = True
        args.remove("--dry-run")

    if len(args) == 0:
        print(__doc__)
        return 2

    targets: list[Path] = []
    if args[0] == "--all":
        if len(args) < 2:
            print("ERR: --all requires a root directory", file=sys.stderr)
            return 2
        root = Path(args[1]).resolve()
        if not root.is_dir():
            print(f"ERR: {root} not found", file=sys.stderr)
            return 2
        # Two levels: data/runs/<scenario>/<timestamp>
        for scen in root.iterdir():
            if scen.is_dir():
                for ts in scen.iterdir():
                    if ts.is_dir():
                        targets.append(ts)
    else:
        targets = [Path(a).resolve() for a in args]

    total_freed = 0
    for t in targets:
        print(f"\n# {t}")
        freed, actions = trim_one(t, dry_run)
        for a in actions:
            print(a)
        print(f"  -> {'would free' if dry_run else 'freed'}: {fmt_bytes(freed)}")
        total_freed += freed

    if len(targets) > 1:
        print(f"\nTOTAL {'would free' if dry_run else 'freed'}: {fmt_bytes(total_freed)} across {len(targets)} run_dirs")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
