# -*- coding: utf-8 -*-
"""Strict per-(protag x ep) normalized comparison: v8/v13/v15/v16 last 10 gens.

每行 protagonist_behaviors.csv 是 1 个 generation 的全 episodes 总和。
归一化分母 = total_protagonists（已经包含 ep 次数）。
"""
import csv
import sys
from pathlib import Path

ROOT = Path("/mnt/d/claude-code/c++/data/runs")

RUNS = {
    "v8":  ROOT / "protagonist_train_a3_v8" / "20260522-142545",
    "v13": ROOT / "v13_hrl"                 / "20260523-013533",
    "v15": ROOT / "v15_qd_real"             / "20260523-120522",
    "v16": ROOT / "v16_fair"                / "20260523-124640",
    "v17": ROOT / "v17_hunt_bd"             / "20260523-135327",
}

LAST_N = 10  # 末尾 10 代

def load_behaviors(run_dir: Path):
    p = run_dir / "protagonist_behaviors.csv"
    if not p.exists():
        return None
    with p.open("r", newline="") as f:
        return list(csv.DictReader(f))

def load_fitness(run_dir: Path):
    p = run_dir / "fitness.csv"
    if not p.exists():
        return None
    with p.open("r", newline="") as f:
        return list(csv.DictReader(f))

def safe_float(v, default=0.0):
    try:
        return float(v)
    except (TypeError, ValueError):
        return default

def aggregate(rows, last_n):
    if not rows:
        return None
    rows = rows[-last_n:]
    total_prot = sum(safe_float(r["total_protagonists"]) for r in rows)
    if total_prot <= 0:
        return None
    survived = sum(safe_float(r["survived_protagonists"]) for r in rows)
    chops    = sum(safe_float(r["chop_successes"]) for r in rows)
    crafts   = sum(safe_float(r["craft_successes"]) for r in rows)
    works    = sum(safe_float(r.get("worksite_completion_events", 0)) for r in rows)
    throws   = sum(safe_float(r["throw_attempts"]) for r in rows)
    hunts    = sum(safe_float(r["hunt_successes"]) for r in rows)
    coop_sec = sum(safe_float(r.get("cooperative_co_presence_seconds", 0)) for r in rows)
    return {
        "alive_rate":   survived / total_prot,
        "chops":        chops / total_prot,
        "works":        works / total_prot,
        "crafts":       crafts / total_prot,
        "throws":       throws / total_prot,
        "hunts":        hunts / total_prot,
        "coop_per_prot": coop_sec / total_prot,
        "n_gens":       len(rows),
        "total_prot":   int(total_prot),
    }

def aggregate_fitness(rows, last_n):
    if not rows:
        return None
    rows = rows[-last_n:]
    n = len(rows)
    avg_fit = sum(safe_float(r["avg_fitness"]) for r in rows) / n
    best    = sum(safe_float(r["best_fitness"]) for r in rows) / n
    me_cov  = sum(safe_float(r.get("me_coverage", 0)) for r in rows) / n
    qd      = sum(safe_float(r.get("me_qd_score", 0)) for r in rows) / n
    return {"avg_fitness": avg_fit, "best_fitness": best, "me_coverage": me_cov, "qd_score": qd}

def main():
    print("=" * 92)
    print(f"Last {LAST_N} gen per-(protag x ep) normalized + fitness avg")
    print("=" * 92)

    results = {}
    for name, path in RUNS.items():
        behaviors = load_behaviors(path)
        fitness   = load_fitness(path)
        b_agg = aggregate(behaviors, LAST_N) if behaviors else None
        f_agg = aggregate_fitness(fitness, LAST_N) if fitness else None
        if b_agg is None and f_agg is None:
            print(f"[skip] {name}: no data at {path}")
            continue
        results[name] = {**(b_agg or {}), **(f_agg or {})}
        results[name]["path"] = str(path)

    # 行为指标
    metrics = [
        ("alive_rate",    "{:.3f}", 1.0),
        ("chops",         "{:.3f}", 1.0),
        ("works",         "{:.4f}", 1.0),
        ("crafts",        "{:.4f}", 1.0),
        ("throws",        "{:.2f}", 1.0),
        ("hunts",         "{:.3f}", 1.0),
        ("coop_per_prot", "{:.2f}", 1.0),
        ("avg_fitness",   "{:.0f}", 1.0),
        ("best_fitness",  "{:.0f}", 1.0),
        ("me_coverage",   "{:.3f}", 1.0),
        ("qd_score",      "{:.0f}", 1.0),
    ]
    cols = ["v8", "v13", "v15", "v16", "v17"]

    print(f"\n{'metric':<16}" + "".join(f"{c:>14}" for c in cols))
    print("-" * 92)
    for key, fmt, _ in metrics:
        row = f"{key:<16}"
        for c in cols:
            if c in results and key in results[c]:
                row += f"{fmt.format(results[c][key]):>14}"
            else:
                row += f"{'-':>14}"
        print(row)

    # v17 vs v16 ratio (focus: hunts recovery)
    if "v16" in results and "v17" in results:
        print(f"\n{'v17 vs v16 (3D BD impact)':<28}")
        for key, fmt, _ in metrics:
            if key in results["v16"] and key in results["v17"]:
                v16v = results["v16"][key]
                v17v = results["v17"][key]
                if abs(v16v) < 1e-9:
                    ratio = "inf" if v17v > 0 else "0"
                else:
                    ratio = f"{v17v / v16v:.2f}x"
                print(f"  {key:<14}: v16={fmt.format(v16v):<10} v17={fmt.format(v17v):<10} -> {ratio}")

    # 每跑总 protag-eps（确认公平 scale）
    print(f"\n{'total_prot (last 10 gen)':<28}")
    for c in cols:
        if c in results:
            tp = results[c].get("total_prot", 0)
            ng = results[c].get("n_gens", 0)
            print(f"  {c}: {tp:>5} = {tp//ng if ng else 0}/gen × {ng} gen ({results[c]['path']})")

if __name__ == "__main__":
    main()
