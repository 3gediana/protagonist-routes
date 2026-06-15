"""Behavioral-spectrum derivation — the multi-dimensional "panorama" that turns
the qualitative vision (理解世界 / 逻辑感 / 方向感 / 不退化 / 真实) into observable
proxy signals the AGENT can read to know "the direction".

Design contract (why this module exists):
- The engine ALREADY emits rich per-episode (DP jsonl) and per-generation (PE
  CSV) data, but the agent's decision path only ever consumed 3-4 coarse scalars
  (DP: score/reward/unlock-count; PE: avg/best/me_cov/qd). So the agent has been
  tuning blind. This module derives the full panorama from that EXISTING data —
  zero C++ changes, zero retrain, no reward/task/physics touched (red lines).
- Everything here is PURE (no I/O): inputs are already-parsed rows, outputs are
  plain dicts. server.py / agent.py do the file reading and call in here. This
  keeps it trivially unit-testable with synthetic rows.

Vision facets each metric serves (see docs/METRIC_DESIGN_world_coherence.md):
  world  = 理解世界 (effective response to causes, not empty-pressing)
  logic  = 逻辑感   (causal chain closure)
  dir    = 方向感   (main-line progression)
  broad  = 不退化   (breadth / diversity, anti single-strategy-collapse)
  real   = 真实     (anticipation / social / memory emergence)
"""
from __future__ import annotations

import math
from typing import Any, Optional

# --- DP vocab (mirrors server.ACTION_NAMES / MILESTONE_NAMES; a unit test
#     asserts these stay in lockstep so they can never silently drift). ------- #
ACTION_NAMES = [
    "eat", "drink", "HUNT", "collect", "shelter", "spear", "sleep",
    "grassdress", "furcloak", "wear", "blueprint", "cyclebld", "deposit",
    "plant", "water", "feedtame", "NOOP", "tendfire", "COOK", "MINE",
    "axe", "pickaxe", "monument",
]
MILESTONE_NAMES = [
    "drink", "eat", "collect", "spear", "shelter",
    "clothing", "seed", "house", "crop", "follower",
]
DEATH_CAUSES = [
    ("deaths_cold", "cold"), ("deaths_food", "food"),
    ("deaths_protein", "protein"), ("deaths_vitamin", "vitamin"),
]
NOOP_IDX = 16
TECH_ACTIONS = ["COOK", "MINE", "axe", "pickaxe", "monument"]   # idx 18..22
TECH_IDX = [ACTION_NAMES.index(a) for a in TECH_ACTIONS]
REWARD_KEYS = [
    "r_alive", "r_food", "r_water", "r_kills", "r_bites", "r_death",
    "r_vital", "r_collect", "r_shelter", "r_craft", "r_milestone", "r_potential",
]
# action index -> the milestone it nominally drives (for build-chain reading)
DEPOSIT_IDX = ACTION_NAMES.index("deposit")

# PE niche names (mirrors *_evolution.csv columns "<niche>_best"). rabbit is the
# prey species, kept separate from the 7 protagonist niches used for diversity.
PE_NICHES = [
    "large_herbivore", "small_forager", "apex_predator", "pack_hunter",
    "ambush_predator", "scavenger", "omnivore",
]
PE_SPECIALISTS = ["apex_predator", "pack_hunter", "ambush_predator"]


# --------------------------------------------------------------------------- #
# small helpers
# --------------------------------------------------------------------------- #
def _f(x: Any, default: float = 0.0) -> float:
    try:
        if x is None or x == "":
            return default
        return float(x)
    except (TypeError, ValueError):
        return default


def _norm_entropy(counts: list[float]) -> float:
    """Shannon entropy of a usage distribution, normalised to [0,1] by the max
    entropy over the number of bins. LOW = single-strategy collapse, HIGH =
    broad repertoire. Returns 0 for empty / single-bin distributions."""
    tot = sum(c for c in counts if c > 0)
    k = len(counts)
    if tot <= 0 or k <= 1:
        return 0.0
    h = 0.0
    for c in counts:
        if c > 0:
            p = c / tot
            h -= p * math.log(p)
    return h / math.log(k)


def _share(values: dict[str, float]) -> dict[str, float]:
    """Fraction of each value by absolute magnitude (so signed reward terms are
    comparable). Sums to ~1 over nonzero entries."""
    denom = sum(abs(v) for v in values.values())
    if denom <= 0:
        return {k: 0.0 for k in values}
    return {k: round(abs(v) / denom, 4) for k, v in values.items()}


