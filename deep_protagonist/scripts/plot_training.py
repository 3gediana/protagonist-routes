# /// script
# requires-python = ">=3.10"
# dependencies = []
# ///
"""Plot training reward curve over episodes (ASCII art, no matplotlib).

Reads a runs/*.jsonl produced by deep_protagonist_train and prints:
  * sliding-window mean reward (window 20) over time
  * per-component breakdown (food / water / kills / bites / death / milestone)
  * compares to wander baseline (BASELINE.md numbers)

Usage:
  python3 scripts/plot_training.py runs/ppo_run1.jsonl
"""

from __future__ import annotations

import argparse
import json
import statistics
import sys
from pathlib import Path

# Wander baseline reference points (from docs/BASELINE.md, seed=42, 100 ep)
WANDER_REF = {
    "reward": -37.38,
    "death_rate": 0.29,
    "r_food": 2.04,
    "r_water": 2.15,
    "r_kills": 0.0,
    "r_milestone": 0.40,
}


def load(path: Path) -> list[dict]:
    out = []
    for line in path.read_text(encoding="utf-8").splitlines():
        line = line.strip()
        if not line:
            continue
        out.append(json.loads(line))
    return out


def sliding_mean(xs: list[float], w: int) -> list[float]:
    out: list[float] = []
    for i in range(len(xs)):
        lo = max(0, i - w + 1)
        out.append(statistics.fmean(xs[lo:i + 1]))
    return out


def ascii_line(values: list[float], width: int = 60, height: int = 14,
               baseline: float | None = None, title: str = "") -> str:
    if not values:
        return f"{title}: (empty)"
    lo = min(values)
    hi = max(values)
    if baseline is not None:
        lo = min(lo, baseline)
        hi = max(hi, baseline)
    if hi - lo < 1e-6:
        hi = lo + 1.0
    rng = hi - lo
    n = len(values)
    cols = []
    for c in range(width):
        idx = int(c * (n - 1) / max(1, width - 1)) if n > 1 else 0
        v = values[idx]
        row = int((1 - (v - lo) / rng) * (height - 1))
        cols.append(row)
    base_row = (None if baseline is None
                else int((1 - (baseline - lo) / rng) * (height - 1)))
    grid = [[' ' for _ in range(width)] for _ in range(height)]
    if base_row is not None and 0 <= base_row < height:
        for c in range(width):
            grid[base_row][c] = '-'
    for c, r in enumerate(cols):
        if 0 <= r < height:
            grid[r][c] = '*'
    out = [f"{title}  range [{lo:+.1f}, {hi:+.1f}]"]
    for row in grid:
        out.append("|" + "".join(row) + "|")
    out.append("+" + "-" * width + "+")
    return "\n".join(out)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("path", type=Path)
    ap.add_argument("--window", type=int, default=20,
                    help="sliding window size for smoothing")
    args = ap.parse_args()

    eps = load(args.path)
    if not eps:
        print(f"No episodes in {args.path}", file=sys.stderr)
        return 1

    rewards = [float(e["reward"]) for e in eps]
    smoothed = sliding_mean(rewards, args.window)
    n = len(eps)
    deaths = sum(1 for e in eps if float(e.get("r_death", 0.0)) < 0)
    last_w = rewards[-args.window:] if len(rewards) >= args.window else rewards
    recent_mean = statistics.fmean(last_w) if last_w else 0.0
    overall_mean = statistics.fmean(rewards)

    print(f"file:        {args.path}")
    print(f"episodes:    {n}")
    print(f"deaths:      {deaths}/{n} ({100.0 * deaths / n:.1f}%)")
    print(f"reward mean: {overall_mean:+.2f} overall  /  "
          f"{recent_mean:+.2f} last {len(last_w)}")
    print(f"wander ref:  {WANDER_REF['reward']:+.2f}  "
          f"(death rate {100.0 * WANDER_REF['death_rate']:.1f}%)")
    delta = recent_mean - WANDER_REF["reward"]
    if   recent_mean > WANDER_REF["reward"] + 137: tag = "🟩 +100 over wander, learning real progression"
    elif recent_mean > WANDER_REF["reward"] +  87: tag = "🟧  +50 over wander, learning food/water/avoid wolves"
    elif recent_mean >                          0: tag = "🟨   >0, learning to survive"
    elif recent_mean > WANDER_REF["reward"]       : tag = "🟦  beats wander, but still negative"
    else:                                          tag = "🟥  behind wander - check for bugs"
    print(f"verdict:     {tag}  (delta {delta:+.1f})")
    print()
    print(ascii_line(smoothed,
                     baseline=WANDER_REF["reward"],
                     title=f"reward sliding-mean(window={args.window})"))
    print()
    # per-component recent means
    print("recent component means (last", len(last_w), "ep):")
    for k, ref_key in (
        ("r_food",     "r_food"),
        ("r_water",    "r_water"),
        ("r_kills",    "r_kills"),
        ("r_bites",    None),
        ("r_collect",  None),
        ("r_shelter",  None),
        ("r_craft",    None),
        ("r_milestone","r_milestone"),
    ):
        vals = [float(e.get(k, 0.0)) for e in eps[-args.window:]]
        m = statistics.fmean(vals) if vals else 0.0
        ref = WANDER_REF.get(ref_key) if ref_key else None
        ref_s = f"  (wander {ref:+.2f})" if ref is not None else ""
        print(f"  {k:<12} {m:+8.2f}{ref_s}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
