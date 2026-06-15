"""D-055 progression diagnostic.

Reads a jsonl produced by main_train.cpp (D-055 build onwards: extra
`stage / collect_total / explored_m / houses_built / sites_active`
fields) and answers the question:

> Why does PPO from D-053 never enter Mid stage?

For each episode we look at:
- whether the agent picked up resources
- how far it moved
- whether progress.stage ever flipped
- whether any construction site was placed

We split into "first 30 ep" vs "last 30 ep" because progress is a
cumulative cross-episode counter; if the policy keeps wandering it
will rise even without any new behavior.
"""
from __future__ import annotations
import json, statistics, sys
from pathlib import Path

ACTION_NAMES = [
    "eat", "drink", "attack", "pick_up", "drop",
    "craft_spear", "build_shelter", "wear_clothes", "sleep", "noop",
    "place_blueprint", "deposit", "switch_build", "plant", "water", "feed",
]
STAGE_NAMES = ["Early", "Mid", "Late"]

def load(p: Path) -> list[dict]:
    return [json.loads(l) for l in p.read_text().splitlines() if l.strip()]

def summarize(eps: list[dict], tag: str) -> None:
    if not eps:
        print(f"[{tag}] no episodes")
        return
    n = len(eps)
    print(f"\n== {tag} ({n} ep) ==")
    # blueprint-relevant per-episode stats
    def pct_unlock(idx: int) -> float:
        return 100.0 * sum(e["unlocks"][idx] for e in eps) / n
    def avg(field: str) -> float:
        return statistics.fmean(e[field] for e in eps)
    print(f"R mean={avg('reward'):+.2f} score={avg('score'):.3f}")
    print(f"death rate {100.0 * sum(1 for e in eps if e['r_death'] < 0) / n:.0f}%")
    # milestone slots from main_train.cpp EpisodeMilestones order
    labels = [
        "first_eat", "first_drink", "first_collect", "first_spear",
        "first_shelter", "first_clothing", "first_house",
        "first_farm", "first_tame", "first_kill",
    ]
    for i, label in enumerate(labels[: len(eps[0]["unlocks"])]):
        print(f"  ep_{label:<14}: {pct_unlock(i):4.0f}%")
    # action distribution
    total_acts = [sum(e["act"][i] for e in eps) for i in range(16)]
    total = sum(total_acts) or 1
    print("\n  action distribution (% of all triggered ticks):")
    pairs = sorted(enumerate(total_acts), key=lambda kv: -kv[1])
    for idx, cnt in pairs:
        pct = 100.0 * cnt / total
        if pct < 0.5:
            continue
        print(f"    {ACTION_NAMES[idx]:<18}: {pct:5.1f}%   ({cnt})")
    # progression diagnostics
    if "stage" not in eps[0]:
        print("  (no progression diagnostics in this jsonl)")
        return
    last = eps[-1]
    first = eps[0]
    print("\n  progression (cumulative across ep):")
    print(f"    stage end of run     : {STAGE_NAMES[last['stage']]} (start {STAGE_NAMES[first['stage']]})")
    print(f"    resources_collected  : {first['collect_total']} -> {last['collect_total']}")
    print(f"    explored_m           : {first['explored_m']:.0f} -> {last['explored_m']:.0f}")
    print(f"    houses_built         : {first['houses_built']} -> {last['houses_built']}")
    print(f"    sites_active (last)  : {last['sites_active']}")
    # stage-by-episode breakdown
    stages_seen = [e["stage"] for e in eps]
    flips = [(i, stages_seen[i - 1], stages_seen[i]) for i in range(1, len(eps)) if stages_seen[i] != stages_seen[i - 1]]
    if flips:
        for i, a, b in flips[:5]:
            print(f"    stage flip ep {i}: {STAGE_NAMES[a]} -> {STAGE_NAMES[b]}")
    else:
        print(f"    stage flips          : 0 (stayed {STAGE_NAMES[stages_seen[0]]})")

def main() -> int:
    if len(sys.argv) < 2:
        print("usage: d055_progression.py PATH.jsonl [--first N --last N]")
        return 2
    path = Path(sys.argv[1])
    eps = load(path)
    print(f"loaded {len(eps)} episodes from {path}")
    summarize(eps, "ALL")
    if len(eps) >= 60:
        summarize(eps[:30], "first 30")
        summarize(eps[-30:], "last 30")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