def _trend(series: list[Optional[float]], rel: float = 0.05) -> dict[str, Any]:
    """Within-run trend of a per-episode/per-gen series: least-squares slope +
    early-half vs late-half delta -> improving|declining|plateau|too_short.
    This is the single most important thing the AGENT lacked: 'am I getting
    better as the round goes on, or degrading?' (a static end-value can't tell)."""
    pts = [(i, float(v)) for i, v in enumerate(series) if v is not None]
    n = len(pts)
    if n < 6:
        return {"verdict": "too_short", "slope": 0.0, "delta": 0.0,
                "early": None, "late": None}
    mx = sum(i for i, _ in pts) / n
    my = sum(v for _, v in pts) / n
    sxx = sum((i - mx) ** 2 for i, _ in pts)
    sxy = sum((i - mx) * (v - my) for i, v in pts)
    slope = sxy / sxx if sxx > 0 else 0.0
    half = n // 2
    early = sum(v for _, v in pts[:half]) / half
    late = sum(v for _, v in pts[half:]) / (n - half)
    delta = late - early
    scale = abs(my) if abs(my) > 1e-9 else 1.0
    if delta > rel * scale:
        verdict = "improving"
    elif delta < -rel * scale:
        verdict = "declining"
    else:
        verdict = "plateau"
    return {"verdict": verdict, "slope": round(slope, 5), "delta": round(delta, 4),
            "early": round(early, 4), "late": round(late, 4)}


def _geomean(vals: list[float], floor: float = 1e-6) -> float:
    """Geometric mean with a tiny floor so a single zero doesn't fully annihilate
    the score but a near-zero mechanism still drags it to the floor (truthful:
    'can't do one of the basic mechanisms' => low overall competence)."""
    xs = [max(_f(v), floor) for v in vals]
    if not xs:
        return 0.0
    return math.exp(sum(math.log(x) for x in xs) / len(xs))


def _arrow(verdict: str) -> str:
    return {"improving": "↑", "declining": "↓", "plateau": "→",
            "too_short": "?"}.get(verdict, "?")


