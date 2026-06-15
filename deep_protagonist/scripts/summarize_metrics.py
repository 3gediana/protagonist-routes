# /// script
# requires-python = ">=3.10"
# dependencies = []
# ///
#
# Summarize a runs/*.jsonl produced by deep_protagonist_train --metrics.
# No external dependencies (uses only stdlib). Run with:
#   uv run scripts/summarize_metrics.py runs/wander_baseline.jsonl
"""Print mean/median/p10/p90 for total_reward and each component, plus
   episode-length stats and death rate. This is the file you compare a
   PPO run against (PPO mean must beat wander mean to count as 'learning')."""

from __future__ import annotations

import argparse
import json
import math
import statistics
import sys
from pathlib import Path


COMPONENTS = (
    "reward",
    "r_alive", "r_food", "r_water", "r_kills", "r_bites",
    "r_death", "r_vital", "r_collect", "r_shelter", "r_craft",
    "r_milestone",
)


def load_episodes(path: Path) -> list[dict]:
    eps = []
    with path.open("r", encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            eps.append(json.loads(line))
    return eps


def quantile(xs: list[float], q: float) -> float:
    if not xs:
        return float("nan")
    xs = sorted(xs)
    pos = q * (len(xs) - 1)
    lo = math.floor(pos)
    hi = math.ceil(pos)
    if lo == hi:
        return xs[lo]
    frac = pos - lo
    return xs[lo] * (1 - frac) + xs[hi] * frac


def fmt(x: float) -> str:
    return f"{x:+9.2f}"


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("path", type=Path)
    args = ap.parse_args()

    eps = load_episodes(args.path)
    if not eps:
        print(f"No episodes in {args.path}", file=sys.stderr)
        return 1

    n = len(eps)
    deaths = sum(1 for e in eps if e.get("r_death", 0.0) < 0.0)
    steps = [e["steps"] for e in eps]

    print(f"file: {args.path}")
    print(f"episodes: {n}")
    print(f"deaths:   {deaths} ({100.0 * deaths / n:.1f}%)")
    print(f"steps:    mean={statistics.fmean(steps):7.1f}  "
          f"min={min(steps)}  max={max(steps)}  "
          f"med={statistics.median(steps):.0f}")
    print()
    print(f"{'component':<14}{'mean':>10}{'std':>10}{'p10':>10}{'med':>10}{'p90':>10}")
    print("-" * 64)
    for k in COMPONENTS:
        vals = [float(e.get(k, 0.0)) for e in eps]
        mean = statistics.fmean(vals)
        std  = statistics.pstdev(vals) if len(vals) > 1 else 0.0
        p10  = quantile(vals, 0.10)
        med  = statistics.median(vals)
        p90  = quantile(vals, 0.90)
        print(f"{k:<14}{fmt(mean):>10}{fmt(std):>10}"
              f"{fmt(p10):>10}{fmt(med):>10}{fmt(p90):>10}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
