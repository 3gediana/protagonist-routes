"""Unit tests for the autonomous training-campaign controller + bounded
auto-tune + vetted-ladder curriculum advance (blueprint-level autonomy).

All pure / code-decided logic is exercised WITHOUT any live training:
  * _campaign_stages: per-stage scale, tier_c overridable.
  * _campaign_verdict: code-decided promote/revert/extend/done/stop from metrics
    (extinction & decline -> revert; plateau -> done at final else promote;
    too-few points -> extend; unreadable -> stop). The LLM cannot fabricate it.
  * _campaign_advance: promote carries the ladder forward & launches the next
    (bigger) stage; revert holds at the last good stage; done/stop terminate.
  * _tune_candidate_is_safe / _tune_pick_winner: tier-C safe-zone validation +
    winner selection (disqualifies extinct runs).
  * _curriculum_advance: autonomous only along a human-vetted ladder; otherwise
    falls back to the gated pe_curriculum_edit path.
  * registration: new tools scoped/visible correctly + forced kb_query.
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

import json  # noqa: E402

import agent  # noqa: E402
from agent import (  # noqa: E402
    _campaign_stages, _campaign_verdict, _tune_pick_winner, _tool_visible,
    _REQUIRE_KB_FIRST, CONCURRENCY_SAFE_TOOLS, TOOL_META,
)

CFG = {"max_procs": 10, "disk_low_gb": 190, "gpu_min_free_mb": 1500,
       "tier_c": {"reward_pct_limit": 0.25, "dp_smoke_steps": 200000,
                  "pe_smoke_generations": 4}}


def _trend(verdict):
    return {"verdict": verdict, "recent": 1.0, "max": 1.0}


# ------------------------------ stage scale -------------------------------- #
def test_dp_stage_scale_defaults_and_smoke_from_tier_c():
    s = _campaign_stages("dp", CFG["tier_c"])
    assert s["smoke"] == 200000 and s["short"] == 1_000_000 and s["long"] == 8_000_000


def test_pe_stage_scale_defaults():
    s = _campaign_stages("pe", CFG["tier_c"])
    assert s["smoke"] == 4 and s["short"] == 15 and s["long"] == 60


def test_stage_scale_override_via_tier_c():
    tc = dict(CFG["tier_c"], pe_ladder_generations={"long": 80})
    assert _campaign_stages("pe", tc)["long"] == 80


# ------------------------------ verdict logic ------------------------------ #
def test_verdict_unreadable_metrics_stops():
    v, _ = _campaign_verdict("pe", {"ok": False}, at_final=False)
    assert v == "stop"


def test_verdict_pe_extinction_reverts():
    am = {"ok": True, "extinct_hits": 2, "avg": _trend("improving")}
    v, _ = _campaign_verdict("pe", am, at_final=False)
    assert v == "revert"


def test_verdict_improving_promotes_then_done_at_final():
    am = {"ok": True, "extinct_hits": 0, "avg": _trend("improving"),
          "qd": _trend("plateau"), "me_cov": _trend("plateau")}
    assert _campaign_verdict("pe", am, at_final=False)[0] == "promote"
    assert _campaign_verdict("pe", am, at_final=True)[0] == "done"


def test_verdict_declining_reverts():
    am = {"ok": True, "extinct_hits": 0, "avg": _trend("declining"),
          "qd": _trend("improving"), "me_cov": _trend("plateau")}
    assert _campaign_verdict("pe", am, at_final=False)[0] == "revert"


def test_verdict_plateau_promotes_unless_final():
    am = {"ok": True, "extinct_hits": 0, "avg": _trend("plateau"),
          "qd": _trend("plateau"), "me_cov": _trend("plateau")}
    assert _campaign_verdict("pe", am, at_final=False)[0] == "promote"
    assert _campaign_verdict("pe", am, at_final=True)[0] == "done"


def test_verdict_too_short_extends():
    am = {"ok": True, "extinct_hits": 0, "avg": _trend("too_short"),
          "qd": _trend("too_short"), "me_cov": _trend("too_short")}
    assert _campaign_verdict("pe", am, at_final=False)[0] == "extend"


def test_verdict_dp_uses_score_reward_unlocks():
    am = {"ok": True, "score": _trend("declining"), "reward": _trend("improving"),
          "unlocks": _trend("plateau")}
    assert _campaign_verdict("dp", am, at_final=False)[0] == "revert"


# ---- multi-axis (panorama) path: takes precedence over coarse scalars ------ #
def _axes(progress, breadth):
    return {"axes_trend": {"progress": _trend(progress), "breadth": _trend(breadth)}}


def test_verdict_multiaxis_breadth_collapse_vetoes_progress():
    # progress improving but breadth collapsing -> revert (anti-Goodhart),
    # even though the legacy score/avg would have said promote.
    am = {"ok": True, "avg": _trend("improving"), "qd": _trend("improving"),
          "me_cov": _trend("improving"), **_axes("improving", "declining")}
    v, why = _campaign_verdict("pe", am, at_final=False)
    assert v == "revert" and "Goodhart" in why


def test_verdict_multiaxis_healthy_promotes():
    am = {"ok": True, **_axes("improving", "plateau")}
    assert _campaign_verdict("dp", am, at_final=False)[0] == "promote"


def test_verdict_multiaxis_extinct_still_reverts():
    am = {"ok": True, "extinct_hits": 2, **_axes("improving", "improving")}
    assert _campaign_verdict("pe", am, at_final=False)[0] == "revert"


# ---------------------------- advance orchestration ------------------------ #
def _setup_campaign_file(tmp_path, monkeypatch, camp):
    p = tmp_path / "camps.json"
    p.write_text(json.dumps([camp], ensure_ascii=False), encoding="utf-8")
    monkeypatch.setattr(agent, "CAMPAIGN_PATH", p)
    return p


def test_advance_promote_launches_next_stage(tmp_path, monkeypatch):
    camp = {"id": "c1", "project": "pe", "goal": "g", "stages": ["smoke", "short", "long"],
            "stage_idx": 0, "status": "running", "base_config": "onepot.toml",
            "base_checkpoint": None, "seed": None, "n_envs": 4, "history": [],
            "extends": 0, "last_run_id": "pe-1", "last_run_file": "onepot_x.log"}
    p = _setup_campaign_file(tmp_path, monkeypatch, camp)
    import server
    # run already ended (not in live processes)
    monkeypatch.setattr(server, "processes", lambda: {"processes": [], "total": 0})
    monkeypatch.setattr(agent, "_analyze_metrics",
                        lambda *a, **k: {"ok": True, "extinct_hits": 0,
                                         "avg": _trend("improving"),
                                         "qd": _trend("improving"),
                                         "me_cov": _trend("plateau"), "hint": "up"})
    monkeypatch.setattr(agent, "_guard_launch", lambda cfg: None)
    launched = {}
    def fake_pe_train(req):
        launched["gens"] = req.generations
        return {"ok": True, "run_id": "pe-2", "log": "/x/onepot_y.log"}
    monkeypatch.setattr(server, "pe_train", fake_pe_train)
    monkeypatch.setattr(server, "PETrainReq", lambda **kw: type("R", (), kw)())

    res = agent._campaign_advance("c1", CFG)
    assert res["ok"] and res["verdict"] == "promote"
    saved = json.loads(p.read_text(encoding="utf-8"))[0]
    assert saved["stage_idx"] == 1 and saved["status"] == "running"
    assert launched["gens"] == 15        # short stage scale
    assert saved["last_run_id"] == "pe-2"


def test_advance_revert_holds_at_previous_stage(tmp_path, monkeypatch):
    camp = {"id": "c2", "project": "pe", "stages": ["smoke", "short", "long"],
            "stage_idx": 1, "status": "running", "base_config": "onepot.toml",
            "history": [], "extends": 0, "last_run_id": "pe-9",
            "last_run_file": "onepot_z.log"}
    p = _setup_campaign_file(tmp_path, monkeypatch, camp)
    import server
    monkeypatch.setattr(server, "processes", lambda: {"processes": []})
    monkeypatch.setattr(agent, "_analyze_metrics",
                        lambda *a, **k: {"ok": True, "extinct_hits": 1,
                                         "avg": _trend("declining")})
    res = agent._campaign_advance("c2", CFG)
    assert res["verdict"] == "revert"
    saved = json.loads(p.read_text(encoding="utf-8"))[0]
    assert saved["stage_idx"] == 0 and saved["status"] == "reverted"


def test_advance_refuses_while_run_still_live(tmp_path, monkeypatch):
    camp = {"id": "c3", "project": "pe", "stages": ["smoke", "short", "long"],
            "stage_idx": 0, "status": "running", "last_run_id": "pe-live",
            "last_run_file": "f.log", "history": []}
    _setup_campaign_file(tmp_path, monkeypatch, camp)
    import server
    monkeypatch.setattr(server, "processes",
                        lambda: {"processes": [{"run_id": "pe-live"}]})
    res = agent._campaign_advance("c3", CFG)
    assert res["ok"] is False and "仍在训练" in res["error"]


def test_advance_done_at_final_plateau(tmp_path, monkeypatch):
    camp = {"id": "c4", "project": "dp", "stages": ["smoke", "short", "long"],
            "stage_idx": 2, "status": "running", "last_run_id": "dp-1",
            "last_run_file": "ft.jsonl", "history": []}
    p = _setup_campaign_file(tmp_path, monkeypatch, camp)
    import server
    monkeypatch.setattr(server, "processes", lambda: {"processes": []})
    monkeypatch.setattr(agent, "_analyze_metrics",
                        lambda *a, **k: {"ok": True, "score": _trend("plateau"),
                                         "reward": _trend("plateau"),
                                         "unlocks": _trend("plateau")})
    res = agent._campaign_advance("c4", CFG)
    assert res["verdict"] == "done"
    assert json.loads(p.read_text(encoding="utf-8"))[0]["status"] == "done"


# ------------------------------ auto-tune ---------------------------------- #
def test_tune_candidate_safe_vs_structural(monkeypatch):
    import server
    def fake_tune(req):
        # echo a delta keyed on the proposed value's sign/magnitude
        val = list(req.edits.values())[0]
        delta = {"numeric": True, "sign_flip": val < 0,
                 "abs_pct_change": 0.1 if abs(val) <= 1.2 else 0.9}
        return {"ok": True, "applied": [{"knob": list(req.edits)[0], "delta": delta}]}
    monkeypatch.setattr(server, "pe_tune", fake_tune)
    monkeypatch.setattr(server, "PETuneReq", lambda **kw: type("R", (), kw)())
    ok, _ = agent._tune_candidate_is_safe("c.toml", "reward.food_weight", 1.1, 0.25)
    assert ok is True
    bad, why = agent._tune_candidate_is_safe("c.toml", "reward.food_weight", -1.0, 0.25)
    assert bad is False and why
    big, _ = agent._tune_candidate_is_safe("c.toml", "reward.food_weight", 5.0, 0.25)
    assert big is False


def test_tune_pick_winner_pe_disqualifies_extinct():
    cmp = {"rows": [
        {"file": "a.log", "ok": True, "avg_recent": 5.0, "qd_recent": 1.0, "extinct_hits": 0},
        {"file": "b.log", "ok": True, "avg_recent": 9.0, "qd_recent": 1.0, "extinct_hits": 3},
        {"file": "c.log", "ok": True, "avg_recent": 6.0, "qd_recent": 2.0, "extinct_hits": 0},
    ]}
    assert _tune_pick_winner("pe", cmp) == "c.log"   # b is highest but extinct -> out


def test_tune_pick_winner_none_when_no_ok_rows():
    assert _tune_pick_winner("pe", {"rows": [{"file": "a", "ok": False}]}) is None


# --------------------------- curriculum advance ---------------------------- #
def test_curriculum_advance_gated_without_ladder(tmp_path, monkeypatch):
    monkeypatch.setattr(agent, "CURRICULUM_LADDER_PATH", tmp_path / "nope.json")
    res = agent._curriculum_advance("onepot.toml", CFG, reason="r")
    assert res["ok"] is False and res.get("gated") is True


def test_curriculum_advance_walks_vetted_ladder(tmp_path, monkeypatch):
    ladder = {"stages": [
        {"name": "s0", "edits": {"evolution.task_templates.0.weight": 1.0}},
        {"name": "s1", "edits": {"evolution.task_templates.0.weight": 1.5}},
    ]}
    lp = tmp_path / "ladder.json"
    lp.write_text(json.dumps(ladder), encoding="utf-8")
    monkeypatch.setattr(agent, "CURRICULUM_LADDER_PATH", lp)
    monkeypatch.setattr(agent, "CURRICULUM_STATE_PATH", tmp_path / "state.json")
    import server
    calls = []
    monkeypatch.setattr(server, "pe_tune",
                        lambda req: calls.append(req.edits) or {"ok": True, "variant": "v"})
    monkeypatch.setattr(server, "PETuneReq", lambda **kw: type("R", (), kw)())
    r1 = agent._curriculum_advance("onepot.toml", CFG, reason="r")
    assert r1["ok"] and r1["advanced_to"] == 0
    r2 = agent._curriculum_advance("onepot.toml", CFG, reason="r")
    assert r2["ok"] and r2["advanced_to"] == 1
    r3 = agent._curriculum_advance("onepot.toml", CFG, reason="r")
    assert r3["ok"] is False and "顶端" in r3["error"]
    assert len(calls) == 2


# ------------------------------ registration ------------------------------- #
def test_new_tools_registered_and_scoped():
    assert TOOL_META["campaign_status"]["project"] == "any"
    assert TOOL_META["start_campaign"]["project"] == "shared"
    assert TOOL_META["advance_campaign"]["project"] == "shared"
    assert TOOL_META["start_tune"]["project"] == "pe"
    assert TOOL_META["curriculum_advance"]["project"] == "pe"


def test_campaign_tools_visibility():
    # campaign_status resident; start/advance need a focus (shared)
    assert _tool_visible("campaign_status", None, set()) is True
    assert _tool_visible("start_campaign", None, set()) is False
    assert _tool_visible("start_campaign", "dp", set()) is True
    assert _tool_visible("start_campaign", "pe", set()) is True
    # start_tune is in pe_reward pack -> hidden until activated, pe-only
    assert _tool_visible("start_tune", "pe", set()) is False
    assert _tool_visible("start_tune", "pe", {"pe_reward"}) is True
    assert _tool_visible("start_tune", "dp", {"pe_reward"}) is False


def test_autonomy_tools_require_kb_first():
    for nm in ("start_campaign", "advance_campaign", "start_tune",
               "resolve_tune", "curriculum_advance"):
        assert nm in _REQUIRE_KB_FIRST


def test_campaign_status_is_concurrency_safe():
    assert "campaign_status" in CONCURRENCY_SAFE_TOOLS