# --------------------------------------------------------------------------- #
# DP — per-episode jsonl rows -> panorama
# --------------------------------------------------------------------------- #
def dp_spectrum(rows: list[dict[str, Any]]) -> dict[str, Any]:
    """Full DP behavioural panorama from per-episode jsonl rows.

    Each row already carries: reward + r_* decomposition, deaths/deaths_<cause>,
    unlocks[10], act[23], night_total/night_shelter, houses_built, sites_active,
    collect_total, explored_m. We DERIVE the multi-axis view from that."""
    n = len(rows)
    if n == 0:
        return {"ok": False, "error": "no DP rows"}

    nA = len(ACTION_NAMES)
    act_tot = [0.0] * nA
    distinct_per_life: list[float] = []
    entropy_series: list[float] = []     # per-episode action entropy (慌/稳)
    survival_series: list[float] = []    # per-episode lived? (1/0)
    score_series: list[float] = []       # per-episode score
    steps_series: list[float] = []       # per-episode lifetime in steps
    for r in rows:
        a = r.get("act", []) or []
        used = 0
        ep_counts = []
        for i in range(nA):
            v = _f(a[i]) if i < len(a) else 0.0
            act_tot[i] += v
            if i != NOOP_IDX:
                ep_counts.append(v)
                if v > 0:
                    used += 1
        distinct_per_life.append(used)
        entropy_series.append(_norm_entropy(ep_counts))
        survival_series.append(1.0 if _f(r.get("deaths", 0)) == 0 else 0.0)
        score_series.append(_f(r.get("score", 0)))
        steps_series.append(_f(r.get("steps", 0)))
    act_mean = [t / n for t in act_tot]

    tracked = [act_tot[i] for i in range(nA) if i != NOOP_IDX]
    actions_alive = sum(1 for t in tracked if t > 0)
    action_entropy = _norm_entropy(tracked)
    repertoire_per_life = sum(distinct_per_life) / n
    dead_actions = [ACTION_NAMES[i] for i in range(nA)
                    if i != NOOP_IDX and act_tot[i] == 0]

    # milestones (主线)
    nM = len(MILESTONE_NAMES)
    ms_fired = [0] * nM
    depth_per_life: list[float] = []
    for r in rows:
        u = r.get("unlocks", []) or []
        d = 0
        for i in range(nM):
            if i < len(u) and u[i]:
                ms_fired[i] += 1
                d += 1
        depth_per_life.append(d)
    ms_rate = [round(f / n, 4) for f in ms_fired]
    milestone_depth = sum(depth_per_life) / n
    milestone_breadth = sum(1 for f in ms_fired if f > 0)

    # tech tree depth (cook->mine->axe->pickaxe->monument)
    techtree_depth = sum(1 for i in TECH_IDX if act_tot[i] > 0)
    techtree_usage = sum(act_mean[i] for i in TECH_IDX)

    # survival / death structure (理解世界)
    dead_total = sum(_f(r.get("deaths", 0)) for r in rows)
    cause_tot = {label: sum(_f(r.get(k, 0)) for r in rows)
                 for k, label in DEATH_CAUSES}
    survival_rate = sum(1 for r in rows if _f(r.get("deaths", 0)) == 0) / n
    cold_death_share = (cause_tot["cold"] / dead_total) if dead_total > 0 else 0.0
    death_cause_balance = _norm_entropy(list(cause_tot.values()))

    # night sheltering (anticipation of cold = world understanding)
    nt = sum(_f(r.get("night_total", 0)) for r in rows)
    ns = sum(_f(r.get("night_shelter", 0)) for r in rows)
    night_shelter_ratio = (ns / nt) if nt > 0 else 0.0

    # build chain closure (逻辑感): collect -> deposit -> worksite -> house
    build_chain = {
        "collect_total": round(sum(_f(r.get("collect_total", 0)) for r in rows) / n, 2),
        "deposit_mean": round(act_mean[DEPOSIT_IDX], 2),
        "sites_active_mean": round(sum(_f(r.get("sites_active", 0)) for r in rows) / n, 3),
        "houses_built_total": int(sum(_f(r.get("houses_built", 0)) for r in rows)),
        "bldg_ticks_mean": round(sum(_f(r.get("bldg_ticks", 0)) for r in rows) / n, 1),
    }
    chain_closed = build_chain["houses_built_total"] > 0

    # reward decomposition (anti-Goodhart visibility): what actually drives reward
    r_means = {k: round(sum(_f(r.get(k, 0)) for r in rows) / n, 3) for k in REWARD_KEYS}
    r_shares = _share(r_means)

    explored_m = round(sum(_f(r.get("explored_m", 0)) for r in rows) / n, 1)
    mean_steps = round(sum(steps_series) / n, 1)

    # --- chain conversion ratios (逻辑感): where does the build chain break? --- #
    collect_mean = build_chain["collect_total"]
    deposit_mean = build_chain["deposit_mean"]
    chain_conv = {
        "deposit_per_collect": round(deposit_mean / collect_mean, 4) if collect_mean > 0 else 0.0,
        "worksite_per_deposit": round(build_chain["sites_active_mean"] / deposit_mean, 4) if deposit_mean > 0 else 0.0,
        "house_per_deposit": round(build_chain["houses_built_total"] / (deposit_mean * n), 5) if deposit_mean > 0 else 0.0,
    }

    # --- within-run trends (这一轮在学习还是退化) --- #
    trends = {
        "milestone_depth": _trend(depth_per_life),
        "survival": _trend(survival_series),
        "action_entropy": _trend(entropy_series),
        "score": _trend(score_series),
    }

    # --- composite directional indices (each carries a picture) --- #
    ms_share = r_shares.get("r_milestone", 0.0)
    goodhart_gap = round(ms_share - survival_rate, 4)         # >0 = 刷分但活不下来
    focus_survival = round(survival_rate - action_entropy, 4)  # 正=笃定且活;负=慌乱速死
    world_coherence = round(
        0.35 * survival_rate
        + 0.25 * min(milestone_depth / len(MILESTONE_NAMES), 1.0)
        + 0.20 * night_shelter_ratio
        + 0.20 * (1.0 if chain_closed else 0.0), 4)

    return {
        "ok": True, "project": "dp", "n": n,
        # 方向感 — main line
        "milestone_depth": round(milestone_depth, 3),
        "milestone_breadth": milestone_breadth,
        "milestone_rate": ms_rate,
        "milestone_names": MILESTONE_NAMES,
        "techtree_depth": techtree_depth,
        "techtree_usage": round(techtree_usage, 2),
        # 不退化 — breadth / anti-collapse
        "actions_alive": actions_alive,
        "action_entropy": round(action_entropy, 4),
        "repertoire_per_life": round(repertoire_per_life, 3),
        "dead_actions": dead_actions,
        "action_mean": [round(x, 2) for x in act_mean],
        "action_names": ACTION_NAMES,
        # 理解世界 — effective survival
        "survival_rate": round(survival_rate, 4),
        "deaths_mean": round(dead_total / n, 2),
        "death_causes": {k: int(v) for k, v in cause_tot.items()},
        "cold_death_share": round(cold_death_share, 4),
        "death_cause_balance": round(death_cause_balance, 4),
        "night_shelter_ratio": round(night_shelter_ratio, 4),
        # 逻辑感 — chain
        "build_chain": build_chain,
        "chain_closed": chain_closed,
        "chain_conversion": chain_conv,
        # anti-Goodhart — reward composition
        "reward_decomp": r_means,
        "reward_shares": r_shares,
        "r_potential_share": r_shares.get("r_potential", 0.0),
        "r_milestone_share": ms_share,
        "explored_m": explored_m,
        "mean_steps": mean_steps,
        # trends within the round (am I learning or degrading?)
        "trends": trends,
        # composite directional indices (each carries a picture)
        "goodhart_gap": goodhart_gap,
        "focus_survival": focus_survival,
        "world_coherence": world_coherence,
    }


def dp_axes(rows: list[dict[str, Any]]) -> dict[str, Any]:
    """Per-episode series feeding the multi-axis campaign verdict.
    progress = main-line depth (方向); breadth = repertoire/life (不退化)."""
    depth_series: list[float] = []
    breadth_series: list[float] = []
    score_series: list[float] = []
    nA = len(ACTION_NAMES)
    for r in rows:
        u = r.get("unlocks", []) or []
        depth_series.append(float(sum(1 for i in range(len(MILESTONE_NAMES))
                                      if i < len(u) and u[i])))
        a = r.get("act", []) or []
        breadth_series.append(float(sum(
            1 for i in range(nA) if i != NOOP_IDX and i < len(a) and _f(a[i]) > 0)))
        score_series.append(_f(r.get("score", 0)))
    return {"progress": depth_series, "breadth": breadth_series,
            "score": score_series}


