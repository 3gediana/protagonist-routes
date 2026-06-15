"""Metric validity & structure study (read-only, on real run data).

Answers three questions on the user's own runs:
  1) effectivite: do the proxy metrics move with the latent quality / are they
     gameable?  -> via internal consistency + Goodhart-gap checks.
  2) structure: which metrics are redundant?  -> Pearson correlation matrix,
     flag |r|>=0.85 pairs (collapse them).
  3) directionality: can we build a stronger leading index? -> lead-lag cross
     correlation: does metric@t predict a slow trusted signal@t+k, earlier/
     better than raw avg/score?

Pure python, no numpy. Within-run time series (large n) is the primary evidence;
cross-run (small n) is suggestive only and labelled as such.
"""
import csv
import json
import math
import os
import sys

try:
    sys.stdout.reconfigure(encoding="utf-8")
except Exception:
    pass

PE_RUN = sys.argv[1] if len(sys.argv) > 1 else None
DP_FILE = sys.argv[2] if len(sys.argv) > 2 else None


# ----------------------------- stats helpers ------------------------------- #
def _f(x):
    try:
        return float(x)
    except (TypeError, ValueError):
        return None


def pearson(xs, ys):
    pairs = [(a, b) for a, b in zip(xs, ys) if a is not None and b is not None]
    n = len(pairs)
    if n < 4:
        return None, n
    mx = sum(a for a, _ in pairs) / n
    my = sum(b for _, b in pairs) / n
    sx = sum((a - mx) ** 2 for a, _ in pairs)
    sy = sum((b - my) ** 2 for _, b in pairs)
    if sx <= 1e-12 or sy <= 1e-12:
        return None, n  # degenerate / constant
    cov = sum((a - mx) * (b - my) for a, b in pairs)
    return cov / math.sqrt(sx * sy), n


def lead_lag(metric, target, maxlag=8):
    """Best |corr| of metric@t vs target@t+k over k=0..maxlag. Returns
    (best_lag, best_r, r_at_0)."""
    r0, _ = pearson(metric, target)
    best = (0, r0 or 0.0)
    for k in range(1, maxlag + 1):
        if k >= len(metric):
            break
        r, n = pearson(metric[:-k], target[k:])
        if r is not None and abs(r) > abs(best[1]):
            best = (k, r)
    return best[0], best[1], r0


def col(rows, key):
    return [_f(r.get(key)) for r in rows]


def ratio(rows, num, den):
    out = []
    for r in rows:
        a, b = _f(r.get(num)), _f(r.get(den))
        out.append((a / b) if (a is not None and b not in (None, 0)) else None)
    return out


def zscore(xs):
    v = [x for x in xs if x is not None]
    if len(v) < 2:
        return [0.0 for _ in xs]
    m = sum(v) / len(v)
    sd = math.sqrt(sum((x - m) ** 2 for x in v) / len(v)) or 1.0
    return [((x - m) / sd) if x is not None else None for x in xs]


def read_csv(path):
    if not os.path.exists(path):
        return []
    with open(path, "r", encoding="utf-8", errors="ignore", newline="") as fh:
        return list(csv.DictReader(fh))


def merge_on_gen(*tables):
    """Inner-join rows from multiple csv tables on 'generation'."""
    if not tables or not tables[0]:
        return []
    idx = []
    maps = [{r.get("generation"): r for r in t} for t in tables]
    keys = [k for k in maps[0] if all(k in m for m in maps[1:])]
    keys.sort(key=lambda k: _f(k) if _f(k) is not None else 0)
    for k in keys:
        merged = {}
        for m in maps:
            merged.update(m[k])
        idx.append(merged)
    return idx


def depict_screen(series: dict, title):
    """Objective half of the 画面感 screen: per metric report variance (dead?),
    coefficient of variation, and its strongest redundancy partner."""
    print(f"\n===== {title}: 画面感初筛 (方差/死指标 + 最强冗余伙伴) =====")
    names = list(series.keys())
    stats = {}
    for a in names:
        v = [x for x in series[a] if x is not None]
        n = len(v)
        if n < 2:
            stats[a] = (n, None, None, "NO-DATA")
            continue
        m = sum(v) / n
        sd = math.sqrt(sum((x - m) ** 2 for x in v) / n)
        cv = (sd / abs(m)) if abs(m) > 1e-9 else (0.0 if sd < 1e-9 else 999)
        flag = "DEAD" if sd < 1e-9 else ""
        stats[a] = (n, m, cv, flag)
    print(f"  {'metric':<14}{'n':>4}{'mean':>12}{'CV':>8}  partner(max|r|)")
    for a in names:
        n, m, cv, flag = stats[a]
        best = ("", 0.0)
        for b in names:
            if b == a:
                continue
            r, _ = pearson(series[a], series[b])
            if r is not None and abs(r) > abs(best[1]):
                best = (b, r)
        ms = f"{m:.4g}" if m is not None else "--"
        cvs = f"{cv:.2f}" if cv is not None else "--"
        part = f"{best[0]} ({best[1]:+.2f})" if best[0] else "--"
        print(f"  {a:<14}{n:>4}{ms:>12}{cvs:>8}  {part:<22}{flag}")


