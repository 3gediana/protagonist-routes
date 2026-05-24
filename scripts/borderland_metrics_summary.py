"""borderland_metrics_summary.py

Internal audit tool. Reads `borderland_metrics.csv` (Layer 6 R3
stage 3 commit H output) from a single run_dir and prints a compact
markdown table summarizing:

  - per-archetype pool growth from gen 0 -> last gen
  - mean fitness drift (commit E/F mini-tick output, when triple_world
    + borderland_eval_period_gens are both on)
  - eval_count progression (how close we are to crossing
    borderland_min_continuity_gens for the first promote-to-main)
  - cumulative promoteBorderlandToMain events per archetype

Plus a global "is borderland evolving" verdict:

  - HEALTHY      : pool sizes stable + mean fitness rising or stable
                   + at least one archetype crossed the continuity gate
  - STAGNANT     : eval_count > 0 but mean fitness hasn't moved (eval
                   isn't actually a selection signal)
  - GATED        : eval_count >= continuity_gate but main_world still
                   empty (rescue or persist_dir issue)
  - INACTIVE     : every archetype's pool is empty (TripleWorld off
                   or sandbox->border promote never fired)

Usage::

    python3 scripts/borderland_metrics_summary.py <run_dir>

Where run_dir is e.g. data/runs/long_run_60gen_2d_curriculum/<timestamp>.
"""

from __future__ import annotations

import csv
import sys
from collections import defaultdict
from pathlib import Path

ARCHETYPE_NAME = {
    0: "protagonist",
    1: "large_herbivore",
    2: "small_forager",
    3: "apex_predator",
    4: "pack_hunter",
    5: "ambush_predator",
    6: "scavenger",
    7: "omnivore",
    8: "rabbit",
}


def to_float(s: str) -> float:
    try:
        return float(s)
    except (ValueError, TypeError):
        return 0.0


def to_int(s: str) -> int:
    try:
        return int(float(s))
    except (ValueError, TypeError):
        return 0


def main() -> int:
    if len(sys.argv) != 2:
        print(__doc__)
        return 2

    run_dir = Path(sys.argv[1]).resolve()
    csv_path = run_dir / "borderland_metrics.csv"
    if not csv_path.exists():
        print(f"ERR: {csv_path} not found", file=sys.stderr)
        return 2

    rows: list[dict[str, str]] = []
    with csv_path.open() as f:
        rows = list(csv.DictReader(f))

    if not rows:
        print(f"borderland_metrics.csv exists but contains no data rows — TripleWorld "
              "probably never fired (triple_world.enabled=false?)")
        return 0

    # Group by archetype
    by_arch: dict[int, list[dict[str, str]]] = defaultdict(list)
    for r in rows:
        a = to_int(r.get("archetype", "-1"))
        by_arch[a].append(r)

    print(f"\n# Borderland metrics · {run_dir.name}")
    print(f"\nTotal rows: {len(rows)}, archetypes seen: {sorted(by_arch.keys())}\n")

    print("| arch | name | pool first->last | mean fit first->last | eval count | promoted total | culled total |")
    print("|---|---|---|---|---|---|---|")

    any_evolving = False
    any_gated = False
    for a in sorted(by_arch.keys()):
        arch_rows = sorted(by_arch[a], key=lambda r: to_int(r["generation"]))
        first, last = arch_rows[0], arch_rows[-1]
        first_pool = to_int(first["pool_size"])
        last_pool = to_int(last["pool_size"])
        first_mean = to_float(first["mean_fitness"])
        last_mean = to_float(last["mean_fitness"])
        last_eval_count = to_int(last["borderland_eval_count"])
        promoted_total = sum(to_int(r["promoted_to_main"]) for r in arch_rows)
        culled_total = sum(to_int(r["culled"]) for r in arch_rows)

        name = ARCHETYPE_NAME.get(a, f"archetype_{a}")
        print(f"| {a} | {name} | {first_pool}->{last_pool} | "
              f"{first_mean:.1f}->{last_mean:.1f} | {last_eval_count} | "
              f"{promoted_total} | {culled_total} |")

        if last_eval_count > 0 and abs(last_mean - first_mean) > 1.0:
            any_evolving = True
        if last_eval_count > 0 and promoted_total == 0:
            any_gated = True

    # Verdict
    print()
    if not by_arch:
        print("Verdict: **INACTIVE** — borderland never populated.")
    elif any_evolving and not any_gated:
        print("Verdict: **HEALTHY** — at least one archetype evolving + promotes firing.")
    elif any_evolving and any_gated:
        print("Verdict: **HEALTHY (partial)** — eval working but at least one archetype "
              "blocked by continuity gate.")
    elif any_gated:
        print("Verdict: **GATED** — eval_count > 0 but mean fitness flat AND "
              "no promoteBorderlandToMain. Check borderland_min_continuity_gens "
              "vs borderland_to_main_period_gens alignment.")
    else:
        print("Verdict: **STAGNANT** — eval running but no signal change. Likely "
              "borderland_eval_period_gens=0 (no eval) or eval_fn is degenerate.")

    return 0


if __name__ == "__main__":
    sys.exit(main())