# --------------------------------------------------------------------------- #
# PE — per-generation CSV rows -> panorama
# --------------------------------------------------------------------------- #
def _funnel(row: dict[str, Any], att_key: str, suc_key: str) -> dict[str, float]:
    att = _f(row.get(att_key))
    suc = _f(row.get(suc_key))
    return {"attempts": round(att, 1), "successes": round(suc, 1),
            "rate": round(suc / att, 5) if att > 0 else 0.0}


def pe_spectrum(behaviors: list[dict[str, Any]],
                fitness: list[dict[str, Any]],
                evolution: list[dict[str, Any]]) -> dict[str, Any]:
    """Full PE behavioural panorama from the per-generation CSV rows the engine
    already writes (behaviors / fitness / evolution). The panel previously read
    NONE of this — only avg/best/me_cov/qd scraped from console text."""
    if not behaviors and not fitness:
        return {"ok": False, "error": "no PE CSV rows"}

    bl = behaviors[-1] if behaviors else {}
    fl = fitness[-1] if fitness else {}
    el = evolution[-1] if evolution else {}

    # 理解世界 — mechanism efficacy funnels (real competence vs empty-pressing)
    funnels = {
        "chop": _funnel(bl, "chop_attempts", "chop_successes"),
        "craft": _funnel(bl, "craft_attempts", "craft_successes"),
        "throw": _funnel(bl, "throw_attempts", "throw_hits"),
        "hunt": _funnel(bl, "hunt_attempts", "hunt_successes"),
    }

    # 逻辑感 — gather -> deposit -> worksite chain
    chain = {
        "deposits_total": round(_f(bl.get("deposits_total")), 1),
        "completed_worksites": round(_f(bl.get("completed_worksites")), 2),
        "worksite_completion_events": round(_f(bl.get("worksite_completion_events")), 2),
        "active_fires": round(_f(bl.get("active_fires")), 2),
        "drank_water_events": round(_f(bl.get("drank_water_events")), 1),
    }
    chain_closed = chain["completed_worksites"] > 0

    # 不退化 — niche diversity / anti wash-out + MAP-Elites
    niche_best = {nm: round(_f(el.get(f"{nm}_best")), 2) for nm in PE_NICHES}
    max_best = max(niche_best.values()) if niche_best else 0.0
    thr = 0.1 * max_best if max_best > 0 else 0.0
    niche_occupancy = sum(1 for v in niche_best.values() if v > thr)
    omn = niche_best.get("omnivore", 0.0)
    spec = [niche_best.get(s, 0.0) for s in PE_SPECIALISTS]
    spec_mean = sum(spec) / len(spec) if spec else 0.0
    specialist_vs_omnivore = round(spec_mean / omn, 3) if omn > 0 else 0.0

    me_coverage = _f(fl.get("me_coverage"))
    me_qd_score = _f(fl.get("me_qd_score"))
    me_max_fitness = _f(fl.get("me_max_fitness"))

    # 真实 — social / memory emergence (PE north-star = realism)
    emits = _f(bl.get("signal_emits"))
    resp = _f(bl.get("signal_response_events"))
    social = {
        "signal_emits": round(emits, 1),
        "signal_response_events": round(resp, 1),
        "signal_response_rate": round(resp / emits, 4) if emits > 0 else 0.0,
        "signal_co_attendance": round(_f(bl.get("signal_co_attendance")), 2),
        "cooperative_seconds": round(_f(bl.get("cooperative_co_presence_seconds")), 1),
        "dnc_usage": round(_f(bl.get("dnc_usage_mean_avg")), 4),
    }

    # survival
    total = _f(bl.get("total_protagonists"))
    surv = _f(bl.get("survived_protagonists"))
    survival_ratio = round(surv / total, 4) if total > 0 else 0.0

    # --- competence (够不够得着世界): geomean of the 4 mechanism success rates - #
    def _rate(row, att, suc):
        a, s = _f(row.get(att)), _f(row.get(suc))
        return (s / a) if a > 0 else 0.0
    competence = round(_geomean([
        funnels["chop"]["rate"], funnels["craft"]["rate"],
        funnels["throw"]["rate"], funnels["hunt"]["rate"]]), 6)
    bottleneck = min(
        ("chop", "craft", "throw", "hunt"),
        key=lambda k: funnels[k]["rate"]) if funnels else None

    # --- diversity reality check (真多样 vs 虚假多样) --- #
    if niche_occupancy >= 5 and competence < 1e-3:
        diversity_real = "fake"        # 铺开了生态位却全够不着机制
    elif niche_occupancy >= 5:
        diversity_real = "healthy"
    else:
        diversity_real = "narrow"
    dominant_niche = max(niche_best, key=niche_best.get) if niche_best else None
    niche_balance = round(_norm_entropy(list(niche_best.values())), 4)
    fitness_spread = round(_f(fl.get("best_fitness")) - _f(fl.get("avg_fitness")), 2)
    min_fitness = round(_f(fl.get("min_fitness")), 2)
    coop_per_survivor = round(social["cooperative_seconds"] / surv, 2) if surv > 0 else 0.0
    worksite_per_deposit = round(chain["completed_worksites"] / chain["deposits_total"], 4) \
        if chain["deposits_total"] > 0 else 0.0
    dnc_detail = {
        "usage_mean": social["dnc_usage"],
        "write_peak": round(_f(bl.get("dnc_write_peak_avg")), 4),
        "read_peak": round(_f(bl.get("dnc_read_peak_avg")), 4),
        "precedence_peak": round(_f(bl.get("dnc_precedence_peak_avg")), 4),
    }

    # --- cross-generation trends (这一轮在进步还是退化/灭绝) --- #
    comp_series = [_geomean([
        _rate(b, "chop_attempts", "chop_successes"),
        _rate(b, "craft_attempts", "craft_successes"),
        _rate(b, "throw_attempts", "throw_hits"),
        _rate(b, "hunt_attempts", "hunt_successes")]) for b in behaviors]
    surv_series = [_f(b.get("survived_protagonists")) for b in behaviors]
    trends = {
        "avg_fitness": _trend([_f(r.get("avg_fitness")) for r in fitness]),
        "me_coverage": _trend([_f(r.get("me_coverage")) for r in fitness]),
        "competence": _trend(comp_series),
        "survivors": _trend(surv_series),
    }
    # extinction risk: survivors falling toward zero
    st = trends["survivors"]
    if surv <= 0:
        extinction_risk = "extinct"
    elif st["verdict"] == "declining" and surv < 0.25 * (total or 1):
        extinction_risk = "rising"
    else:
        extinction_risk = "stable"

    return {
        "ok": True, "project": "pe",
        "gen_last": int(_f(bl.get("generation", fl.get("generation", 0)))),
        "n_behavior_rows": len(behaviors), "n_fitness_rows": len(fitness),
        # 理解世界
        "mechanism_funnels": funnels,
        # 逻辑感
        "chain": chain, "chain_closed": chain_closed,
        "mechanism_competence": competence,
        "mechanism_bottleneck": bottleneck,
        # 逻辑感 — chain conversion
        "worksite_per_deposit": worksite_per_deposit,
        # 不退化
        "niche_best": niche_best,
        "niche_occupancy": niche_occupancy,
        "specialist_vs_omnivore": specialist_vs_omnivore,
        "dominant_niche": dominant_niche,
        "niche_balance": niche_balance,
        "diversity_real": diversity_real,
        "me_coverage": me_coverage,
        "me_qd_score": me_qd_score,
        "me_max_fitness": me_max_fitness,
        # 方向感 — fitness level
        "avg_fitness": round(_f(fl.get("avg_fitness")), 2),
        "best_fitness": round(_f(fl.get("best_fitness")), 2),
        "min_fitness": min_fitness,
        "fitness_spread": fitness_spread,
        # 真实
        "social": social,
        "coop_per_survivor": coop_per_survivor,
        "dnc_detail": dnc_detail,
        # survival
        "survival_ratio": survival_ratio,
        "survivors": int(surv),
        "population": int(total),
        # trends within the round (progress / degradation / extinction)
        "trends": trends,
        "extinction_risk": extinction_risk,
    }


