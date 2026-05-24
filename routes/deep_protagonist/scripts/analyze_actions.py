#!/usr/bin/env python3
"""Analyse the act histogram in a metrics jsonl. Print per-episode and
overall percentage by discrete action category."""
import json
import sys

NAMES = [
    "eat", "drink", "attack", "collect", "place_shelter", "craft_spear",
    "sleep", "craft_grass_dress", "craft_fur_cloak", "wear_clothes",
    "place_blueprint", "cycle_building_type", "deposit_to_site",
    "plant_seed", "water_plot", "feed_tame",
]


def main(path: str, last_n: int = 0):
    eps = []
    with open(path, "r", encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            eps.append(json.loads(line))
    if not eps:
        print(f"{path}: no episodes")
        return
    if last_n > 0:
        eps = eps[-last_n:]

    total_steps = 0
    sum_act = [0] * 16
    sum_mv = 0.0
    sum_yw = 0.0
    has_act = "act" in eps[0]
    has_mvyw = "mv" in eps[0]

    for ep in eps:
        total_steps += ep["steps"]
        if has_act:
            for i, v in enumerate(ep["act"]):
                sum_act[i] += v
        if has_mvyw:
            sum_mv += ep.get("mv", 0.0) * ep["steps"]
            sum_yw += ep.get("yw", 0.0) * ep["steps"]

    print(f"=== {path} ({len(eps)} episodes, {total_steps} steps) ===")
    if not has_act:
        print("(no act field in this jsonl)")
        return

    rows = []
    for i in range(16):
        pct = 100.0 * sum_act[i] / total_steps if total_steps else 0
        rows.append((pct, NAMES[i], sum_act[i]))
    rows.sort(reverse=True)
    print(f"{'rank':>4}  {'action':<22} {'count':>10}  {'pct':>6}")
    cum = 0.0
    for r, (pct, name, count) in enumerate(rows, 1):
        cum += pct
        print(f"{r:>4}  {name:<22} {count:>10}  {pct:5.2f}%   cum {cum:5.1f}%")
    # noop = 1 - sum of triggered
    noop_count = total_steps - sum(sum_act)
    noop_pct = 100.0 * noop_count / total_steps
    print(f"      {'NOOP':<22} {noop_count:>10}  {noop_pct:5.2f}%")
    if has_mvyw:
        print(f"mean |move|={sum_mv/total_steps:.3f}  "
              f"mean |yaw|={sum_yw/total_steps:.3f} rad/s")


def trend(path: str):
    """Compare action distribution between first 25% and last 25% of episodes,
    showing which actions rose/fell as policy evolved."""
    eps = []
    with open(path, "r", encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            eps.append(json.loads(line))
    n = len(eps)
    if n < 8 or "act" not in eps[0]:
        print(f"need >= 8 episodes with act field; got {n}")
        return

    quarter = max(2, n // 4)
    first = eps[:quarter]
    last = eps[-quarter:]

    def agg(group):
        steps = sum(e["steps"] for e in group)
        sums = [0] * 16
        for e in group:
            for i, v in enumerate(e["act"]):
                sums[i] += v
        noop = steps - sum(sums)
        return steps, sums, noop

    s1, a1, n1 = agg(first)
    s2, a2, n2 = agg(last)

    print(f"=== TREND {path} (first {quarter} ep vs last {quarter} ep) ===")
    print(f"{'action':<22} {'first %':>8} {'last %':>8} {'delta':>8}  trend")
    rows = []
    for i in range(16):
        p1 = 100.0 * a1[i] / s1
        p2 = 100.0 * a2[i] / s2
        rows.append((NAMES[i], p1, p2, p2 - p1))
    # NOOP
    p1n = 100.0 * n1 / s1
    p2n = 100.0 * n2 / s2
    rows.append(("NOOP", p1n, p2n, p2n - p1n))
    rows.sort(key=lambda r: -abs(r[3]))
    for name, p1, p2, d in rows:
        bar = ""
        if d > 1:
            bar = "+" * min(20, int(d))
        elif d < -1:
            bar = "-" * min(20, int(-d))
        print(f"{name:<22} {p1:7.2f}% {p2:7.2f}% {d:+7.2f}%  {bar}")

    # Reward trend
    print()
    print(f"{'metric':<14} {'first':>8} {'last':>8} {'delta':>8}")
    for k in ("reward", "r_water", "r_food", "r_collect",
              "r_shelter", "r_milestone", "r_bites"):
        m1 = sum(e.get(k, 0) for e in first) / len(first)
        m2 = sum(e.get(k, 0) for e in last) / len(last)
        print(f"{k:<14} {m1:+7.2f}  {m2:+7.2f}  {m2 - m1:+7.2f}")


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("usage: analyze_actions.py <jsonl> [last_n|trend]")
        sys.exit(1)
    if len(sys.argv) > 2 and sys.argv[2] == "trend":
        trend(sys.argv[1])
    else:
        last_n = int(sys.argv[2]) if len(sys.argv) > 2 else 0
        main(sys.argv[1], last_n)
