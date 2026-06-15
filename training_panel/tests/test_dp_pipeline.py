"""Unit tests for the DP pipeline argv builders + PE tune delta logic.

These verify that the orchestration endpoints reconstruct EXACTLY the CLI the
human handoff scripts used (bc_then_ppo.ps1 / train_hunt.ps1 / train_repertoire.ps1),
so the agent's expanded action space drives the binary the proven way — no source
or dimension changes, just the existing CLI switches.
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

import server  # noqa: E402
from server import (  # noqa: E402
    DPDemosReq, DPBCReq, DPDaggerReq, DPFinetuneReq,
    _dp_demos_argv, _dp_bc_argv, _dp_dagger_argv, _dp_finetune_argv,
    _knob_delta,
)

EXE = server.DP_ROOT / "build_d122_v2" / "bin" / "deep_protagonist_train.exe"


def _kv(argv, flag):
    """Return the value following `flag`, or None if flag is absent."""
    return argv[argv.index(flag) + 1] if flag in argv else None


def test_record_demos_matches_handoff():
    argv, demos, metrics = _dp_demos_argv(EXE, DPDemosReq(tag="hv2", seed=24))
    assert _kv(argv, "--policy") == "settler"
    assert "--weather" in argv and "--allow-cook" in argv
    assert _kv(argv, "--n-envs") == "1"
    assert _kv(argv, "--steps") == "2000000"
    assert _kv(argv, "--seed") == "24"
    assert _kv(argv, "--record-demos") == demos == os.path.join("traces", "demos_hv2_s24.bin")
    assert metrics == os.path.join("metrics", "rec_hv2.jsonl")


def test_record_demos_optional_flags():
    argv, _, _ = _dp_demos_argv(EXE, DPDemosReq(
        teacher="scripted", demo_no_regen=True, oracle_no_fire=True,
        weather=False, allow_cook=False, episodes=50, steps=None))
    assert _kv(argv, "--policy") == "scripted"
    assert "--weather" not in argv and "--allow-cook" not in argv
    assert "--steps" not in argv
    assert _kv(argv, "--episodes") == "50"
    assert "--demo-no-regen" in argv and "--oracle-no-fire" in argv


def test_train_bc_matches_handoff():
    argv, bc_out = _dp_bc_argv(EXE, DPBCReq(demos="demos_hv2_s24.bin", tag="hv2"))
    assert _kv(argv, "--policy") == "bc"
    # demo name routed to the binary: bare passthrough when the file is absent,
    # DP_ROOT-relative (traces/...) when it exists on the box -> assert basename.
    assert os.path.basename(_kv(argv, "--bc-demos")) == "demos_hv2_s24.bin"
    assert _kv(argv, "--bc-out") == bc_out == os.path.join("checkpoints", "bc_hv2.pt")
    assert _kv(argv, "--bc-epochs") == "10"
    assert _kv(argv, "--bc-batch") == "16"
    assert _kv(argv, "--bc-bptt") == "64"
    assert _kv(argv, "--bc-lr") == "0.001"
    assert _kv(argv, "--bc-trigger-weight") == "5.0"
    assert _kv(argv, "--bc-min-eplen") == "300"
    assert _kv(argv, "--bc-trigger-oversample") == "3"


def test_train_bc_multi_demo_csv():
    argv, _ = _dp_bc_argv(EXE, DPBCReq(demos="a.bin, b.bin ,c.bin", tag="x"))
    assert _kv(argv, "--bc-demos") == "a.bin,b.bin,c.bin"


def test_dagger_invocation_is_source_confirmed():
    # source: --policy ppo --load <clone> --record-demos <out> --dagger
    argv, demos = _dp_dagger_argv(EXE, DPDaggerReq(clone="bc_hv2.pt", tag="d1", seed=24))
    assert _kv(argv, "--policy") == "ppo"
    assert os.path.basename(_kv(argv, "--load")) == "bc_hv2.pt"
    assert "--dagger" in argv
    assert _kv(argv, "--record-demos") == demos == os.path.join("traces", "dagger_d1_s24.bin")
    # dagger is a record loop: must NOT carry a PPO save-path
    assert "--save-path" not in argv


def test_finetune_matches_train_hunt():
    argv, save, metrics = _dp_finetune_argv(
        EXE, DPFinetuneReq(load="bc_hv2.pt", tag="bc05", hunt_shaping=0.5), tag="bc05_T")
    assert _kv(argv, "--policy") == "ppo"
    assert os.path.basename(_kv(argv, "--load")) == "bc_hv2.pt"
    assert "--weather" in argv and "--allow-cook" in argv
    assert _kv(argv, "--ent-coef") == "0.0015"
    assert _kv(argv, "--critic-warmup") == "32"
    assert _kv(argv, "--lr") == "0.00015"          # 1.5e-4
    assert _kv(argv, "--ppo-target-kl") == "0.08"
    assert _kv(argv, "--ppo-hunt-shaping") == "0.5"
    assert _kv(argv, "--save-every") == "25"
    assert save == os.path.join("checkpoints", "ft_bc05_T.pt")
    assert metrics == os.path.join("metrics", "ft_bc05_T.jsonl")


def test_finetune_repertoire_shaping_levers():
    argv, _, _ = _dp_finetune_argv(EXE, DPFinetuneReq(
        load="ft_bc10.pt", repertoire_shaping=1.5, hunt_shaping=0.3,
        warmth_shaping=0.0, ent_coef=0.01), tag="rep")
    assert _kv(argv, "--ppo-repertoire-shaping") == "1.5"
    assert _kv(argv, "--ppo-hunt-shaping") == "0.3"
    assert _kv(argv, "--ppo-warmth-shaping") == "0.0"
    assert _kv(argv, "--ent-coef") == "0.01"


def test_finetune_omits_unset_shaping():
    argv, _, _ = _dp_finetune_argv(EXE, DPFinetuneReq(load="bc_hv2.pt"), tag="t")
    for flag in ("--ppo-hunt-shaping", "--ppo-repertoire-shaping",
                 "--ppo-warmth-shaping", "--thirst-decay", "--trigger-cost"):
        assert flag not in argv


def test_knob_delta_sign_and_magnitude():
    # +25% bump, same sign -> small, no flip
    d = _knob_delta("1.0", 1.25)
    assert d["numeric"] and not d["sign_flip"] and abs(d["abs_pct_change"] - 0.25) < 1e-9
    # sign flip
    assert _knob_delta("0.5", -0.5)["sign_flip"] is True
    # doubling -> 100% change
    assert abs(_knob_delta("2", 4)["abs_pct_change"] - 1.0) < 1e-9
    # non-numeric (e.g. bool/string knob)
    assert _knob_delta("true", False)["numeric"] is False