def pe_axes(behaviors: list[dict[str, Any]],
            fitness: list[dict[str, Any]],
            evolution: list[dict[str, Any]]) -> dict[str, Any]:
    """Per-generation series feeding the multi-axis verdict.
    progress = avg_fitness (方向); breadth = me_coverage (不退化)."""
    progress = [_f(r.get("avg_fitness")) for r in fitness] if fitness else \
               [_f(r.get("avg_fitness")) for r in evolution]
    breadth = [_f(r.get("me_coverage")) for r in fitness]
    qd = [_f(r.get("me_qd_score")) for r in fitness]
    return {"progress": progress, "breadth": breadth, "qd": qd}


# --------------------------------------------------------------------------- #
# multi-axis verdict — the anti-Goodhart core
# --------------------------------------------------------------------------- #
def multi_axis_verdict(progress_trend: dict[str, Any],
                       breadth_trend: dict[str, Any], *,
                       at_final: bool,
                       extinct: bool = False) -> tuple[str, str]:
    """Decide promote|revert|extend|done from a PROGRESS-axis trend and a
    BREADTH-axis trend. The anti-Goodhart rule: a collapsing breadth axis vetoes
    promotion even when progress is rising (score went up but behaviour
    collapsed to one strategy = wrong direction). PURE -> unit-testable.

    Inputs are _trend() dicts ({"verdict": improving|declining|plateau|too_short}).
    """
    if extinct:
        return "revert", "检测到灭绝(extinct)——回退到上一阶段并保持。"
    pv = progress_trend.get("verdict")
    bv = breadth_trend.get("verdict")
    if (pv in (None, "too_short")) and (bv in (None, "too_short")):
        return "extend", "样本太少,延长观察一档再判定。"
    # anti-Goodhart: breadth collapse vetoes everything (even rising progress)
    if bv == "declining":
        return "revert", ("广度轴下行(行为坍缩/多样性流失)——即便进度上行也回退,"
                          "这是反 Goodhart 护栏:刷分坍缩不算进步。")
    if pv == "declining":
        return "revert", "进度轴下行(主线/适应度退步)——回退保持,先查既往坑。"
    if pv == "improving":
        if at_final:
            return "done", "最终阶段进度仍上行且广度未退——产出当前最优,收尾。"
        return "promote", "进度上行且广度不退——晋级到更大规模/更长训练。"
    # progress plateau, breadth not declining
    if at_final:
        return "done", "最终阶段进度平台且广度稳住——视为收敛,产出当前最优。"
    return "promote", "当前规模进度已学满(平台)且广度稳住——晋级继续提升。"


