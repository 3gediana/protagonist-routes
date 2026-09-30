#!/usr/bin/env python3
"""One-command verdict for the D5 (replication) + D6 (40M scaling) chain.

Run any time; works on partial data and says so. Produces:
  1. D5 replication table: deep-chain unlock rates (mine/axe/pickaxe/
     follower/cook) for base vs teleport at seeds 42/9, full run + last 25%.
     Verdict logic: tp counts as REPLICATED if it beats its same-seed base on
     >=2 deep chains in the last-25% window on both seeds.
  2. D6 scaling table: deep-chain rates at 40M vs the 10M D4 endpoint for the
     same flags (ascent check), plus survival sanity.
  3. Early-verdict gate demo on the D5 arms (first 10% ranking).

Usage:  python scripts/d5_d6_verdict.py
"""
import glob
import json
import os
import statistics as st

PROJ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DEEP = [("cook", 11), ("mine", 12), ("axe", 13), ("pickaxe", 14)]
FOLLOWER, SEED_I = 10, 7
BASE_FLAGS = "--survive-first-mask --afford-mask --nav-potential 0.75"
D4 = {"base": "d4_base_s7", "tp": "d4_tp_s7", "ss_tp": "d4_ss_tp_s7"}
D5 = {("base", 42): "d5_base_s42", ("tp", 42): "d5_tp_s42",
      ("base", 9): "d5_base_s9", ("tp", 9): "d5_tp_s9"}
D6 = {"tp": "d6_tp_s7_40m", "ss_tp": "d6_ss_tp_s7_40m"}


def rows(name):
    p = os.path.join(PROJ, "metrics", name + ".jsonl")
    if not os.path.exists(p):
        return []
    try:
        return [json.loads(l) for l in open(p)]
    except json.JSONDecodeError:
        return []


def unlock_table(name):
    """Full-run and last-25% unlock percentages per mechanism."""
    r = rows(name)
    if not r:
        return None
    out = {}
    for label, seg in (("full", r), ("last25", r[-max(1, len(r) // 4):])):
        n = len(seg)
        out[label] = {
            m: 100.0 * sum(1 for x in seg if x["unlocks"][i]) / n
            for m, i in DEEP + [("follower", FOLLOWER), ("seed", SEED_I)]
        }
    out["eps"] = len(r)
    out["score"] = st.mean(float(x["score"]) for x in r[-100:])
    out["cold"] = st.mean(float(x["deaths_cold"]) for x in r[-200:])
    return out


def main():
    print("=" * 74)
    print("D5 REPLICATION  (10M, seeds 42/9; tp replicated if it beats same-seed")
    print("base on >=2 deep chains in the last-25% window on BOTH seeds)")
    print("=" * 74)
    t = {k: unlock_table(v) for k, v in D5.items()}
    if not any(t.values()):
        print("  no D5 metrics yet")
    else:
        hdr = f"{'arm':14s} {'eps':>5} | last25%: " + \
              " ".join(f"{m:>8s}" for m, _ in DEEP) + f" {'follow':>7s} {'score':>6s} {'cold':>6s}"
        print(hdr)
        for (kind, seed), name in D5.items():
            u = t[(kind, seed)]
            if not u:
                print(f"{name:14s}    -- (no data)")
                continue
            cells = " ".join(f"{u['last25'][m]:8.1f}" for m, _ in DEEP)
            print(f"{name:14s} {u['eps']:5d} | {cells} "
                  f"{u['last25']['follower']:7.1f} {u['score']:6.3f} {u['cold']:6.1f}")
        for seed in (42, 9):
            b, p = t[("base", seed)], t[("tp", seed)]
            if not b or not p or b["eps"] < 800 or p["eps"] < 800:
                print(f"seed {seed}: insufficient data (need >=800 eps/arm)")
                continue
            wins = sum(1 for m, _ in DEEP
                       if p["last25"][m] > b["last25"][m])
            fw = p["last25"]["follower"] > b["last25"]["follower"]
            print(f"seed {seed}: tp beats base on {wins}/4 deep chains"
                  f" + follower:{'+win' if fw else 'no'}"
                  f"  -> {'REPLICATED' if wins >= 2 else 'NOT REPLICATED'}"
                  f"  (partial data, {b['eps']}/{p['eps']} eps)")

    print()
    print("=" * 74)
    print("D6 SCALING  (40M vs D4 10M endpoint, same seed 7)")
    print("=" * 74)
    d4t = {k: unlock_table(v) for k, v in D4.items()}
    d6t = {k: unlock_table(v) for k, v in D6.items()}
    for kind, name in D6.items():
        u6 = d6t[kind]
        if not u6:
            print(f"{name:18s} (not started / D6 pending)")
            continue
        ref = d4t.get("tp" if kind == "tp" else "ss_tp")
        cells = " ".join(f"{m}:{u6['last25'][m]:.1f}" for m, _ in DEEP)
        print(f"{name:18s} eps={u6['eps']:5d} last25% {cells} "
              f"follower:{u6['last25']['follower']:.1f} score:{u6['score']:.3f}")
        if ref:
            up = sum(1 for m, _ in DEEP
                     if u6["last25"][m] >= ref["last25"][m])
            print(f"    vs D4-10M endpoint: {up}/4 deep chains at-or-above; "
                  f"cook D4={ref['last25']['cook']:.1f} -> now={u6['last25']['cook']:.1f}")

    print()
    print("=" * 74)
    print("EARLY-VERDICT GATE (first 10% ranking, informational for D5)")
    print("=" * 74)
    os.system("python scripts/early_verdict.py --manifest night_manifest_d5_repl.txt")


if __name__ == "__main__":
    main()
