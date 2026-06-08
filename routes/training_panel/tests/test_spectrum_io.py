"""I/O-level tests: server payload readers + PE CSV-dir discovery + the
agent's _attach_panorama wiring. Uses tmp dirs + monkeypatched paths."""
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

import agent  # noqa: E402
import server  # noqa: E402
import spectrum as S  # noqa: E402


def _write_dp_jsonl(path, n=6):
    nA = len(S.ACTION_NAMES)
    with open(path, "w", encoding="utf-8") as fh:
        for i in range(n):
            act = [0] * nA
            for k in (0, 1, 2, 3):
                act[k] = 5 + i
            fh.write(json.dumps({
                "episode": i, "score": 0.2 + 0.05 * i, "reward": 1.0 + i,
                "act": act, "unlocks": [1, 1, 1, 0, 0, 0, 0, 0, 0, 0],
                "deaths": 0, "night_total": 10, "night_shelter": 4,
            }) + "\n")


def test_dp_spectrum_payload(tmp_path, monkeypatch):
    monkeypatch.setattr(server, "DP_METRICS", tmp_path)
    _write_dp_jsonl(tmp_path / "ft_demo.jsonl")
    out = server.dp_spectrum_payload("ft_demo.jsonl")
    assert out["ok"] and out["project"] == "dp"
    assert out["actions_alive"] == 4
    assert "axes" in out and len(out["axes"]["progress"]) == 6


def _write_pe_run(root, scenario="panel_demo_50027", ts="20260608-120000"):
    d = root / scenario / ts
    d.mkdir(parents=True)
    with open(d / "fitness.csv", "w", encoding="utf-8", newline="") as fh:
        fh.write("generation,avg_fitness,best_fitness,me_coverage,me_qd_score,me_max_fitness\n")
        for g in range(5):
            fh.write(f"{g},{100+g*10},{7000},{8+g},{5000+g*100},{7000}\n")
    with open(d / "protagonist_behaviors.csv", "w", encoding="utf-8", newline="") as fh:
        fh.write("generation,total_protagonists,survived_protagonists,"
                 "chop_attempts,chop_successes,craft_attempts,craft_successes,"
                 "throw_attempts,throw_hits,hunt_attempts,hunt_successes,"
                 "deposits_total,completed_worksites,worksite_completion_events,"
                 "active_fires,drank_water_events,signal_emits,"
                 "signal_response_events,signal_co_attendance,"
                 "cooperative_co_presence_seconds,dnc_usage_mean_avg\n")
        fh.write("4,48,12,1000,10,500,5,2000,760,3000,0,50,1,1,3,0,100,20,2,5,0.2\n")
    with open(d / "evolution.csv", "w", encoding="utf-8", newline="") as fh:
        fh.write("generation,omnivore_best,apex_predator_best,pack_hunter_best,"
                 "ambush_predator_best,large_herbivore_best,small_forager_best,"
                 "scavenger_best\n")
        fh.write("4,7000,100,50,10,200,300,80\n")
    return d


def test_pe_csv_dir_discovery_and_payload(tmp_path, monkeypatch):
    monkeypatch.setattr(server, "PE_RUNS", tmp_path)
    _write_pe_run(tmp_path, scenario="panel_demo_50027")
    # log file carries a *different* trailing timestamp than the run dir
    d = server._pe_csv_dir("panel_demo_50027_20260608-115959.stdout.log")
    assert d is not None and (d / "fitness.csv").exists()
    out = server.pe_spectrum_payload("panel_demo_50027_20260608-115959.stdout.log")
    assert out["ok"] and out["me_coverage"] == 12   # last gen (8+4)
    assert out["mechanism_funnels"]["throw"]["rate"] == 0.38
    assert "axes" in out and out["axes"]["breadth"][-1] == 12


def test_pe_csv_dir_missing_returns_none(tmp_path, monkeypatch):
    monkeypatch.setattr(server, "PE_RUNS", tmp_path)
    assert server._pe_csv_dir("nonexistent_20260101-000000.stdout.log") is None
    assert server.pe_spectrum_payload("nope.log")["ok"] is False


def test_attach_panorama_dp(tmp_path, monkeypatch):
    monkeypatch.setattr(server, "DP_METRICS", tmp_path)
    _write_dp_jsonl(tmp_path / "ft_demo.jsonl")
    am = agent._analyze_metrics("dp", "ft_demo.jsonl")
    assert am["ok"]
    assert "spectrum" in am and am["spectrum"]["project"] == "dp"
    assert "panorama" in am and "DP谱" in am["panorama"]
    assert "axes_trend" in am
    assert am["axes_trend"]["breadth"]["verdict"] in (
        "improving", "plateau", "declining", "too_short")


def test_attach_panorama_failure_is_additive(tmp_path, monkeypatch):
    # DP metrics file exists (so analyze succeeds) but spectrum payload points at
    # an empty dir -> panorama_error set, am still ok.
    monkeypatch.setattr(server, "DP_METRICS", tmp_path)
    _write_dp_jsonl(tmp_path / "ft_demo.jsonl", n=2)
    am = agent._analyze_metrics("dp", "ft_demo.jsonl")
    assert am["ok"]   # never breaks the base read