# --------------------------------------------------------------------------- #
# compact human/agent-facing summary line (goes into KB each run)
# --------------------------------------------------------------------------- #
def summarize(spec: dict[str, Any]) -> str:
    """One-line panorama digest for the KB / timeline (agent's cross-gen memory)."""
    if not spec or not spec.get("ok"):
        return "(谱不可用)"
    if spec.get("project") == "dp":
        return (
            f"DP谱: 主线深度{spec['milestone_depth']:.2f}/10 广度{spec['milestone_breadth']}/10 "
            f"科技树{spec['techtree_depth']}/5 | 动作存活{spec['actions_alive']}/22 "
            f"熵{spec['action_entropy']:.2f} 每命机制{spec['repertoire_per_life']:.1f} | "
            f"存活率{spec['survival_rate']:.2f} 夜避寒{spec['night_shelter_ratio']:.2f} "
            f"死因均衡{spec['death_cause_balance']:.2f} | 链闭合={spec['chain_closed']} "
            f"势能份额{spec['r_potential_share']:.2f}")
    f = spec.get("mechanism_funnels", {})
    return (
        f"PE谱@gen{spec.get('gen_last')}: 覆盖{spec['me_coverage']:.0f} QD{spec['me_qd_score']:.0f} "
        f"niche占用{spec['niche_occupancy']}/7 专精/通才{spec['specialist_vs_omnivore']:.2f} | "
        f"砍{f.get('chop',{}).get('rate',0):.3f} 造{f.get('craft',{}).get('rate',0):.3f} "
        f"投{f.get('throw',{}).get('rate',0):.3f} 猎{f.get('hunt',{}).get('rate',0):.3f} | "
        f"链闭合={spec['chain_closed']} 通讯响应{spec['social']['signal_response_rate']:.2f} "
        f"存活{spec['survival_ratio']:.2f}")


# --------------------------------------------------------------------------- #
# narrate — turn the panorama into SCENE captions the agent reads to judge a
# round. Output is grouped by the four questions the agent actually asks:
#   末态(now) / 趋势(getting better?) / 反刷分(real or gamed?) / 总判(act)
# Each scene = {q, metric, value, caption}. Also returns a joined `story` text
# and a `verdict_hint` (promote|revert|redirect|hold + reason).
# --------------------------------------------------------------------------- #
def _pct(x: float) -> str:
    return f"{x * 100:.2f}%"


def narrate_dp(spec: dict[str, Any]) -> dict[str, Any]:
    if not spec or not spec.get("ok") or spec.get("project") != "dp":
        return {"ok": False, "scenes": [], "story": "(DP谱不可用)", "verdict_hint": ("hold", "无数据")}
    tr = spec.get("trends", {})
    sr = spec["survival_rate"]
    md = spec["milestone_depth"]
    ent = spec["action_entropy"]
    fs = spec["focus_survival"]
    gg = spec["goodhart_gap"]
    bc = spec["build_chain"]
    scenes = []

    # ① 末态 now
    if fs >= 0:
        cap = f"笃定且能活——做对的几件事并活下来(focus_survival={fs:+.2f})"
    else:
        cap = f"慌乱速死——动作熵高({ent:.2f})却几乎不存活(focus_survival={fs:+.2f})"
    scenes.append({"q": "末态", "metric": "focus_survival", "value": fs, "caption": cap})
    scenes.append({"q": "末态", "metric": "survival_rate", "value": sr,
                   "caption": f"存活率{_pct(sr)} {_arrow(tr.get('survival',{}).get('verdict','?'))}——"
                              + ("几乎开局即送" if sr < 0.05 else "能撑过一部分局")})
    scenes.append({"q": "末态", "metric": "milestone_depth", "value": md,
                   "caption": f"主线深度{md:.2f}/10 {_arrow(tr.get('milestone_depth',{}).get('verdict','?'))}——"
                              + ("永远卡在第一步" if md < 1 else f"推进到第~{md:.0f}级")
                              + f";科技树{spec['techtree_depth']}/5"})
    scenes.append({"q": "末态", "metric": "night_shelter_ratio", "value": spec["night_shelter_ratio"],
                   "caption": f"夜间避寒{_pct(spec['night_shelter_ratio'])}——"
                              + ("天黑从不躲,在野外挨冻" if spec["night_shelter_ratio"] < 0.05 else "会躲遮蔽")})
    scenes.append({"q": "末态", "metric": "action_entropy", "value": ent,
                   "caption": f"动作熵{ent:.2f}——" + ("乱按一通(慌)" if ent > 0.5 else "动作集中(稳)")
                              + f";死动作{len(spec['dead_actions'])}个从未触发"})
    chain_state = "闭合(建出房)" if spec["chain_closed"] else "断裂(从不建房)"
    scenes.append({"q": "末态", "metric": "build_chain", "value": spec["chain_closed"],
                   "caption": f"建造链:采集{bc['collect_total']:.0f}→存放{bc['deposit_mean']:.1f}→"
                              f"房{bc['houses_built_total']} = {chain_state}"})

    # ② 趋势 getting better?
    for key, label in (("milestone_depth", "主线深度"), ("survival", "存活"),
                       ("action_entropy", "动作熵"), ("score", "score")):
        t = tr.get(key, {})
        v = t.get("verdict", "too_short")
        good = (v == "improving") if key != "action_entropy" else (v == "declining")
        note = "(向好)" if good else ("(退化)" if v in ("declining", "improving") else "")
        if key == "action_entropy" and v == "improving":
            note = "(在变慌/退化)"
        scenes.append({"q": "趋势", "metric": f"trend.{key}", "value": v,
                       "caption": f"{label}趋势 {_arrow(v)} {v} {note} "
                                  f"(早{t.get('early')}→晚{t.get('late')})"})

    # ③ 反刷分 real or gamed?
    if gg > 0.1:
        cap = f"⚠刷分嫌疑:里程碑奖励占比{_pct(spec['r_milestone_share'])}远高于存活{_pct(sr)}(gap={gg:+.2f})"
    else:
        cap = f"奖励与存活基本对齐(goodhart_gap={gg:+.2f})"
    scenes.append({"q": "反刷分", "metric": "goodhart_gap", "value": gg, "caption": cap})
    scenes.append({"q": "反刷分", "metric": "r_potential_share", "value": spec["r_potential_share"],
                   "caption": f"势能塑形占比{_pct(spec['r_potential_share'])}"
                              + ("(偏高,警惕过拟合塑形项)" if spec["r_potential_share"] > 0.2 else "")})

    # ④ 总判 act
    hint = _dp_verdict(spec)
    headline = (f"DP: world_coherence={spec['world_coherence']:.2f}, focus_survival={fs:+.2f}, "
                f"主线{md:.2f}/10, 存活{_pct(sr)}, goodhart_gap={gg:+.2f}")
    story = headline + "\n" + "\n".join(f"  [{s['q']}] {s['caption']}" for s in scenes)
    story += f"\n  → 判定: {hint[0]} — {hint[1]}"
    return {"ok": True, "project": "dp", "scenes": scenes, "headline": headline,
            "story": story, "verdict_hint": hint}


