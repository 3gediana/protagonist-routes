"""Unit tests for the behavioural-spectrum derivation (pure, synthetic data)."""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

import spectrum as S  # noqa: E402


# --------------------------------------------------------------------------- #
# constants stay in lockstep with server.py (drift guard)
# --------------------------------------------------------------------------- #
def test_constants_match_server():
    import server
    assert S.ACTION_NAMES == server.ACTION_NAMES
    assert S.MILESTONE_NAMES == server.MILESTONE_NAMES
    assert [k for k, _ in S.DEATH_CAUSES] == [k for k, _ in server.DEATH_CAUSES]
    assert S.NOOP_IDX in server.UNTRACKED_ACT


# --------------------------------------------------------------------------- #
# entropy helper
# --------------------------------------------------------------------------- #
def test_norm_entropy_bounds():
    assert S._norm_entropy([]) == 0.0
    assert S._norm_entropy([5]) == 0.0           # single bin
    assert S._norm_entropy([3, 0, 0, 0]) == 0.0  # collapsed to one
    # uniform over all bins -> 1.0
    assert abs(S._norm_entropy([1, 1, 1, 1]) - 1.0) < 1e-9
    # partial spread is between 0 and 1
    h = S._norm_entropy([4, 1, 0, 0])
    assert 0.0 < h < 1.0


def _dp_row(act, unlocks, **kw):
    r = {"act": act, "unlocks": unlocks, "score": kw.get("score", 0.0),
         "deaths": kw.get("deaths", 0)}
    r.update(kw)
    return r


def test_dp_spectrum_basic():
    # two episodes: a narrow one and a broad one
    nA = len(S.ACTION_NAMES)
    narrow = [0] * nA
    narrow[2] = 100          # only HUNT
    broad = [0] * nA
    for i in (0, 1, 2, 5, 6, 18, 19):   # incl tech actions COOK(18)/MINE(19)
        broad[i] = 10
    rows = [
        _dp_row(narrow, [1, 1, 0, 1, 0, 0, 0, 0, 0, 0], score=0.3,
                deaths=5, deaths_cold=5, night_total=100, night_shelter=0),
        _dp_row(broad, [1, 1, 1, 1, 1, 1, 0, 0, 0, 0], score=0.5,
                deaths=0, deaths_cold=0, night_total=100, night_shelter=60,
                houses_built=1, sites_active=1, collect_total=20),
    ]
    spec = S.dp_spectrum(rows)
    assert spec["ok"] and spec["project"] == "dp" and spec["n"] == 2
    # milestone depth = avg(3,6)=4.5 ; breadth = union {0,1,2,3,4,5}=6
    assert abs(spec["milestone_depth"] - 4.5) < 1e-9
    assert spec["milestone_breadth"] == 6
    # tech tree: COOK + MINE alive -> 2/5
    assert spec["techtree_depth"] == 2
    # survival rate = 1 of 2
    assert abs(spec["survival_rate"] - 0.5) < 1e-9
    # night shelter ratio = 60 / 200
    assert abs(spec["night_shelter_ratio"] - 0.3) < 1e-9
    # chain closed via houses_built
    assert spec["chain_closed"] is True
    # repertoire/life = avg(1 distinct, 7 distinct) = 4.0
    assert abs(spec["repertoire_per_life"] - 4.0) < 1e-9
    # reward shares sum ~1 when any reward present (here none -> all 0)
    assert "r_potential_share" in spec


def test_dp_collapse_has_low_entropy():
    nA = len(S.ACTION_NAMES)
    collapsed = [0] * nA
    collapsed[2] = 500
    rows = [_dp_row(collapsed, [1, 0, 0, 0, 0, 0, 0, 0, 0, 0])]
    spec = S.dp_spectrum(rows)
    assert spec["action_entropy"] == 0.0
    assert spec["actions_alive"] == 1


def test_dp_axes_series_lengths():
    nA = len(S.ACTION_NAMES)
    rows = [_dp_row([1] * nA, [1, 1, 0, 0, 0, 0, 0, 0, 0, 0], score=float(i))
            for i in range(6)]
    ax = S.dp_axes(rows)
    assert len(ax["progress"]) == 6 and len(ax["breadth"]) == 6
    assert ax["score"][-1] == 5.0


