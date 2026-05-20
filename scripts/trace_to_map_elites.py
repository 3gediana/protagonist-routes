"""trace_to_map_elites.py

Stage 5 / P5 - turn an existing protagonist run's trace into a MAP-Elites
diversity report.

The C++ MapElitesArchive class is built into neural_eco::core, but the
runtime evolution loop does not yet feed it during training (that hook
would change reproduction strategy and is intentionally Phase B). This
script lets us *demonstrate* MAP-Elites coverage on already-recorded
trace data: read agent_trace.csv across all generations and episodes,
build a BehaviorCharacterization per (generation, agent_id), bin them
into a regular grid, and report coverage + per-cell champion fitness.

Output:

    <run_dir>/map_elites_report.csv     - one row per occupied cell
    <run_dir>/map_elites_summary.txt    - per-generation coverage line

This complements scripts/trace_to_world_model_dataset.py (which builds
the offline transition dataset) by giving a quality-diversity view of
the same trace.

Usage::

    python3 scripts/trace_to_map_elites.py <run_dir> [--bins N]

<run_dir> = data/runs/protagonist_<scenario>/<timestamp>
--bins N defaults to 4 per dimension.
"""

from __future__ import annotations

import argparse
import csv
import math
from collections import defaultdict
from pathlib import Path


# 8-dimensional behavior characterization that mirrors C++
# BehaviorCharacterization. Values are aggregates per (generation, agent_id).
# Each tuple = (column_in_agent_trace.csv, agg_kind).
# agg_kind: "last" -> take the final timestep value, "max" -> max over all
# steps, "sum" -> sum, "ratio" -> last value normalized by visited cells.
BC_DIMS = [
    ("x",                "last"),  # final_x
    ("y",                "last"),  # final_y
    ("visited_cells",    "last"),  # total_distance proxy
    ("energy",           "avg"),   # avg_energy
    ("visited_cells",    "ratio"), # visited_cells_ratio (saturation: visited/(steps*0.5))
    ("food_eaten",       "max"),   # food_eaten cumulative high water
    ("damage_dealt",     "max"),   # damage_dealt cumulative high water
    ("kills",            "max"),   # kills count proxy for signals_emitted
]

# Per-dim normalization range (matches what we expect a smoke-train to span;
# tuned for the 200x200 world + 200s episodes the smoke uses).
DIM_RANGES = [
    (0.0, 200.0),
    (0.0, 200.0),
    (0.0, 50.0),
    (0.0, 200.0),
    (0.0, 1.0),
    (0.0, 50.0),
    (0.0, 200.0),
    (0.0, 5.0),
]


def parse_float(s: str, default: float = 0.0) -> float:
    try:
        return float(s)
    except (ValueError, TypeError):
        return default


def parse_int(s: str, default: int = 0) -> int:
    try:
        return int(float(s))
    except (ValueError, TypeError):
        return default


def aggregate(rows: list[dict]) -> tuple[list[float], float]:
    """Return (8-dim BC, fitness)."""
    bc = []
    last = rows[-1]
    for col, kind in BC_DIMS:
        if kind == "last":
            bc.append(parse_float(last.get(col, "0")))
        elif kind == "max":
            vmax = -math.inf
            for r in rows:
                vmax = max(vmax, parse_float(r.get(col, "0")))
            bc.append(0.0 if vmax == -math.inf else vmax)
        elif kind == "sum":
            bc.append(sum(parse_float(r.get(col, "0")) for r in rows))
        elif kind == "avg":
            if not rows:
                bc.append(0.0)
            else:
                bc.append(sum(parse_float(r.get(col, "0")) for r in rows) / len(rows))
        elif kind == "ratio":
            steps = max(1, len(rows))
            visited = parse_float(last.get(col, "0"))
            bc.append(min(1.0, visited / (steps * 0.5)))
        else:
            bc.append(0.0)
    fitness = parse_float(last.get("fitness", "0"))
    return bc, fitness