def _dp_verdict(spec: dict[str, Any]) -> tuple[str, str]:
    tr = spec.get("trends", {})
    sr = spec["survival_rate"]
    gg = spec["goodhart_gap"]
    md_t = tr.get("milestone_depth", {}).get("verdict")
    sv_t = tr.get("survival", {}).get("verdict")
    ent_t = tr.get("action_entropy", {}).get("verdict")
    if gg > 0.15 and sr < 0.1:
        return ("redirect", "在刷里程碑/势能分但活不下来——降低里程碑+势能权重,先把存活救起来(别再加分项)。")
    if sv_t == "declining" or ent_t == "improving":
        return ("revert", "存活在退化或动作熵在上涨(越来越慌)——回退上一档,先稳住存活与专注。")
    if md_t == "improving" and sv_t in ("improving", "plateau"):
        return ("promote", "主线在爬升且存活未退化——可晋级更大规模/更长训练。")
    if sr < 0.05 and md_t in ("plateau", "too_short"):
        return ("redirect", "存活地板且主线不动——换方向:先解决前期觅食/避寒可达性,而非加难度。")
    return ("hold", "进度平台、无明显退化——保持当前档位,延长观察或微调安全域旋钮。")


def narrate_pe(spec: dict[str, Any]) -> dict[str, Any]:
    if not spec or not spec.get("ok") or spec.get("project") != "pe":
        return {"ok": False, "scenes": [], "story": "(PE谱不可用)", "verdict_hint": ("hold", "无数据")}
    tr = spec.get("trends", {})
    comp = spec["mechanism_competence"]
    cov = spec["me_coverage"]
    occ = spec["niche_occupancy"]
    dr = spec["diversity_real"]
    avg = spec["avg_fitness"]
    sr = spec["survival_ratio"]
    f = spec["mechanism_funnels"]
    soc = spec["social"]
    scenes = []

    # ① 末态 now
    dr_cap = {"fake": f"⚠虚假多样:niche占用{occ}/7看着健康,但机制几乎全够不着(competence={comp:.4f})——铺开了却执行不了",
              "healthy": f"真多样:niche占用{occ}/7且机制可执行(competence={comp:.4f})",
              "narrow": f"窄:niche占用仅{occ}/7,种群挤在少数打法"}.get(dr, dr)
    scenes.append({"q": "末态", "metric": "diversity_real", "value": dr, "caption": dr_cap})
    scenes.append({"q": "末态", "metric": "mechanism_competence", "value": comp,
                   "caption": f"机制胜任度{comp:.4f}(瓶颈={spec['mechanism_bottleneck']}):"
                              f"砍{f['chop']['rate']:.3f}/造{f['craft']['rate']:.3f}/"
                              f"投{f['throw']['rate']:.3f}/猎{f['hunt']['rate']:.3f}"
                              + ("——可达性灾难" if comp < 1e-3 else "")})
    scenes.append({"q": "末态", "metric": "me_coverage", "value": cov,
                   "caption": f"行为空间覆盖{cov:.2f} {_arrow(tr.get('me_coverage',{}).get('verdict','?'))};"
                              f"专精/通才{spec['specialist_vs_omnivore']:.2f};主导niche={spec['dominant_niche']}"})
    scenes.append({"q": "末态", "metric": "avg_fitness", "value": avg,
                   "caption": f"平均适应度{avg:.0f}{_arrow(tr.get('avg_fitness',{}).get('verdict','?'))}"
                              f"(最强{spec['best_fitness']:.0f},离散{spec['fitness_spread']:.0f})"})
    scenes.append({"q": "末态", "metric": "survival_ratio", "value": sr,
                   "caption": f"存活{_pct(sr)}({spec['survivors']}/{spec['population']})"})
    scenes.append({"q": "末态", "metric": "social", "value": soc["signal_response_rate"],
                   "caption": f"通讯响应{_pct(soc['signal_response_rate'])}"
                              f"(发{soc['signal_emits']:.0f}/应{soc['signal_response_events']:.0f})"
                              + ("——通讯未涌现" if soc["signal_response_rate"] <= 0 else "")
                              + f";协作/存活者{spec['coop_per_survivor']:.0f}s;记忆用量{soc['dnc_usage']:.2f}"})
    scenes.append({"q": "末态", "metric": "worksite_per_deposit", "value": spec["worksite_per_deposit"],
                   "caption": f"协作链:存放{spec['chain']['deposits_total']:.0f}→完成工地"
                              f"{spec['chain']['completed_worksites']:.1f}(转化{spec['worksite_per_deposit']:.3f})"})

    # ② 趋势
    for key, label, good_dir in (("avg_fitness", "平均适应度", "improving"),
                                 ("me_coverage", "覆盖", "improving"),
                                 ("competence", "机制胜任度", "improving"),
                                 ("survivors", "存活者数", "improving")):
        t = tr.get(key, {})
        v = t.get("verdict", "too_short")
        note = "(向好)" if v == good_dir else ("(退化)" if v == "declining" else "")
        scenes.append({"q": "趋势", "metric": f"trend.{key}", "value": v,
                       "caption": f"{label}趋势 {_arrow(v)} {v} {note} "
                                  f"(早{t.get('early')}→晚{t.get('late')})"})
    scenes.append({"q": "趋势", "metric": "extinction_risk", "value": spec["extinction_risk"],
                   "caption": f"灭绝风险={spec['extinction_risk']}"})

    # ③ 反刷分
    if dr == "fake":
        scenes.append({"q": "反刷分", "metric": "fake_diversity", "value": True,
                       "caption": "覆盖率虚高(看着多样)但机制够不着——别拿覆盖当进步,先救可达性"})
    if tr.get("avg_fitness", {}).get("verdict") == "improving" and \
       tr.get("me_coverage", {}).get("verdict") == "declining":
        scenes.append({"q": "反刷分", "metric": "breadth_collapse", "value": True,
                       "caption": "⚠适应度涨但覆盖在坍缩——刷分坍缩,反 Goodhart 应回退"})

    hint = _pe_verdict(spec)
    headline = (f"PE@gen{spec.get('gen_last')}: competence={comp:.4f}({dr}), 覆盖{cov:.2f}, "
                f"avg{avg:.0f}, 存活{_pct(sr)}, 灭绝风险={spec['extinction_risk']}")
    story = headline + "\n" + "\n".join(f"  [{s['q']}] {s['caption']}" for s in scenes)
    story += f"\n  → 判定: {hint[0]} — {hint[1]}"
    return {"ok": True, "project": "pe", "scenes": scenes, "headline": headline,
            "story": story, "verdict_hint": hint}