# --------------------------------------------------------------------------- #
# PE
# --------------------------------------------------------------------------- #
def test_pe_spectrum_funnels_and_niches():
    behaviors = [{
        "generation": 40, "total_protagonists": 48, "survived_protagonists": 12,
        "chop_attempts": 1000, "chop_successes": 10,
        "craft_attempts": 500, "craft_successes": 5,
        "throw_attempts": 2000, "throw_hits": 760,
        "hunt_attempts": 3000, "hunt_successes": 0,
        "deposits_total": 50, "completed_worksites": 1,
        "worksite_completion_events": 1, "active_fires": 3,
        "drank_water_events": 0, "signal_emits": 100,
        "signal_response_events": 0, "signal_co_attendance": 0,
        "cooperative_co_presence_seconds": 5, "dnc_usage_mean_avg": 0.2,
    }]
    fitness = [{"generation": 40, "avg_fitness": 100, "best_fitness": 7000,
                "me_coverage": 10, "me_qd_score": 5000, "me_max_fitness": 7000}]
    evolution = [{"generation": 40, "omnivore_best": 7000,
                  "apex_predator_best": 100, "pack_hunter_best": 50,
                  "ambush_predator_best": 10, "large_herbivore_best": 200,
                  "small_forager_best": 300, "scavenger_best": 80}]
    spec = S.pe_spectrum(behaviors, fitness, evolution)
    assert spec["ok"] and spec["project"] == "pe"
    assert abs(spec["mechanism_funnels"]["throw"]["rate"] - 0.38) < 1e-9
    assert spec["mechanism_funnels"]["hunt"]["rate"] == 0.0
    assert spec["me_coverage"] == 10
    assert spec["chain_closed"] is True            # completed_worksites=1
    # omnivore dominates -> specialist/omnivore < 1 (wash-out)
    assert spec["specialist_vs_omnivore"] < 1.0
    assert spec["social"]["signal_response_rate"] == 0.0   # zero comms
    assert abs(spec["survival_ratio"] - 12 / 48) < 1e-9


def test_pe_spectrum_empty():
    assert S.pe_spectrum([], [], [])["ok"] is False


# --------------------------------------------------------------------------- #
# multi-axis verdict — the anti-Goodhart core
# --------------------------------------------------------------------------- #
IMPROVING = {"verdict": "improving"}
DECLINING = {"verdict": "declining"}
PLATEAU = {"verdict": "plateau"}
SHORT = {"verdict": "too_short"}


def test_verdict_extinct_reverts():
    v, _ = S.multi_axis_verdict(IMPROVING, IMPROVING, at_final=False, extinct=True)
    assert v == "revert"


def test_verdict_breadth_collapse_vetoes_progress():
    # progress improving BUT breadth collapsing -> revert (anti-Goodhart)
    v, why = S.multi_axis_verdict(IMPROVING, DECLINING, at_final=False)
    assert v == "revert" and "Goodhart" in why


def test_verdict_progress_decline_reverts():
    v, _ = S.multi_axis_verdict(DECLINING, PLATEAU, at_final=False)
    assert v == "revert"


def test_verdict_healthy_improvement_promotes():
    v, _ = S.multi_axis_verdict(IMPROVING, PLATEAU, at_final=False)
    assert v == "promote"


def test_verdict_improvement_at_final_done():
    v, _ = S.multi_axis_verdict(IMPROVING, IMPROVING, at_final=True)
    assert v == "done"


def test_verdict_plateau_promotes_midladder_done_final():
    assert S.multi_axis_verdict(PLATEAU, PLATEAU, at_final=False)[0] == "promote"
    assert S.multi_axis_verdict(PLATEAU, PLATEAU, at_final=True)[0] == "done"


def test_verdict_too_short_extends():
    assert S.multi_axis_verdict(SHORT, SHORT, at_final=False)[0] == "extend"


def test_summarize_runs():
    nA = len(S.ACTION_NAMES)
    spec = S.dp_spectrum([_dp_row([1] * nA, [1] * 10, score=0.4)])
    line = S.summarize(spec)
    assert "DP谱" in line
    assert S.summarize({"ok": False}) == "(谱不可用)"