def bin_index(value: float, lo: float, hi: float, bins: int) -> int:
    if hi <= lo:
        return 0
    norm = (value - lo) / (hi - lo)
    norm = max(0.0, min(1.0, norm))
    idx = int(math.floor(norm * bins))
    return min(idx, bins - 1)


def cell_key(bc: list[float], bins: int) -> tuple[int, ...]:
    return tuple(bin_index(v, lo, hi, bins) for v, (lo, hi) in zip(bc, DIM_RANGES))


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("run_dir", help="Protagonist run dir (contains trace/gen*/ep*)")
    ap.add_argument("--bins", type=int, default=4)
    args = ap.parse_args()

    run_dir = Path(args.run_dir).resolve()
    bins = args.bins

    paths = sorted(run_dir.glob("trace/generation_*/episode_*/agent_trace.csv"))
    if not paths:
        print(f"[trace2me] no agent_trace.csv under {run_dir}")
        return 1

    # cell -> (generation, agent_id, fitness, bc_vector)
    elites: dict[tuple, tuple[int, int, float, list[float]]] = {}
    per_gen_coverage: dict[int, set] = defaultdict(set)

    for path in paths:
        # gen number from path
        try:
            gen = int(path.parent.parent.name.split("_")[-1])
        except (IndexError, ValueError):
            gen = -1

        per_agent: dict[int, list[dict]] = defaultdict(list)
        with path.open(encoding="utf-8") as f:
            reader = csv.DictReader(f)
            for row in reader:
                aid = parse_int(row.get("agent_id", "0"))
                if parse_int(row.get("alive", "1")) == 0 and parse_int(row.get("step_index", "0")) == 0:
                    continue
                per_agent[aid].append(row)

        for aid, rows in per_agent.items():
            if not rows:
                continue
            bc, fit = aggregate(rows)
            key = cell_key(bc, bins)
            per_gen_coverage[gen].add(key)
            existing = elites.get(key)
            if existing is None or fit > existing[2]:
                elites[key] = (gen, aid, fit, bc)

    # Write per-cell report
    report_path = run_dir / "map_elites_report.csv"
    with report_path.open("w", encoding="utf-8", newline="") as f:
        w = csv.writer(f)
        header = (
            ["cell_key", "generation", "agent_id", "fitness"]
            + [f"bc_{c}_{k}" for c, k in BC_DIMS]
        )
        w.writerow(header)
        for key, (gen, aid, fit, bc) in sorted(elites.items()):
            w.writerow([
                "_".join(str(v) for v in key),
                gen, aid, f"{fit:.3f}",
                *[f"{v:.3f}" for v in bc],
            ])

    # Write per-generation coverage summary
    summary_path = run_dir / "map_elites_summary.txt"
    with summary_path.open("w", encoding="utf-8") as f:
        f.write(f"[map_elites_summary]\n")
        f.write(f"bins_per_dim = {bins}\n")
        f.write(f"behavior_dims = {len(BC_DIMS)}\n")
        f.write(f"max_possible_cells = {bins ** len(BC_DIMS)}\n")
        f.write(f"observed_unique_cells = {len(elites)}\n")
        f.write(f"\nPer-generation coverage:\n")
        for gen in sorted(per_gen_coverage):
            cov = len(per_gen_coverage[gen])
            f.write(f"  gen {gen:3d}: {cov} unique cells\n")
        if elites:
            sorted_by_fit = sorted(elites.values(), key=lambda x: -x[2])[:5]
            f.write(f"\nTop 5 elites by fitness:\n")
            for gen, aid, fit, bc in sorted_by_fit:
                f.write(f"  gen={gen} aid={aid} fit={fit:.2f} bc={[round(v,2) for v in bc]}\n")

    print(f"[trace2me] consumed {len(paths)} agent_trace files")
    print(f"[trace2me] wrote {len(elites)} elite cells to {report_path}")
    print(f"[trace2me] summary: {summary_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