def _pe_verdict(spec: dict[str, Any]) -> tuple[str, str]:
    tr = spec.get("trends", {})
    er = spec["extinction_risk"]
    dr = spec["diversity_real"]
    avg_t = tr.get("avg_fitness", {}).get("verdict")
    cov_t = tr.get("me_coverage", {}).get("verdict")
    comp_t = tr.get("competence", {}).get("verdict")
    if er == "extinct":
        return ("revert", "种群灭绝——回退上一档并保持,先解决可生存性。")
    if er == "rising":
        return ("redirect", "存活者向零滑落(灭绝风险上升)——降低难度/资源稀缺度,先救存活,别推进度。")
    if dr == "fake":
        return ("redirect", "虚假多样(覆盖虚高但机制够不着)——先把机制可达性(competence)救起来,别庆祝覆盖率。")
    if avg_t == "improving" and cov_t == "declining":
        return ("revert", "适应度涨但覆盖坍缩——反 Goodhart:刷分坍缩不算进步,回退。")
    if (avg_t == "improving" or comp_t == "improving") and cov_t in ("improving", "plateau"):
        return ("promote", "适应度/胜任度上行且覆盖未退——可晋级更大规模/更长训练。")
    return ("hold", "进度平台、无坍缩/灭绝——保持档位,延长观察或在安全域微调。")
