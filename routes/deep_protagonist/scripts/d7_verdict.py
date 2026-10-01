#!/usr/bin/env python3
"""One-command verdict for the D7 experience-injection chain (seed 7, 10M).

Arms (all share the teleport curriculum):
  d7_tp          control (teleport only)
  d7_tp_frc      + forced deep-chain practice (4 forced decisions per restore)
  d7_tp_inj      + random action injection (p=0.02)
  d7_tp_frc_inj  + both

Verdict logic (works on partial data, says so): an arm counts as a WIN vs the
control if it beats d7_tp on >=2 deep chains in the last-25% window (per-chain
win = strictly higher unlock rate). Also reports full-run rates, score, and
the injection/forced counters where relevant.

Usage:  python scripts/d7_verdict.py
"""
import json
import os
import statistics as st

PROJ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DEEP = [("cook", 11), ("mine", 12), ("axe", 13), ("pickaxe", 14)]
FOLLOWER, SEED_I = 10, 7
ARMS = ["d7_tp", "d7_tp_frc", "d7_tp_inj", "d7_tp_frc_inj"]


def rows(name):
    p = os.path.join(PROJ, "metrics", name + ".jsonl")
    if not os.path.exists(p):
        return []
    try:
        return [json.loads(l) for l in open(p)]
    except json.JSONDecodeError:
        return []


def unlock_table(name):
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


def fmt(t, label):
    if t is None:
        return f"{label:<16s}  (no data)"
    keys = ["cook", "mine", "axe", "pickaxe", "follower"]

    def cells(which):
        return " ".join(f"{t[which][k]:>5.1f}" for k in keys)

    return (f"{label:<16s} eps={t['eps']:>5} full [{cells('full')}]  "
            f"last25 [{cells('last25')}]  score={t['score']:.3f} "
            f"cold={t['cold']:.1f}")


def main():
    tabs = {a: unlock_table(a) for a in ARMS}
    any_data = any(tabs[a] for a in ARMS)
    total_eps = sum(t["eps"] for t in tabs.values() if t)
    print("=" * 78)
    print("D7 EXPERIENCE INJECTION (10M, seed 7; teleport curriculum everywhere)")
    print("  columns: cook mine axe pickaxe follower  (unlock % in window)")
    print("=" * 78)
    for a in ARMS:
        print(fmt(tabs[a], a) + ("" if tabs[a] else ""))
    if not any_data:
        print("\n(no D7 metrics yet)")
        return
    ctrl = tabs.get("d7_tp")
    if ctrl:
        print("-" * 78)
        for a in ARMS[1:]:
            t = tabs.get(a)
            if not t or not ctrl:
                continue
            frac = ctrl["eps"] and min(t["eps"], ctrl["eps"]) / max(ctrl["eps"], 1)
            wins = [k for k in ["cook", "mine", "axe", "pickaxe", "follower"]
                    if t["last25"][k] > ctrl["last25"][k]]
            ties = [k for k in ["cook", "mine", "axe", "pickaxe", "follower"]
                    if t["last25"][k] == ctrl["last25"][k] and t["last25"][k] > 0]
            partial = "  (partial data)" if t["eps"] < ctrl["eps"] * 0.98 else ""
            if len(wins) >= 2:
                v = "WIN"
            elif len(wins) == 1 and len(ties) >= 1:
                v = "LEAN WIN"
            elif len(wins) == 1:
                v = "LEAN/AMBIGUOUS"
            else:
                v = "NO GAIN"
            print(f"{a:>16s} vs control: last25 wins={wins or 'none'} "
                  f"ties@nonzero={ties or 'none'} -> {v}{partial}")
    print("=" * 78)
    print("context: d4_tp_s7 endpoint rates for reference")
    ref = unlock_table("d4_tp_s7")
    if ref:
        print(fmt(ref, "d4_tp_s7"))


if __name__ == "__main__":
    main()
