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
    for r in rows:
        a = r.get("act", []) or []
        used = 0
        for i in range(nA):
            v = _f(a[i]) if i < len(a) else 0.0
            act_tot[i] += v
            if i != NOOP_IDX and v > 0:
                used += 1
        distinct_per_life.append(used)
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
        # anti-Goodhart — reward composition
        "reward_decomp": r_means,
        "reward_shares": r_shares,
        "r_potential_share": r_shares.get("r_potential", 0.0),
        "explored_m": explored_m,
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

    return {
        "ok": True, "project": "pe",
        "gen_last": int(_f(bl.get("generation", fl.get("generation", 0)))),
        "n_behavior_rows": len(behaviors), "n_fitness_rows": len(fitness),
        # 理解世界
        "mechanism_funnels": funnels,
        # 逻辑感
        "chain": chain, "chain_closed": chain_closed,
        # 不退化
        "niche_best": niche_best,
        "niche_occupancy": niche_occupancy,
        "specialist_vs_omnivore": specialist_vs_omnivore,
        "me_coverage": me_coverage,
        "me_qd_score": me_qd_score,
        "me_max_fitness": me_max_fitness,
        # 方向感 — fitness level
        "avg_fitness": round(_f(fl.get("avg_fitness")), 2),
        "best_fitness": round(_f(fl.get("best_fitness")), 2),
        # 真实
        "social": social,
        # survival
        "survival_ratio": survival_ratio,
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
