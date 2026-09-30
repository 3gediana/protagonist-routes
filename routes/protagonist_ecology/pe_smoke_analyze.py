#!/usr/bin/env python3
"""PE smoke analyzer: A (base) vs B (+hunt BD axis) vs C (+two-stage flee +
coop prey + signal response) across seeds. Tail-window means + mechanism
rates. Anti-fake-breakthrough: qd alone is never progress evidence; every
verdict cites behavior counts (me_coverage, hunt/chop/throw rates,
signal_response, prey survival)."""
import csv
import glob
import os
import sys

TAIL = 10  # last-N-generation window


def latest_run(runs_dir):
    ds = sorted(glob.glob(os.path.join(runs_dir, "2*")))
    return ds[-1] if ds else None


def load(path):
    with open(path, newline="") as f:
        return list(csv.DictReader(f))


def fnum(row, key, default=0.0):
    try:
        return float(row.get(key, default) or default)
    except (TypeError, ValueError):
        return default


def tail_mean(rows, key):
    vals = [fnum(r, key) for r in rows[-TAIL:]]
    return sum(vals) / max(1, len(vals))


def analyze_run(run_dir):
    fit = load(os.path.join(run_dir, "fitness.csv"))
    beh = load(os.path.join(run_dir, "protagonist_behaviors.csv"))
    n = min(len(fit), len(beh))
    fit, beh = fit[-n:], beh[-n:]
    gens = [int(float(r["generation"])) for r in fit]
    cov_first = sum(fnum(r, "me_coverage") for r in fit[:5]) / max(1, min(5, n))
    cov_last = sum(fnum(r, "me_coverage") for r in fit[-TAIL:]) / TAIL
    qd_last = sum(fnum(r, "me_qd_score") for r in fit[-TAIL:]) / TAIL
    # per-generation mechanism rates over the tail window
    hunts_a = tail_mean(beh, "hunt_attempts")
    hunts_s = tail_mean(beh, "hunt_successes")
    throw_a = tail_mean(beh, "throw_attempts")
    throw_h = tail_mean(beh, "throw_hits")
    chop_a = tail_mean(beh, "chop_attempts")
    chop_s = tail_mean(beh, "chop_successes")
    craft_a = tail_mean(beh, "craft_attempts")
    craft_s = tail_mean(beh, "craft_successes")
    resp = tail_mean(beh, "signal_response_events")
    emits = tail_mean(beh, "signal_emits")
    drank = tail_mean(beh, "drank_water_events")
    surv = tail_mean(beh, "survived_protagonists")
    tot = tail_mean(beh, "total_protagonists")
    avgf = tail_mean(fit, "avg_fitness")
    bestf = tail_mean(fit, "best_fitness")
    rate = lambda s, a: (s / a * 100) if a > 0 else 0.0
    return {
        "gens": n, "gen_last": gens[-1] if gens else -1,
        "cov_first": cov_first, "cov_last": cov_last, "qd_last": qd_last,
        "hunt_rate": rate(hunts_s, hunts_a), "hunt_succ": hunts_s,
        "throw_rate": rate(throw_h, throw_a), "throw_hits": throw_h,
        "chop_rate": rate(chop_s, chop_a), "chop_succ": chop_s,
        "craft_rate": rate(craft_s, craft_a), "craft_succ": craft_s,
        "signal_resp": resp, "signal_emits": emits, "drank": drank,
        "survived": surv, "pop": tot, "avg_fit": avgf, "best_fit": bestf,
    }


def prey_check(run_dir):
    """Prey-side health: min population per species over the tail window."""
    sp_path = os.path.join(run_dir, "species_metrics.csv")
    if not os.path.exists(sp_path):
        return {}
    sp = load(sp_path)
    tail_g = sorted({int(float(r["generation"])) for r in sp})[-TAIL:]
    tail = [r for r in sp if int(float(r["generation"])) in tail_g]
    mins = {}
    for r in tail:
        name = r["species"]
        mins[name] = min(mins.get(name, 1e9), fnum(r, "population"))
    return mins


def main():
    base = os.path.dirname(os.path.abspath(__file__))
    seeds = sys.argv[1:] or ["50031", "50032"]
    for seed in seeds:
        print(f"\n===== seed {seed} =====")
        print(f"{'arm':<4} {'gens':>4} {'covFirst':>8} {'covLast':>8} "
              f"{'hunt%':>6} {'huntS':>6} {'throw%':>7} {'chop%':>6} "
              f"{'craft%':>7} {'sigResp':>8} {'surv':>5} {'avgFit':>8}")
        results = {}
        for arm in ("a", "b", "c"):
            rd = latest_run(os.path.join(base, "runs", f"pe_smoke_{arm}"))
            if not rd:
                print(f"{arm:<4}  (no run found)")
                continue
            r = analyze_run(rd)
            results[arm] = r
            print(f"{arm:<4} {r['gens']:>4} {r['cov_first']:>8.3f} "
                  f"{r['cov_last']:>8.3f} {r['hunt_rate']:>6.2f} "
                  f"{r['hunt_succ']:>6.1f} {r['throw_rate']:>7.2f} "
                  f"{r['chop_rate']:>6.2f} {r['craft_rate']:>7.3f} "
                  f"{r['signal_resp']:>8.2f} {r['survived']:>5.1f} "
                  f"{r['avg_fit']:>8.1f}")
        # prey-side health on the most mechanism-loaded arm available
        for arm in ("c", "b", "a"):
            rd = latest_run(os.path.join(base, "runs", f"pe_smoke_{arm}"))
            if rd:
                mins = prey_check(rd)
                extinct = {k: v for k, v in mins.items() if v <= 0}
                print(f"prey/health (arm {arm}): min-pop per species; "
                      f"EXTINCT={extinct if extinct else 'none'}")
                break
        # deltas vs baseline
        if "a" in results:
            for arm in ("b", "c"):
                if arm not in results:
                    continue
                r, a = results[arm], results["a"]
                dcov = r["cov_last"] - a["cov_last"]
                dhunt = r["hunt_rate"] - a["hunt_rate"]
                print(f"delta {arm}-a: covLast {dcov:+.3f}  hunt% {dhunt:+.2f} "
                      f" sigResp {r['signal_resp'] - a['signal_resp']:+.2f}")


if __name__ == "__main__":
    main()
