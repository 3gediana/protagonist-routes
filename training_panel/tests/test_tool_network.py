"""Unit tests for the project-partitioned hybrid tool network + tier-C gate.

Covers (pure, no live training):
  * _tool_visible: focus=None hides actions; focus scopes project tools; long-tail
    packs stay hidden until activated; resident/shared always visible under focus.
  * _find_tools: keyword/pack discovery activates the right pack (focus-scoped).
  * focus serialization: switching focus clears activated packs; wrong-focus
    action tools are refused at the code level.
  * tier-C classifier (_needs_approval / _is_new_bloodline / _delta_is_structural):
    low-risk autonomous, high-risk (curriculum / new bloodline / structural reward)
    queued for approval even in auto mode.
  * forced behavior helpers: _kb_query_in_recent, _REQUIRE_KB_FIRST membership.
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

import agent  # noqa: E402
from agent import (  # noqa: E402
    _tool_visible, _find_tools, _needs_approval, _is_new_bloodline,
    _delta_is_structural, _kb_query_in_recent, _REQUIRE_KB_FIRST,
    LONGTAIL_PACKS, TOOL_META, STATE,
)

CFG = {"tier_c": {"reward_pct_limit": 0.25, "dp_smoke_steps": 200000,
                  "pe_smoke_generations": 4}}


# --------------------------- tool visibility ------------------------------- #
def test_resident_tools_visible_without_focus():
    for nm in ("get_status", "focus_project", "find_tools", "kb_query", "sleep"):
        assert _tool_visible(nm, None, set()) is True


def test_actions_hidden_until_focus_set():
    # no focus -> project + shared action tools are hidden (force focus_project)
    assert _tool_visible("start_dp_training", None, set()) is False
    assert _tool_visible("start_pe_training", None, set()) is False
    assert _tool_visible("analyze_metrics", None, set()) is False


def test_focus_scopes_project_tools():
    assert _tool_visible("start_dp_training", "dp", set()) is True
    assert _tool_visible("start_dp_training", "pe", set()) is False
    assert _tool_visible("start_pe_training", "pe", set()) is True
    assert _tool_visible("start_pe_training", "dp", set()) is False
    # shared reads visible under either focus
    assert _tool_visible("analyze_metrics", "dp", set()) is True
    assert _tool_visible("analyze_metrics", "pe", set()) is True


def test_longtail_hidden_until_activated():
    # dp_pipeline tools invisible under dp focus until the pack is activated
    assert _tool_visible("dp_train_bc", "dp", set()) is False
    assert _tool_visible("dp_train_bc", "dp", {"dp_pipeline"}) is True
    # pe reward pack
    assert _tool_visible("pe_reward_tune", "pe", set()) is False
    assert _tool_visible("pe_reward_tune", "pe", {"pe_reward"}) is True
    # activating a pack under the WRONG focus still doesn't reveal it
    assert _tool_visible("dp_train_bc", "pe", {"dp_pipeline"}) is False


# ------------------------------ find_tools --------------------------------- #
def test_find_tools_requires_focus():
    res = _find_tools("bc", "", None)
    assert res["ok"] is False


def test_find_tools_keyword_activates_dp_pipeline():
    res = _find_tools("bc", "", "dp")
    assert res["ok"] is True
    assert "dp_pipeline" in res["activated"]
    names = {t["name"] for t in res["tools"]}
    assert {"dp_record_demos", "dp_train_bc", "dp_dagger", "dp_finetune"} <= names


def test_find_tools_is_focus_scoped():
    # a pe keyword under dp focus must not activate a pe pack
    res = _find_tools("reward", "", "dp")
    assert "pe_reward" not in res["activated"]


def test_find_tools_by_pack_name():
    res = _find_tools("", "pe_curriculum", "pe")
    assert "pe_curriculum" in res["activated"]


# -------------------------- focus serialization ---------------------------- #
def test_focus_switch_clears_packs_and_refuses_cross_project():
    STATE.focus_project = None
    STATE.active_packs = set()
    # focus dp + activate pipeline
    agent._execute_tool("focus_project", {"project": "dp"}, CFG)
    _find_tools("bc", "", "dp")
    assert STATE.focus_project == "dp" and "dp_pipeline" in STATE.active_packs
    # a PE action under DP focus is refused at the code level
    res = agent._execute_tool("pe_reward_tune",
                              {"config": "x.toml", "edits": {"reward.food_weight": 1.0}}, CFG)
    assert res.get("refused") is True and res.get("focus_required") == "pe"
    # switching focus clears the activated packs (packs are project-scoped)
    agent._execute_tool("focus_project", {"project": "pe"}, CFG)
    assert STATE.focus_project == "pe" and STATE.active_packs == set()
    STATE.focus_project = None


def test_focus_project_rejects_bad_value():
    res = agent._execute_tool("focus_project", {"project": "nope"}, CFG)
    assert res.get("ok") is False


# ------------------------------ tier-C gate -------------------------------- #
def test_delta_is_structural_rules():
    assert _delta_is_structural({"numeric": True, "sign_flip": True,
                                 "abs_pct_change": 0.1}, 0.25) is True
    assert _delta_is_structural({"numeric": True, "sign_flip": False,
                                 "abs_pct_change": 0.5}, 0.25) is True   # >25%
    assert _delta_is_structural({"numeric": True, "sign_flip": False,
                                 "abs_pct_change": 0.1}, 0.25) is False  # small
    assert _delta_is_structural({"numeric": False}, 0.25) is True        # non-numeric
    assert _delta_is_structural({"numeric": True, "sign_flip": False,
                                 "abs_pct_change": None}, 0.25) is True   # old==0


def test_curriculum_always_needs_approval():
    need, why = _needs_approval("pe_curriculum_edit",
                                {"config": "x.toml", "edits": {}}, CFG)
    assert need is True and why


def test_low_risk_dp_pipeline_is_autonomous():
    # using an EXISTING teacher/base for BC/DAgger/finetune is low-risk
    for nm, args in (("dp_train_bc", {"demos": "d.bin"}),
                     ("dp_record_demos", {"teacher": "settler"}),
                     ("dp_dagger", {"clone": "bc.pt"}),
                     ("dp_finetune", {"load": "bc.pt", "hunt_shaping": 0.5})):
        need, _ = _needs_approval(nm, args, CFG)
        assert need is False, nm


def test_new_bloodline_needs_approval():
    # DP: no base checkpoint + steps beyond smoke -> new bloodline
    assert _is_new_bloodline("start_dp_training", {"steps": 8_000_000}, CFG)
    # DP smoke (short) off no base -> allowed
    assert _is_new_bloodline("start_dp_training", {"steps": 100_000}, CFG) is None
    # DP resuming a base ckpt -> not a new bloodline regardless of length
    assert _is_new_bloodline("start_dp_training",
                             {"checkpoint": "ft.pt", "steps": 8_000_000}, CFG) is None
    # PE: no resume + generations beyond smoke -> new bloodline
    assert _is_new_bloodline("start_pe_training", {"generations": 30}, CFG)
    assert _is_new_bloodline("start_pe_training", {"generations": 3}, CFG) is None
    assert _is_new_bloodline("start_pe_training",
                             {"resume_checkpoint": "g.ckpt", "generations": 30}, CFG) is None


def test_needs_approval_new_bloodline_via_classifier():
    need, why = _needs_approval("start_pe_training", {"generations": 30}, CFG)
    assert need is True and "血脉" in why


# --------------------------- forced behavior ------------------------------- #
def test_require_kb_first_membership():
    assert {"start_dp_training", "start_pe_training", "dp_train_bc",
            "pe_reward_tune", "pe_curriculum_edit"} <= _REQUIRE_KB_FIRST


def test_kb_query_in_recent_detects_call():
    msgs = [{"role": "assistant", "tool_calls": [
        {"function": {"name": "kb_query", "arguments": "{}"}}]}]
    assert _kb_query_in_recent(msgs) is True
    msgs2 = [{"role": "assistant", "tool_calls": [
        {"function": {"name": "get_status", "arguments": "{}"}}]}]
    assert _kb_query_in_recent(msgs2) is False


# --------------------------- metadata integrity ---------------------------- #
def test_every_meta_pack_is_known():
    for nm, meta in TOOL_META.items():
        pk = meta.get("pack")
        if pk is not None:
            assert pk in LONGTAIL_PACKS, nm


# --------------------------- launch resource guard ------------------------- #
GCFG = {"max_procs": 10, "disk_low_gb": 190, "gpu_min_free_mb": 1500}


def _patch_status(monkeypatch, *, total=0, free_gb=400, gpu=None):
    import server
    monkeypatch.setattr(server, "processes", lambda: {"total": total,
                        "dp_count": 0, "pe_count": 0})
    monkeypatch.setattr(server, "system_status",
                        lambda: {"disk": {"free_gb": free_gb}, "gpu": gpu})


def test_guard_refuses_when_vram_headroom_too_low(monkeypatch):
    _patch_status(monkeypatch, gpu={"util_pct": 96, "mem_used_mb": 7800,
                                    "mem_total_mb": 8151})  # ~351MB free
    reason = agent._guard_launch(GCFG)
    assert reason and "显存" in reason


def test_guard_allows_when_vram_has_headroom(monkeypatch):
    _patch_status(monkeypatch, gpu={"util_pct": 2, "mem_used_mb": 2300,
                                    "mem_total_mb": 8151})  # ~5.8GB free
    assert agent._guard_launch(GCFG) is None


def test_guard_multi_gpu_uses_freest(monkeypatch):
    # one GPU is full, another is mostly free -> allow (run lands on the freest)
    _patch_status(monkeypatch, gpu=[{"mem_used_mb": 7900, "mem_total_mb": 8151},
                                    {"mem_used_mb": 1000, "mem_total_mb": 8151}])
    assert agent._guard_launch(GCFG) is None


def test_guard_proc_cap_takes_precedence(monkeypatch):
    _patch_status(monkeypatch, total=10, gpu={"mem_used_mb": 100,
                                              "mem_total_mb": 8151})
    reason = agent._guard_launch(GCFG)
    assert reason and "上限" in reason


def test_guard_no_gpu_reading_does_not_block(monkeypatch):
    # nvidia-smi unavailable -> gpu None -> VRAM check is skipped, not a refusal
    _patch_status(monkeypatch, gpu=None)
    assert agent._guard_launch(GCFG) is None