def corr_table(series: dict, title):
    print(f"\n===== {title}: 相关矩阵 (Pearson r, 代内/局内时间序列) =====")
    names = list(series.keys())
    print("       " + " ".join(f"{n[:6]:>6}" for n in names))
    redundant = []
    for a in names:
        cells = []
        for b in names:
            r, n = pearson(series[a], series[b])
            cells.append(f"{r:+.2f}" if r is not None else "  -- ")
            if a < b and r is not None and abs(r) >= 0.85:
                redundant.append((a, b, r))
        print(f"{a[:6]:>6} " + " ".join(f"{c:>6}" for c in cells))
    if redundant:
        print("  [!] 高度冗余对 |r|>=0.85 (建议塌成一个轴):")
        for a, b, r in redundant:
            print(f"     {a} ~ {b}: r={r:+.2f}")
    else:
        print("  (无 |r|>=0.85 的冗余对)")
    return redundant


def leadlag_table(series: dict, targets, title):
    print(f"\n===== {title}: 领先性 lead-lag (metric@t -> target@t+k) =====")
    for tgt in targets:
        if tgt not in series:
            continue
        print(f"  -- 目标 = {tgt} --")
        rows = []
        for name, xs in series.items():
            if name == tgt:
                continue
            k, rk, r0 = lead_lag(xs, series[tgt])
            lead = "领先" if (k > 0 and abs(rk) - abs(r0 or 0) > 0.05) else ""
            rows.append((abs(rk), name, k, rk, r0, lead))
        rows.sort(reverse=True)
        for ar, name, k, rk, r0, lead in rows:
            r0s = f"{r0:+.2f}" if r0 is not None else " -- "
            print(f"     {name:<14} best_lag={k}  r@lag={rk:+.2f}  r@0={r0s}  {lead}")


# ------------------------------- PE study ---------------------------------- #
def study_pe(run_dir):
    fit = read_csv(os.path.join(run_dir, "fitness.csv"))
    beh = read_csv(os.path.join(run_dir, "protagonist_behaviors.csv"))
    evo = read_csv(os.path.join(run_dir, "evolution.csv"))
    rows = merge_on_gen(fit, beh, evo)
    print(f"\n################ PE run: {run_dir}")
    print(f"merged gens = {len(rows)}")
    if len(rows) < 8:
        print("  too few gens; skip")
        return
    arche = ["large_herbivore", "small_forager", "apex_predator", "pack_hunter",
             "ambush_predator", "scavenger", "omnivore"]
    # per-gen niche occupancy: # archetypes whose best >10% of the gen's max best
    occ = []
    spec_omni = []
    for r in rows:
        bests = {a: _f(r.get(a + "_best")) for a in arche}
        vals = [v for v in bests.values() if v is not None]
        if not vals:
            occ.append(None)
            spec_omni.append(None)
            continue
        mx = max(vals) or 1.0
        occ.append(sum(1 for v in vals if v > 0.1 * mx))
        omni = bests.get("omnivore") or 1e-9
        spec = [bests[a] for a in arche if a != "omnivore" and bests[a] is not None]
        spec_omni.append((sum(spec) / len(spec) / omni) if spec and omni else None)
    series = {
        "avg": col(rows, "avg_fitness"),
        "best": col(rows, "best_fitness"),
        "me_cov": col(rows, "me_coverage"),
        "qd": col(rows, "me_qd_score"),
        "survival": ratio(rows, "survived_protagonists", "total_protagonists"),
        "chop": ratio(rows, "chop_successes", "chop_attempts"),
        "craft": ratio(rows, "craft_successes", "craft_attempts"),
        "throw": ratio(rows, "throw_hits", "throw_attempts"),
        "hunt": ratio(rows, "hunt_successes", "hunt_attempts"),
        "deposits": col(rows, "deposits_total"),
        "worksites": col(rows, "completed_worksites"),
        "sig_resp": ratio(rows, "signal_response_events", "signal_emits"),
        "coop_s": col(rows, "cooperative_co_presence_seconds"),
        "dnc": col(rows, "dnc_usage_mean_avg"),
        "niche_occ": occ,
        "spec_omni": spec_omni,
    }
    depict_screen(series, "PE")
    corr_table(series, "PE")
    # candidate composite: competence = mean z of the 4 mechanism rates
    zr = [zscore(series[k]) for k in ("chop", "craft", "throw", "hunt")]
    competence = []
    for i in range(len(rows)):
        vs = [z[i] for z in zr if z[i] is not None]
        competence.append(sum(vs) / len(vs) if vs else None)
    series2 = dict(series)
    series2["COMPETENCE"] = competence
    # diversity health = niche_occ z + (spec_omni balance, penalise <1)
    zocc = zscore(series["niche_occ"])
    zso = zscore(series["spec_omni"])
    divhealth = []
    for i in range(len(rows)):
        a, b = zocc[i], zso[i]
        vs = [v for v in (a, b) if v is not None]
        divhealth.append(sum(vs) / len(vs) if vs else None)
    series2["DIV_HEALTH"] = divhealth
    leadlag_table(series2, ["me_cov", "avg", "qd"], "PE (含候选复合指标)")


