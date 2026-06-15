"""d046_compare.py - compare ppo_d043_2M (clip-bug-contaminated) vs
ppo_d046_2M (post D-045 clip fix) head-to-head.

Output: per-window means of total reward / r_bites / death rate /
score / unlocks-coverage / r_milestone / r_potential. Window =
20 episodes by default; also reports "first time score breaks 0.20"
which is the D-043 success criterion.

Usage:
    python3 scripts/d046_compare.py
    python3 scripts/d046_compare.py --window 50
"""
import argparse
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent / "runs"
RUNS = [
    ("d043 (clip leaked)", ROOT / "ppo_d043_2M.jsonl"),
    ("d046 (clip fixed)",  ROOT / "ppo_d046_2M.jsonl"),
]


def load(path):
    if not path.exists():
        return []
    return [json.loads(line) for line in path.read_text().splitlines()
            if line.strip()]


def death_rate(eps):
    if not eps:
        return 0.0
    return sum(1 for e in eps if e.get("r_death", 0) < -0.5) / len(eps)


def coverage(eps):
    if not eps:
        return 0.0
    union = [0] * 10
    for e in eps:
        for i, v in enumerate(e.get("unlocks", [0] * 10)):
            if v:
                union[i] = 1
    return sum(union) / 10.0


def mean(eps, key):
    if not eps:
        return 0.0
    return sum(e.get(key, 0) for e in eps) / len(eps)


def report(label, eps, window):
    if not eps:
        print(f"  ({label}: no data yet)")
        return
    n = len(eps)
    last = eps[-window:] if n >= window else eps
    print(f"  {label}: {n} ep total")
    print(f"    last {len(last):3d} ep: R={mean(last,'reward'):+8.2f} "
          f"score={mean(last,'score'):.3f} "
          f"r_bites={mean(last,'r_bites'):+7.2f} "
          f"r_milestone={mean(last,'r_milestone'):+6.1f} "
          f"r_potential={mean(last,'r_potential'):+6.1f} "
          f"deaths={death_rate(last)*100:5.1f}% "
          f"unlock_cov={coverage(last)*100:.0f}%")
    score_breaks = next((e["episode"] for e in eps
                         if e.get("score", 0) >= 0.20), None)
    print(f"    first ep with score>=0.20: "
          f"{score_breaks if score_breaks is not None else 'never'}")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--window", type=int, default=20)
    args = ap.parse_args()
    print(f"=== D-046 head-to-head (window={args.window}) ===")
    for label, path in RUNS:
        eps = load(path)
        report(label, eps, args.window)
    print()
    print("Verdict: ppo_d046 wins if r_bites less negative, "
          "death rate lower, score >= 0.20 sooner, AND R mean > -10.")


if __name__ == "__main__":
    main()
