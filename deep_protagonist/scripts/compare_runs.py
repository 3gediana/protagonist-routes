#!/usr/bin/env python3
"""Compare multiple PPO training runs side-by-side.

Reads jsonl metrics files and prints a summary table with key indicators
(mean reward, deaths, per-component means) for ALL episodes and LAST 20.

Usage:
    python3 scripts/compare_runs.py runs/run1.jsonl runs/run2.jsonl ...
    python3 scripts/compare_runs.py runs/*.jsonl --last 30
"""
import argparse
import json
import os
import sys
from typing import List, Dict


def died(ep: dict) -> bool:
    return ep.get("r_death", 0) <= -50


def load_jsonl(path: str) -> List[dict]:
    eps = []
    with open(path, "r", encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            eps.append(json.loads(line))
    return eps


def stats(eps: List[dict]) -> Dict[str, float]:
    if not eps:
        return {}
    n = len(eps)
    survivors = [e for e in eps if not died(e)]
    return {
        "n": n,
        "mean": sum(e["reward"] for e in eps) / n,
        "deaths": sum(1 for e in eps if died(e)),
        "death_pct": 100 * sum(1 for e in eps if died(e)) / n,
        "survivor_mean": (sum(e["reward"] for e in survivors) / len(survivors)
                          if survivors else 0.0),
        "r_alive": sum(e["r_alive"] for e in eps) / n,
        "r_food":  sum(e["r_food"]  for e in eps) / n,
        "r_water": sum(e["r_water"] for e in eps) / n,
        "r_collect": sum(e["r_collect"] for e in eps) / n,
        "r_shelter": sum(e["r_shelter"] for e in eps) / n,
        "r_craft":   sum(e["r_craft"]   for e in eps) / n,
        "r_milestone": sum(e["r_milestone"] for e in eps) / n,
        "r_bites": sum(e["r_bites"] for e in eps) / n,
        "r_vital": sum(e["r_vital"] for e in eps) / n,
        "r_kills": sum(e["r_kills"] for e in eps) / n,
    }


def print_table(headers: List[str], rows: List[List[str]]):
    widths = [max(len(h), max((len(r[i]) for r in rows), default=0))
              for i, h in enumerate(headers)]
    fmt = "  ".join(f"{{:<{w}}}" for w in widths)
    print(fmt.format(*headers))
    print(fmt.format(*("-" * w for w in widths)))
    for r in rows:
        print(fmt.format(*r))


def main():
    p = argparse.ArgumentParser()
    p.add_argument("paths", nargs="+", help="jsonl metric files")
    p.add_argument("--last", type=int, default=20,
                   help="last N episodes window (default 20)")
    args = p.parse_args()

    runs = []
    for path in args.paths:
        if not os.path.exists(path):
            print(f"skip missing: {path}", file=sys.stderr)
            continue
        eps = load_jsonl(path)
        if not eps:
            print(f"empty: {path}", file=sys.stderr)
            continue
        runs.append((os.path.basename(path), eps))

    if not runs:
        print("no runs to compare")
        return

    # Section 1: Overall summary
    print("=== ALL EPISODES ===")
    rows = []
    for name, eps in runs:
        s = stats(eps)
        rows.append([
            name,
            f"{s['n']}",
            f"{s['mean']:+.2f}",
            f"{s['death_pct']:.1f}%",
            f"{s['survivor_mean']:+.2f}",
            f"{s['r_water']:+.2f}",
            f"{s['r_food']:+.2f}",
            f"{s['r_collect']:+.2f}",
            f"{s['r_shelter']:+.2f}",
            f"{s['r_milestone']:+.2f}",
            f"{s['r_bites']:+.2f}",
        ])
    print_table(
        ["run", "ep", "mean", "death%", "surv", "water", "food",
         "collect", "shelter", "miles", "bites"],
        rows,
    )
    print()

    # Section 2: Last N
    print(f"=== LAST {args.last} ===")
    rows = []
    for name, eps in runs:
        s = stats(eps[-args.last:])
        rows.append([
            name,
            f"{s['n']}",
            f"{s['mean']:+.2f}",
            f"{s['death_pct']:.1f}%",
            f"{s['survivor_mean']:+.2f}",
            f"{s['r_water']:+.2f}",
            f"{s['r_food']:+.2f}",
            f"{s['r_collect']:+.2f}",
            f"{s['r_shelter']:+.2f}",
            f"{s['r_milestone']:+.2f}",
            f"{s['r_bites']:+.2f}",
        ])
    print_table(
        ["run", "ep", "mean", "death%", "surv", "water", "food",
         "collect", "shelter", "miles", "bites"],
        rows,
    )
    print()

    # Section 3: Verdict (only first run gets the symbol)
    print("=== VERDICT (last N reward mean) ===")
    print("  red  -inf~-37 = below wander baseline (broken)")
    print("  amber -37~+0  = barely surviving")
    print("  yellow +0~+50 = learning vitals + collect (current S4 plateau)")
    print("  orange +50~+100 = breaking into mid-game shelter/build/milestone")
    print("  green +100+   = crushing the game")
    for name, eps in runs:
        s = stats(eps[-args.last:])
        m = s["mean"]
        if m < -37:    sym = "[RED]"
        elif m < 0:    sym = "[AMBER]"
        elif m < 50:   sym = "[YELLOW]"
        elif m < 100:  sym = "[ORANGE]"
        else:          sym = "[GREEN]"
        print(f"  {sym:9s} {name:40s} mean={m:+6.2f}")


if __name__ == "__main__":
    main()