# ------------------------------- DP study ---------------------------------- #
def shannon(counts):
    tot = sum(counts)
    if tot <= 0:
        return 0.0
    ps = [c / tot for c in counts if c > 0]
    if len(ps) <= 1:
        return 0.0
    h = -sum(p * math.log(p) for p in ps)
    return h / math.log(len(counts))


def study_dp(path):
    rows = []
    with open(path, "r", encoding="utf-8", errors="ignore") as fh:
        for line in fh:
            line = line.strip()
            if line:
                try:
                    rows.append(json.loads(line))
                except json.JSONDecodeError:
                    pass
    print(f"\n################ DP run: {os.path.basename(path)}")
    print(f"episodes = {len(rows)}")
    if len(rows) < 8:
        print("  too few episodes; skip")
        return
    print("keys:", sorted(rows[0].keys()))

    def g(r, k):
        return _f(r.get(k))

    entropy, repertoire, ndeaths, night_sh, ms_depth = [], [], [], [], []
    for r in rows:
        act = r.get("act") or []
        act = [a for a in act if isinstance(a, (int, float))]
        body = act[1:] if len(act) > 1 else act  # drop NOOP (idx0)
        entropy.append(shannon(body) if body else None)
        repertoire.append(sum(1 for a in body if a > 0) if body else None)
        d = r.get("deaths")
        ndeaths.append(_f(d) if not isinstance(d, list) else float(sum(d)))
        nt, ns = g(r, "night_total"), g(r, "night_shelter")
        night_sh.append((ns / nt) if (nt and ns is not None) else None)
        unl = r.get("unlocks") or []
        depth = 0
        for u in unl:
            if u:
                depth += 1
            else:
                break
        ms_depth.append(float(depth))
    survival = [(0.0 if (d and d > 0) else 1.0) if d is not None else None
                for d in ndeaths]
    series = {
        "score": col(rows, "score"),
        "reward": col(rows, "reward"),
        "ms_depth": ms_depth,
        "entropy": entropy,
        "repert": repertoire,
        "night_sh": night_sh,
        "survival": survival,
    }
    # reward-decomp shares if present
    for k in ("r_milestone", "r_potential", "r_death", "r_alive", "r_collect",
              "r_shelter", "r_craft"):
        c = col(rows, k)
        if any(v is not None for v in c):
            series[k] = c
    depict_screen(series, "DP")
    corr_table(series, "DP")
    # candidate composite: coherence = z(survival)+z(ms_depth)+z(night_sh)
    zs = {k: zscore(series[k]) for k in ("survival", "ms_depth", "night_sh", "repert")}
    coherence = []
    for i in range(len(rows)):
        vs = [zs[k][i] for k in zs if zs[k][i] is not None]
        coherence.append(sum(vs) / len(vs) if vs else None)
    series2 = dict(series)
    series2["COHERENCE"] = coherence
    leadlag_table(series2, ["survival", "score", "ms_depth"], "DP (含候选复合指标)")


if __name__ == "__main__":
    if PE_RUN:
        study_pe(PE_RUN)
    if DP_FILE:
        study_dp(DP_FILE)
