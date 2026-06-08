#!/usr/bin/env python3
"""training_panel — shared DP/PE training management web panel.

A local control panel (runs on the training Windows box, localhost only) that
wraps the deep_protagonist (DP) and protagonist_ecology (PE) training tooling.
It lets you:

  * launch / stop training with adjustable params + seeds (TRUE parallel: many
    concurrent runs, keyed by run-id, no per-project lock)
  * discover ALL training processes on the box (panel-launched AND external,
    e.g. the batch sweep started from PowerShell), with pid / args / uptime
  * browse and clear logs / checkpoints (safe move-to-trash default; red-line
    assets protected, matching the repo's own prune conventions)
  * list and clean up replay traces (PE per-episode traces + DP dump dirs)
  * parse training jsonl into live metrics (ported from scripts/dp_dashboard.py)
  * "new world, same model": run an inference-only rollout of a chosen
    checkpoint in a fresh seed/world and replay the resulting trace
  * pick a trace and load it into the existing web3d 3D viewers
  * watch GPU / disk / process status

It NEVER touches training core logic or game mechanics; it only spawns the
existing executables and reads/moves files under the two project trees.

Real layout discovered on the training box (paths auto-derive from this file):
  <repo>/routes/training_panel/server.py           <- this file
  <repo>/routes/deep_protagonist/                  <- DP project
      build_d122_v2/bin/deep_protagonist_train.exe <- DP training
      build_d122_v2/bin/deep_protagonist_dump.exe  <- DP new-world dumper (opt)
      runs/                                         <- *.pt / *.jsonl / *.log
      viewer/web3d/                                 <- DP 3D viewer
  <repo>/routes/protagonist_ecology/               <- PE project
      *.toml                                        <- PE configs
      data/runs/<scenario>/<ts>/trace/...           <- PE traces
      viewer/web3d/                                 <- PE 3D viewer
  <repo>/build_ninja_cuda/bin/neural-eco-protagonist-batched.exe  <- PE training

Run:  python -m uvicorn server:app --host 127.0.0.1 --port 8070
  or:  ./start_panel.ps1
"""
from __future__ import annotations

import asyncio
import json
import os
import re
import shutil
import signal
import subprocess
import threading
import time
from dataclasses import dataclass, field
from datetime import datetime
from pathlib import Path
from typing import Any, Optional

try:
    import tomllib  # py3.11+
except ModuleNotFoundError:  # pragma: no cover
    tomllib = None  # type: ignore

try:
    import psutil  # cross-platform process discovery
except ModuleNotFoundError:  # pragma: no cover
    psutil = None  # type: ignore

from fastapi import FastAPI, HTTPException, WebSocket, WebSocketDisconnect
from fastapi.responses import FileResponse
from fastapi.staticfiles import StaticFiles
from pydantic import BaseModel

import spectrum  # behavioural-panorama derivation (pure)

# --------------------------------------------------------------------------- #
# Paths — auto-derived from this file's location.
# --------------------------------------------------------------------------- #
PANEL_DIR = Path(__file__).resolve().parent
ROUTES_DIR = PANEL_DIR.parent                       # .../routes
REPO_ROOT = ROUTES_DIR.parent                       # .../  (monorepo root)
DP_ROOT = ROUTES_DIR / "deep_protagonist"
PE_ROOT = ROUTES_DIR / "protagonist_ecology"

DP_RUNS = DP_ROOT / "runs"               # legacy run scratch / markers
DP_CKPT = DP_ROOT / "checkpoints"         # *.pt / *.pt.opt models
DP_LOGS = DP_ROOT / "logs"                # *.stdout.log / *.stderr.log
DP_METRICS = DP_ROOT / "metrics"          # *.jsonl training metrics
DP_TRACES = DP_ROOT / "traces"            # viewer dump dirs + demo bins
DP_WEB3D = DP_ROOT / "viewer" / "web3d"
DP_BIN = DP_ROOT / "build_d122_v2" / "bin"

PE_RUNS = PE_ROOT / "runs"               # per-run dirs (genomes+csv+trace/)
PE_WEB3D = PE_ROOT / "viewer" / "web3d"
PE_PANEL_LOGS = PE_ROOT / "logs"          # all PE console logs consolidated

TRASH_DIR = PANEL_DIR / "_trash"                    # safe deletes go here

# CUDA runtime the PE exe needs on PATH (from clone_pe.ps1). Only prepended if
# the directory actually exists, so it is a no-op off the training box.
CUDA_BIN = r"D:\NVIDIA\CUDA\v12.8\bin"

# DP executables (resolved lazily; *_train is training, dump is new-world).
DP_TRAIN_EXE = [DP_BIN / "deep_protagonist_train.exe",
                DP_ROOT / "build" / "bin" / "deep_protagonist_train.exe"]
DP_DUMP_EXE = [DP_BIN / "deep_protagonist_dump.exe",
               DP_ROOT / "build" / "bin" / "deep_protagonist_dump.exe"]

# PE executable: lives at <repo>/build_ninja_cuda/bin (NOT under PE_ROOT).
# `-batched` is the variant actually used for the parallel sweep.
PE_EXE = [REPO_ROOT / "build_ninja_cuda" / "bin" / "neural-eco-protagonist-batched.exe",
          REPO_ROOT / "build_ninja_cuda" / "bin" / "neural-eco-protagonist.exe"]

# Process image names used to discover training processes (panel + external).
DP_TRAIN_IMAGES = {"deep_protagonist_train.exe", "deep_protagonist_train"}
PE_IMAGE_PREFIX = "neural-eco-protagonist"          # matches all PE variants

# --------------------------------------------------------------------------- #
# Protected (red-line) assets — must NEVER be deletable from the panel.
# Mirrors scripts/prune_runs.ps1 (gold ckpts) + cleanup_logs.ps1 (bloodline).
# --------------------------------------------------------------------------- #
DP_PROTECTED_NAMES = {
    "ft_gen.pt", "bc_gen.pt", "bc_hv2.pt",
}
# Gold DP iterations kept by prune_runs.ps1, plus generic protective patterns,
# plus the bloodline metrics record. Matched case-insensitively.
PROTECTED_PATTERNS = [re.compile(p, re.I) for p in (
    r"^ppo_d(061|065|076)_",     # gold checkpoints (prune_runs.ps1 goldKeep)
    r"^r1_cook_s24",             # bloodline empirical record
    r"_red\b", r"_final\b", r"_champ\b", r"_keep\b", r"_gold\b",
    r"\.bak",                    # manual-hide files
)]

# --------------------------------------------------------------------------- #
# Authoritative label tables (kept in sync with dp_dashboard.py / C++ source).
# --------------------------------------------------------------------------- #
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
UNTRACKED_ACT = {16}  # NOOP never tracked
DEATH_CAUSES = [
    ("deaths_cold", "cold"), ("deaths_food", "food"),
    ("deaths_protein", "protein"), ("deaths_vitamin", "vitamin"),
]
WANDER_REWARD = -37.38  # docs/BASELINE.md wander reference


# --------------------------------------------------------------------------- #
# Run registry — TRUE parallel: many concurrent runs keyed by run-id.
# --------------------------------------------------------------------------- #
@dataclass
class TrainRun:
    run_id: str
    project: str                     # "dp" | "pe"
    proc: subprocess.Popen
    cmd: str
    log_path: Path
    kind: str = "train"              # "train" | "newworld"
    meta: dict[str, Any] = field(default_factory=dict)
    started: float = field(default_factory=time.time)
    lines: list[str] = field(default_factory=list)   # ring buffer

    @property
    def running(self) -> bool:
        return self.proc.poll() is None


RUNS: dict[str, TrainRun] = {}          # run_id -> TrainRun
RING_MAX = 4000
_RUN_SEQ = 0


def _new_run_id(project: str) -> str:
    global _RUN_SEQ
    _RUN_SEQ += 1
    return f"{project}-{datetime.now().strftime('%H%M%S')}-{_RUN_SEQ}"


def _resolve(cands: list[Path]) -> Optional[Path]:
    for c in cands:
        if c.exists():
            return c
    return None


def _safe_join(base: Path, rel: str) -> Path:
    """Resolve `rel` under `base`, refusing path traversal."""
    p = (base / rel).resolve()
    b = base.resolve()
    if b != p and b not in p.parents:
        raise HTTPException(400, f"path escapes base: {rel}")
    return p


def _is_protected(name: str) -> bool:
    if name.lower() in {n.lower() for n in DP_PROTECTED_NAMES}:
        return True
    return any(p.search(name) for p in PROTECTED_PATTERNS)


def _trash(path: Path, bucket: str) -> Path:
    dest = TRASH_DIR / bucket / (datetime.now().strftime("%Y%m%d-%H%M%S_") + path.name)
    dest.parent.mkdir(parents=True, exist_ok=True)
    shutil.move(str(path), str(dest))
    return dest


def _dir_size(path: Path) -> int:
    total = 0
    for root, _dirs, files in os.walk(path):
        for fn in files:
            try:
                total += (Path(root) / fn).stat().st_size
            except OSError:
                pass
    return total


# --------------------------------------------------------------------------- #
# Spawning
# --------------------------------------------------------------------------- #
def _spawn(run_id: str, project: str, argv: list[str], cwd: Path,
           env: dict[str, str], log_path: Path, *, kind: str = "train",
           meta: Optional[dict[str, Any]] = None) -> TrainRun:
    log_path.parent.mkdir(parents=True, exist_ok=True)
    logf = open(log_path, "w", encoding="utf-8", errors="ignore")
    creationflags = 0
    if os.name == "nt":
        creationflags = subprocess.CREATE_NEW_PROCESS_GROUP
    proc = subprocess.Popen(
        argv, cwd=str(cwd), env=env,
        stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
        bufsize=1, text=True, encoding="utf-8", errors="ignore",
        creationflags=creationflags,
    )
    run = TrainRun(run_id=run_id, project=project, proc=proc,
                   cmd=" ".join(argv), log_path=log_path, kind=kind,
                   meta=meta or {})
    RUNS[run_id] = run

    def _pump() -> None:
        assert proc.stdout is not None
        for line in proc.stdout:
            logf.write(line)
            logf.flush()
            run.lines.append(line.rstrip("\n"))
            if len(run.lines) > RING_MAX:
                del run.lines[: len(run.lines) - RING_MAX]
        logf.close()

    threading.Thread(target=_pump, daemon=True).start()
    return run


# --------------------------------------------------------------------------- #
# FastAPI app
# --------------------------------------------------------------------------- #
app = FastAPI(title="DP/PE Training Panel")


@app.get("/")
def index() -> FileResponse:
    return FileResponse(str(PANEL_DIR / "index.html"))


@app.get("/api/health")
def health() -> dict[str, Any]:
    return {
        "ok": True,
        "repo_root": str(REPO_ROOT),
        "dp_train_exe": str(_resolve(DP_TRAIN_EXE) or ""),
        "dp_dump_exe": str(_resolve(DP_DUMP_EXE) or ""),
        "pe_exe": str(_resolve(PE_EXE) or ""),
        "dp_runs_exists": DP_CKPT.exists(),
        "pe_runs_exists": PE_RUNS.exists(),
        "psutil": psutil is not None,
        "time": datetime.now().isoformat(timespec="seconds"),
    }


# ----------------------------- training control ---------------------------- #
class DPTrainReq(BaseModel):
    checkpoint: Optional[str] = None     # file under runs/, loaded via --load
    policy: str = "ppo"
    seed: Optional[int] = 24
    steps: Optional[int] = 8_000_000
    n_envs: int = 4
    save_every: int = 25
    spawn_random: bool = True
    hunt_v2: bool = True
    prey_priority: bool = True
    tag: str = "panel"                   # output file prefix
    extra_args: str = ""                 # raw extra CLI args
    reason: Optional[str] = None         # why this run (agent decision rationale)
    purpose: Optional[str] = None        # what this run is meant to achieve


class PETrainReq(BaseModel):
    config: str                          # *.toml file name under PE_ROOT
    seed: Optional[int] = None           # override runtime.random_seed (clones toml)
    resume_checkpoint: Optional[str] = None   # absolute/relative ckpt path (clones toml)
    generations: Optional[int] = None    # override generation count (clones toml)
    reason: Optional[str] = None         # why this run (agent decision rationale)
    purpose: Optional[str] = None        # what this run is meant to achieve


# --------------------------------------------------------------------------- #
# DP pipeline request models — teacher rollout -> BC -> DAgger -> PPO finetune.
# Every flag below mirrors an EXISTING CLI switch in main_train.cpp; these
# endpoints only orchestrate the binary (no source / dimension changes).
# Authoritative invocations taken from handoff/scripts/{bc_then_ppo,train_hunt,
# train_repertoire}.ps1 (the human-run pipeline that grew kills off 0).
# --------------------------------------------------------------------------- #
class DPDemosReq(BaseModel):
    """Teacher/oracle rollout that records (obs,action) demos for BC.
    Mirrors bc_then_ppo.ps1 'record' stage:
      --policy settler --weather --allow-cook --n-envs 1 --steps 2e6 --seed 24
      --record-demos traces\\demos_<tag>_s<seed>.bin --metrics metrics\\rec_<tag>.jsonl
    with env DP_SPAWN_RANDOM=1 DP_ORACLE_HUNT=1 DP_PREY_PRIORITY=1 DP_HUNT_V2=1."""
    teacher: str = "settler"            # settler|scripted|wander|noop (oracle policy)
    seed: int = 24
    steps: Optional[int] = 2_000_000
    episodes: Optional[int] = None      # alt bound; adds --episodes if set
    tag: str = "agent"
    weather: bool = True
    allow_cook: bool = True
    oracle_hunt: bool = True            # env DP_ORACLE_HUNT=1 (oracle actually hunts)
    demo_no_regen: bool = False         # --demo-no-regen
    oracle_no_fire: bool = False        # --oracle-no-fire (D-114 fire-free relabel)
    extra_args: str = ""
    reason: Optional[str] = None
    purpose: Optional[str] = None
    dry_run: bool = False               # build+return argv WITHOUT spawning


class DPBCReq(BaseModel):
    """Behavioral cloning from recorded demos. Mirrors 'bc' stage:
      --policy bc --bc-demos <csv> --bc-out checkpoints\\bc_<tag>.pt
      --bc-epochs 10 --bc-batch 16 --bc-bptt 64 --bc-lr 1e-3
      --bc-trigger-weight 5 --bc-min-eplen 300 --bc-trigger-oversample 3"""
    demos: str                          # comma-sep demo file names (traces/ or runs/)
    tag: str = "agent"
    epochs: int = 10
    batch: int = 16
    bptt: int = 64
    lr: float = 1e-3
    trigger_weight: float = 5.0
    min_eplen: Optional[int] = 300
    trigger_oversample: Optional[int] = 3
    build_oversample: Optional[int] = None
    max_eplen: Optional[int] = None
    extra_args: str = ""
    reason: Optional[str] = None
    purpose: Optional[str] = None
    dry_run: bool = False


class DPDaggerReq(BaseModel):
    """DAgger relabel (D-102): step env with the LOADED clone, label with the
    ScriptedSettler oracle. Fixes a BC clone's covariate shift. Source-confirmed
    invocation: --policy ppo --load <clone> --record-demos <out> --dagger."""
    clone: str                          # clone checkpoint to step with (--load)
    seed: int = 24
    steps: Optional[int] = 2_000_000
    episodes: Optional[int] = None
    tag: str = "dagger"
    weather: bool = True
    allow_cook: bool = True
    oracle_hunt: bool = True
    oracle_no_fire: bool = False
    extra_args: str = ""
    reason: Optional[str] = None
    purpose: Optional[str] = None
    dry_run: bool = False


class DPFinetuneReq(BaseModel):
    """PPO finetune from a BC/PPO base with potential-based reward-shaping
    coefficients (policy-invariant; the 'safe' reward levers). Mirrors
    train_hunt.ps1 / train_repertoire.ps1:
      --policy ppo --load <base> --weather --allow-cook --ent-coef 0.0015
      --critic-warmup 32 --lr 1.5e-4 --ppo-target-kl 0.08 --ppo-hunt-shaping H
      [--ppo-repertoire-shaping R --ppo-warmth-shaping W ...]"""
    load: str                           # base checkpoint (e.g. checkpoints/bc_hv2.pt)
    seed: int = 24
    steps: int = 8_000_000
    n_envs: int = 4
    save_every: int = 25
    tag: str = "agent"
    # ppo knobs (handoff-proven defaults)
    ent_coef: float = 0.0015
    lr: float = 1.5e-4
    critic_warmup: int = 32
    target_kl: Optional[float] = 0.08
    # potential-based reward-shaping coefficients (the safe reward levers)
    hunt_shaping: Optional[float] = None
    repertoire_shaping: Optional[float] = None
    warmth_shaping: Optional[float] = None
    night_shaping: Optional[float] = None
    day_shaping: Optional[float] = None
    # mechanism toggles / env params (CLI-exposed, no source change)
    weather: bool = True
    allow_cook: bool = True
    leanto: bool = False                # env DP_LEANTO=1
    thirst_decay: Optional[float] = None
    trigger_cost: Optional[float] = None
    extra_args: str = ""
    reason: Optional[str] = None
    purpose: Optional[str] = None
    dry_run: bool = False


@app.post("/api/dp/train")
def dp_train(req: DPTrainReq) -> dict[str, Any]:
    exe = _resolve(DP_TRAIN_EXE)
    if not exe:
        raise HTTPException(500, "deep_protagonist_train.exe not found (build DP first)")
    ts = datetime.now().strftime("%Y%m%d-%H%M%S")
    tag = f"{req.tag}_{ts}"
    argv = [str(exe), "--policy", req.policy]
    if req.checkpoint:
        ckpt = _dp_resolve(req.checkpoint)
        if not ckpt.exists():
            raise HTTPException(404, f"checkpoint not found: {req.checkpoint}")
        argv += ["--load", str(ckpt.relative_to(DP_ROOT))]
    DP_CKPT.mkdir(parents=True, exist_ok=True)
    DP_METRICS.mkdir(parents=True, exist_ok=True)
    argv += [
        "--n-envs", str(req.n_envs),
        "--steps", str(req.steps),
        "--save-path", str(Path("checkpoints") / f"{tag}.pt"),
        "--save-every", str(req.save_every),
        "--metrics", str(Path("metrics") / f"{tag}.jsonl"),
        "--seed", str(req.seed if req.seed is not None else 24),
    ]
    if req.extra_args.strip():
        argv += req.extra_args.split()

    env = dict(os.environ)
    env["DP_SPAWN_RANDOM"] = "1" if req.spawn_random else "0"
    env["DP_HUNT_V2"] = "1" if req.hunt_v2 else "0"
    env["DP_PREY_PRIORITY"] = "1" if req.prey_priority else "0"

    run_id = _new_run_id("dp")
    log_path = DP_LOGS / f"{tag}.stdout.log"
    run = _spawn(run_id, "dp", argv, DP_ROOT, env, log_path,
                 meta={"tag": tag, "seed": req.seed, "checkpoint": req.checkpoint,
                       "metrics": f"{tag}.jsonl", "policy": req.policy,
                       "steps": req.steps, "n_envs": req.n_envs,
                       "reason": req.reason, "purpose": req.purpose})
    return {"ok": True, "run_id": run_id, "pid": run.proc.pid, "cmd": run.cmd,
            "log": str(log_path), "metrics": f"{tag}.jsonl"}


# --------------------------------------------------------------------------- #
# DP pipeline: teacher-rollout -> BC -> DAgger -> PPO finetune.
# argv builders are PURE (no spawn) so they are unit-testable and feed dry_run.
# All paths are emitted relative to DP_ROOT (the spawn cwd), matching dp_train.
# --------------------------------------------------------------------------- #
def _argnum(v: Any) -> str:
    """Format a scalar for the CLI. str() keeps floats parseable by std::stof
    (e.g. 1.5e-4 -> '0.00015', 0.0015 -> '0.0015')."""
    if isinstance(v, bool):
        return "1" if v else "0"
    return str(v)


def _dp_env(*, spawn_random: bool = True, hunt_v2: bool = True,
            prey_priority: bool = True, oracle_hunt: bool = False,
            leanto: bool = False) -> dict[str, str]:
    """Build the DP env block. The four DP_* toggles match what the handoff
    scripts export; oracle_hunt/leanto are opt-in for specific stages."""
    env = dict(os.environ)
    env["DP_SPAWN_RANDOM"] = "1" if spawn_random else "0"
    env["DP_HUNT_V2"] = "1" if hunt_v2 else "0"
    env["DP_PREY_PRIORITY"] = "1" if prey_priority else "0"
    if oracle_hunt:
        env["DP_ORACLE_HUNT"] = "1"
    if leanto:
        env["DP_LEANTO"] = "1"
    return env


def _dp_demo_rel(name: str) -> str:
    """Resolve a demo file name to a DP_ROOT-relative path the binary can read;
    pass the raw name through if it isn't on disk yet (binary will validate)."""
    p = _dp_resolve(name)
    if p.exists():
        try:
            return str(p.relative_to(DP_ROOT))
        except ValueError:
            return str(p)
    return name


def _dp_demos_argv(exe: Path, req: "DPDemosReq") -> tuple[list[str], str, str]:
    demo_rel = str(Path("traces") / f"demos_{req.tag}_s{req.seed}.bin")
    metrics_rel = str(Path("metrics") / f"rec_{req.tag}.jsonl")
    argv = [str(exe), "--policy", req.teacher]
    if req.weather:
        argv.append("--weather")
    if req.allow_cook:
        argv.append("--allow-cook")
    argv += ["--n-envs", "1", "--record-demos", demo_rel,
             "--metrics", metrics_rel, "--seed", str(req.seed)]
    if req.steps is not None:
        argv += ["--steps", str(req.steps)]
    if req.episodes is not None:
        argv += ["--episodes", str(req.episodes)]
    if req.demo_no_regen:
        argv.append("--demo-no-regen")
    if req.oracle_no_fire:
        argv.append("--oracle-no-fire")
    if req.extra_args.strip():
        argv += req.extra_args.split()
    return argv, demo_rel, metrics_rel


def _dp_bc_argv(exe: Path, req: "DPBCReq") -> tuple[list[str], str]:
    demos = ",".join(_dp_demo_rel(d.strip())
                     for d in req.demos.split(",") if d.strip())
    bc_out = str(Path("checkpoints") / f"bc_{req.tag}.pt")
    argv = [str(exe), "--policy", "bc",
            "--bc-demos", demos,
            "--bc-out", bc_out,
            "--bc-epochs", str(req.epochs),
            "--bc-batch", str(req.batch),
            "--bc-bptt", str(req.bptt),
            "--bc-lr", _argnum(req.lr),
            "--bc-trigger-weight", _argnum(req.trigger_weight)]
    if req.min_eplen is not None:
        argv += ["--bc-min-eplen", str(req.min_eplen)]
    if req.trigger_oversample is not None:
        argv += ["--bc-trigger-oversample", str(req.trigger_oversample)]
    if req.build_oversample is not None:
        argv += ["--bc-build-oversample", str(req.build_oversample)]
    if req.max_eplen is not None:
        argv += ["--bc-max-eplen", str(req.max_eplen)]
    if req.extra_args.strip():
        argv += req.extra_args.split()
    return argv, bc_out


def _dp_dagger_argv(exe: Path, req: "DPDaggerReq") -> tuple[list[str], str]:
    load_rel = _dp_demo_rel(req.clone)
    demo_rel = str(Path("traces") / f"dagger_{req.tag}_s{req.seed}.bin")
    argv = [str(exe), "--policy", "ppo", "--load", load_rel,
            "--dagger", "--record-demos", demo_rel]
    if req.weather:
        argv.append("--weather")
    if req.allow_cook:
        argv.append("--allow-cook")
    argv += ["--n-envs", "1", "--seed", str(req.seed)]
    if req.steps is not None:
        argv += ["--steps", str(req.steps)]
    if req.episodes is not None:
        argv += ["--episodes", str(req.episodes)]
    if req.oracle_no_fire:
        argv.append("--oracle-no-fire")
    if req.extra_args.strip():
        argv += req.extra_args.split()
    return argv, demo_rel


def _dp_finetune_argv(exe: Path, req: "DPFinetuneReq",
                      tag: str) -> tuple[list[str], str, str]:
    load_rel = _dp_demo_rel(req.load)
    save_rel = str(Path("checkpoints") / f"ft_{tag}.pt")
    metrics_rel = str(Path("metrics") / f"ft_{tag}.jsonl")
    argv = [str(exe), "--policy", "ppo", "--load", load_rel]
    if req.weather:
        argv.append("--weather")
    if req.allow_cook:
        argv.append("--allow-cook")
    argv += ["--ent-coef", _argnum(req.ent_coef),
             "--critic-warmup", str(req.critic_warmup),
             "--lr", _argnum(req.lr)]
    if req.target_kl is not None:
        argv += ["--ppo-target-kl", _argnum(req.target_kl)]
    for flag, val in (("--ppo-hunt-shaping", req.hunt_shaping),
                      ("--ppo-repertoire-shaping", req.repertoire_shaping),
                      ("--ppo-warmth-shaping", req.warmth_shaping),
                      ("--ppo-night-shaping", req.night_shaping),
                      ("--ppo-day-shaping", req.day_shaping),
                      ("--thirst-decay", req.thirst_decay),
                      ("--trigger-cost", req.trigger_cost)):
        if val is not None:
            argv += [flag, _argnum(val)]
    argv += ["--n-envs", str(req.n_envs), "--steps", str(req.steps),
             "--save-path", save_rel, "--save-every", str(req.save_every),
             "--metrics", metrics_rel, "--seed", str(req.seed)]
    if req.extra_args.strip():
        argv += req.extra_args.split()
    return argv, save_rel, metrics_rel


@app.post("/api/dp/record_demos")
def dp_record_demos(req: DPDemosReq) -> dict[str, Any]:
    """Stage 1: a scripted teacher/oracle plays the world and records
    (obs,action) demos for behavioral cloning."""
    exe = _resolve(DP_TRAIN_EXE)
    if not exe:
        raise HTTPException(500, "deep_protagonist_train.exe not found (build DP first)")
    DP_TRACES.mkdir(parents=True, exist_ok=True)
    DP_METRICS.mkdir(parents=True, exist_ok=True)
    argv, demo_rel, metrics_rel = _dp_demos_argv(exe, req)
    if req.dry_run:
        return {"ok": True, "dry_run": True, "cmd": " ".join(argv),
                "demos": demo_rel, "metrics": metrics_rel}
    env = _dp_env(oracle_hunt=req.oracle_hunt)
    run_id = _new_run_id("dp")
    tag = f"rec_{req.tag}"
    log_path = DP_LOGS / f"{tag}.stdout.log"
    run = _spawn(run_id, "dp", argv, DP_ROOT, env, log_path, kind="record",
                 meta={"tag": tag, "stage": "record_demos", "teacher": req.teacher,
                       "seed": req.seed, "demos": demo_rel, "metrics": metrics_rel,
                       "reason": req.reason, "purpose": req.purpose})
    return {"ok": True, "run_id": run_id, "pid": run.proc.pid, "cmd": run.cmd,
            "log": str(log_path), "demos": demo_rel, "metrics": metrics_rel}


@app.post("/api/dp/train_bc")
def dp_train_bc(req: DPBCReq) -> dict[str, Any]:
    """Stage 2: behavioral cloning from recorded demos -> checkpoints/bc_<tag>.pt."""
    exe = _resolve(DP_TRAIN_EXE)
    if not exe:
        raise HTTPException(500, "deep_protagonist_train.exe not found (build DP first)")
    if not req.demos.strip():
        raise HTTPException(400, "need at least one demo file (demos=...)")
    DP_CKPT.mkdir(parents=True, exist_ok=True)
    argv, bc_out = _dp_bc_argv(exe, req)
    if req.dry_run:
        return {"ok": True, "dry_run": True, "cmd": " ".join(argv), "bc_out": bc_out}
    env = _dp_env()
    run_id = _new_run_id("dp")
    tag = f"bc_{req.tag}"
    log_path = DP_LOGS / f"{tag}.stdout.log"
    run = _spawn(run_id, "dp", argv, DP_ROOT, env, log_path, kind="bc",
                 meta={"tag": tag, "stage": "train_bc", "demos": req.demos,
                       "bc_out": bc_out, "epochs": req.epochs,
                       "reason": req.reason, "purpose": req.purpose})
    return {"ok": True, "run_id": run_id, "pid": run.proc.pid, "cmd": run.cmd,
            "log": str(log_path), "bc_out": bc_out}


@app.post("/api/dp/dagger")
def dp_dagger(req: DPDaggerReq) -> dict[str, Any]:
    """Stage 2b (D-102): step env with a loaded clone, label with the oracle, to
    fix the clone's covariate shift -> traces/dagger_<tag>_s<seed>.bin."""
    exe = _resolve(DP_TRAIN_EXE)
    if not exe:
        raise HTTPException(500, "deep_protagonist_train.exe not found (build DP first)")
    if not req.clone.strip():
        raise HTTPException(400, "need a clone checkpoint to step with (clone=...)")
    DP_TRACES.mkdir(parents=True, exist_ok=True)
    argv, demo_rel = _dp_dagger_argv(exe, req)
    if req.dry_run:
        return {"ok": True, "dry_run": True, "cmd": " ".join(argv), "demos": demo_rel}
    env = _dp_env(oracle_hunt=req.oracle_hunt)
    run_id = _new_run_id("dp")
    tag = f"dagger_{req.tag}"
    log_path = DP_LOGS / f"{tag}.stdout.log"
    run = _spawn(run_id, "dp", argv, DP_ROOT, env, log_path, kind="dagger",
                 meta={"tag": tag, "stage": "dagger", "clone": req.clone,
                       "seed": req.seed, "demos": demo_rel,
                       "reason": req.reason, "purpose": req.purpose})
    return {"ok": True, "run_id": run_id, "pid": run.proc.pid, "cmd": run.cmd,
            "log": str(log_path), "demos": demo_rel}


@app.post("/api/dp/finetune")
def dp_finetune(req: DPFinetuneReq) -> dict[str, Any]:
    """Stage 3: PPO finetune from a BC/PPO base with reward-shaping PBRS
    coefficients -> checkpoints/ft_<tag>.pt."""
    exe = _resolve(DP_TRAIN_EXE)
    if not exe:
        raise HTTPException(500, "deep_protagonist_train.exe not found (build DP first)")
    if not req.load.strip():
        raise HTTPException(400, "finetune needs a base checkpoint (load=...)")
    DP_CKPT.mkdir(parents=True, exist_ok=True)
    DP_METRICS.mkdir(parents=True, exist_ok=True)
    ts = datetime.now().strftime("%Y%m%d-%H%M%S")
    tag = f"{req.tag}_{ts}"
    argv, save_rel, metrics_rel = _dp_finetune_argv(exe, req, tag)
    if req.dry_run:
        return {"ok": True, "dry_run": True, "cmd": " ".join(argv),
                "save_path": save_rel, "metrics": metrics_rel}
    env = _dp_env(leanto=req.leanto)
    run_id = _new_run_id("dp")
    log_path = DP_LOGS / f"ft_{tag}.stdout.log"
    run = _spawn(run_id, "dp", argv, DP_ROOT, env, log_path, kind="finetune",
                 meta={"tag": tag, "stage": "finetune", "load": req.load,
                       "seed": req.seed, "steps": req.steps, "n_envs": req.n_envs,
                       "save_path": save_rel, "metrics": f"ft_{tag}.jsonl",
                       "reason": req.reason, "purpose": req.purpose})
    return {"ok": True, "run_id": run_id, "pid": run.proc.pid, "cmd": run.cmd,
            "log": str(log_path), "save_path": save_rel, "metrics": f"ft_{tag}.jsonl"}


def _clone_pe_config(base: Path, *, seed: Optional[int],
                     resume_checkpoint: Optional[str],
                     generations: Optional[int]) -> Path:
    """Clone a PE toml with overrides (seed = new world, resume = same model).

    Mirrors clone_pe.ps1: swap random_seed / resume_checkpoint_path /
    generations / scenario_name / runs_dir so each clone writes to its own dir.
    """
    text = base.read_text(encoding="utf-8", errors="ignore")
    suffix_parts = []
    if seed is not None:
        suffix_parts.append(f"s{seed}")
        text = re.sub(r"random_seed\s*=\s*\d+", f"random_seed = {seed}", text)
        text = re.sub(r'scenario_name\s*=\s*"([^"]*)"',
                      lambda m: f'scenario_name = "{m.group(1)}_{("s%d" % seed)}"', text)
        text = re.sub(r'runs_dir\s*=\s*"([^"]*)"',
                      lambda m: f'runs_dir = "{m.group(1)}_s{seed}"', text)
    if resume_checkpoint is not None:
        rc = resume_checkpoint.replace("\\", "/")
        if re.search(r"resume_checkpoint_path\s*=", text):
            text = re.sub(r'resume_checkpoint_path\s*=\s*"[^"]*"',
                          f'resume_checkpoint_path = "{rc}"', text)
        else:
            text += f'\nresume_checkpoint_path = "{rc}"\n'
        suffix_parts.append("warm")
    if generations is not None:
        if re.search(r"(?m)^\s*generations\s*=", text):
            text = re.sub(r"(?m)^(\s*)generations\s*=\s*\d+",
                          rf"\g<1>generations = {generations}", text)
        else:
            text += f"\ngenerations = {generations}\n"
    suffix = "_".join(suffix_parts) or datetime.now().strftime("%H%M%S")
    dst = PE_ROOT / f"panel_{base.stem}_{suffix}.toml"
    dst.write_text(text, encoding="utf-8")
    return dst


def _toml_literal(v: Any) -> str:
    """Render a Python scalar as a TOML-valid literal. Numeric/bool strings are
    emitted bare; everything else is quoted as a TOML string."""
    if isinstance(v, bool):
        return "true" if v else "false"
    if isinstance(v, int):
        return str(v)
    if isinstance(v, float):
        return repr(v)
    s = str(v).strip()
    if s.lower() in ("true", "false"):
        return s.lower()
    try:
        int(s)
        return s
    except ValueError:
        pass
    try:
        float(s)
        return s
    except ValueError:
        pass
    return '"' + s.replace("\\", "\\\\").replace('"', '\\"') + '"'


def _toml_set_knob(text: str, knob: str, literal: str) -> tuple[str, Optional[str], bool]:
    """Set a single (possibly dotted) knob to `literal` in a TOML string,
    scoping the search to the knob's [section] so duplicate leaf keys across
    sections don't collide. Preserves inline comments. Returns
    (new_text, old_value_or_None, changed)."""
    section, leaf = (knob.rsplit(".", 1) + [""])[:2] if "." in knob else ("", knob)
    lines = text.splitlines()
    start, end = 0, len(lines)
    if section:
        hdr = "[" + section + "]"
        si = next((i for i, ln in enumerate(lines) if ln.strip() == hdr), None)
        if si is None:
            return text, None, False
        start = si + 1
        end = next((j for j in range(start, len(lines))
                    if lines[j].lstrip().startswith("[")), len(lines))
    else:
        end = next((j for j in range(len(lines))
                    if lines[j].lstrip().startswith("[")), len(lines))
    pat = re.compile(r"^(\s*)" + re.escape(leaf) + r"(\s*=\s*)(.+?)(\s*(?:#.*)?)$")
    for k in range(start, end):
        m = pat.match(lines[k])
        if m:
            old = m.group(3).strip()
            lines[k] = f"{m.group(1)}{leaf}{m.group(2)}{literal}{m.group(4) or ''}"
            out = "\n".join(lines)
            if text.endswith("\n"):
                out += "\n"
            return out, old, True
    return text, None, False


def make_config_variant(base_name: str, knob: str, value: Any) -> dict[str, Any]:
    """ab_knob actuator: clone a PE TOML changing exactly ONE knob, isolating
    the variant's scenario_name/runs_dir so its run output never collides with
    the baseline. Returns the new config filename for start_pe_training."""
    if not base_name or not knob:
        return {"ok": False, "error": "need base (toml) and knob"}
    try:
        base = _safe_join(PE_ROOT, base_name)
    except Exception as e:  # noqa: BLE001
        return {"ok": False, "error": str(e)}
    if not base.exists():
        return {"ok": False, "error": f"config not found: {base_name}"}
    if value is None:
        return {"ok": False, "error": "need a value for the knob"}
    text = base.read_text(encoding="utf-8", errors="ignore")
    literal = _toml_literal(value)
    new_text, old, changed = _toml_set_knob(text, knob, literal)
    if not changed:
        return {"ok": False, "error": f"knob not found in config: {knob}"}
    tag = (re.sub(r"[^0-9A-Za-z]+", "_", knob).strip("_") + "_"
           + re.sub(r"[^0-9A-Za-z.+-]+", "_", str(value)).strip("_"))
    # Isolate the variant's run output so A/B doesn't overwrite the baseline.
    new_text = re.sub(r'scenario_name\s*=\s*"([^"]*)"',
                      lambda m: f'scenario_name = "{m.group(1)}_ab_{tag}"',
                      new_text, count=1)
    new_text = re.sub(r'runs_dir\s*=\s*"([^"]*)"',
                      lambda m: f'runs_dir = "{m.group(1)}_ab_{tag}"',
                      new_text, count=1)
    dst = PE_ROOT / f"panel_{base.stem}_ab_{tag}.toml"
    dst.write_text(new_text, encoding="utf-8")
    return {"ok": True, "variant": dst.name, "base": base_name, "knob": knob,
            "old_value": old, "new_value": literal,
            "note": "已生成单旋钮对照配置(scenario/runs_dir 已隔离),用 start_pe_training 启动它与基线对照。"}


def _num_or_none(s: Optional[str]) -> Optional[float]:
    """Parse a TOML literal as float for delta math; None if non-numeric."""
    if s is None:
        return None
    try:
        return float(str(s).strip())
    except (TypeError, ValueError):
        return None


def _knob_delta(old: Optional[str], new: Any) -> dict[str, Any]:
    """Numeric delta summary used by the UI + as a cross-check for the agent's
    tier-C gate (sign flip / magnitude). Non-numeric edits report numeric=False."""
    o, n = _num_or_none(old), _num_or_none(new)
    if o is None or n is None:
        return {"numeric": False}
    sign_flip = (o > 0 > n) or (o < 0 < n) or (o == 0) != (n == 0)
    pct = (abs(n - o) / abs(o)) if o != 0 else (float("inf") if n != 0 else 0.0)
    return {"numeric": True, "old": o, "new": n, "sign_flip": sign_flip,
            "abs_pct_change": (None if pct == float("inf") else round(pct, 4))}


class PETuneReq(BaseModel):
    """Apply one-or-more knob edits to a PE TOML, producing ONE isolated variant
    (scenario_name/runs_dir suffixed so it never overwrites the baseline). Used
    by pe_reward_tune / pe_archive_geometry / approved pe_curriculum_edit. The
    server only edits TOML — it does not classify risk (the agent's tier-C gate
    decides autonomy); the returned per-knob deltas let it cross-check."""
    config: str                          # base *.toml under PE_ROOT
    edits: dict[str, Any]                # {dotted_knob: value}, e.g. {"reward.food_weight": 1.2}
    label: str = "tune"                  # variant name hint
    dry_run: bool = False


@app.post("/api/pe/tune")
def pe_tune(req: PETuneReq) -> dict[str, Any]:
    if not req.config or not req.edits:
        raise HTTPException(400, "need config and at least one knob edit")
    try:
        base = _safe_join(PE_ROOT, req.config)
    except Exception as e:  # noqa: BLE001
        raise HTTPException(400, str(e))
    if not base.exists():
        raise HTTPException(404, f"config not found: {req.config}")
    text = base.read_text(encoding="utf-8", errors="ignore")
    applied: list[dict[str, Any]] = []
    for knob, value in req.edits.items():
        if value is None:
            return {"ok": False, "error": f"knob {knob} has no value"}
        literal = _toml_literal(value)
        text, old, changed = _toml_set_knob(text, knob, literal)
        if not changed:
            return {"ok": False, "error": f"knob not found in config: {knob}"}
        applied.append({"knob": knob, "old_value": old, "new_value": literal,
                        "delta": _knob_delta(old, value)})
    safe = re.sub(r"[^0-9A-Za-z]+", "_", req.label).strip("_") or "tune"
    text = re.sub(r'scenario_name\s*=\s*"([^"]*)"',
                  lambda m: f'scenario_name = "{m.group(1)}_{safe}"', text, count=1)
    text = re.sub(r'runs_dir\s*=\s*"([^"]*)"',
                  lambda m: f'runs_dir = "{m.group(1)}_{safe}"', text, count=1)
    if req.dry_run:
        return {"ok": True, "dry_run": True, "base": req.config,
                "would_write": f"panel_{base.stem}_{safe}.toml", "applied": applied}
    dst = PE_ROOT / f"panel_{base.stem}_{safe}.toml"
    dst.write_text(text, encoding="utf-8")
    return {"ok": True, "variant": dst.name, "base": req.config, "applied": applied,
            "note": "已生成多旋钮变体(scenario/runs_dir 已隔离),用 start_pe_training 启动。"}


@app.post("/api/pe/train")
def pe_train(req: PETrainReq) -> dict[str, Any]:
    exe = _resolve(PE_EXE)
    if not exe:
        raise HTTPException(500, "neural-eco-protagonist-batched.exe not found "
                                 "(expected under build_ninja_cuda/bin)")
    base = _safe_join(PE_ROOT, req.config)
    if not base.exists():
        raise HTTPException(404, f"config not found: {req.config}")

    # Any override => clone a new toml so we never mutate the user's config.
    if req.seed is not None or req.resume_checkpoint is not None or req.generations is not None:
        cfg = _clone_pe_config(base, seed=req.seed,
                               resume_checkpoint=req.resume_checkpoint,
                               generations=req.generations)
    else:
        cfg = base

    # exe is launched from PE_ROOT and given the config file NAME (relative).
    argv = [str(exe), "--config", cfg.name]

    env = dict(os.environ)
    if Path(CUDA_BIN).exists():
        env["PATH"] = CUDA_BIN + os.pathsep + env.get("PATH", "")

    run_id = _new_run_id("pe")
    ts = datetime.now().strftime("%Y%m%d-%H%M%S")
    log_path = PE_PANEL_LOGS / f"{cfg.stem}_{ts}.stdout.log"
    run = _spawn(run_id, "pe", argv, PE_ROOT, env, log_path,
                 meta={"config": cfg.name, "seed": req.seed,
                       "resume_checkpoint": req.resume_checkpoint,
                       "generations": req.generations,
                       "reason": req.reason, "purpose": req.purpose})
    return {"ok": True, "run_id": run_id, "pid": run.proc.pid, "cmd": run.cmd,
            "config": cfg.name, "log": str(log_path)}


class StopReq(BaseModel):
    run_id: Optional[str] = None
    pid: Optional[int] = None
    hard: bool = False                   # False = graceful (CTRL_BREAK/term), True = kill -9
    timeout_s: float = 8.0


@app.post("/api/train/stop")
def train_stop(req: StopReq) -> dict[str, Any]:
    """Stop a run by run-id (panel-launched) or pid (any, incl. external).

    Graceful path: CTRL_BREAK (Windows) / SIGINT, wait, then terminate. Both
    trainers checkpoint periodically (DP --save-every, PE per-generation), so a
    stop loses at most the work since the last checkpoint. `hard=True` forces
    an immediate kill (confirm on the client first — may truncate a checkpoint
    being written).
    """
    run = RUNS.get(req.run_id) if req.run_id else None
    if run is not None:
        proc = run.proc
        if proc.poll() is not None:
            return {"ok": True, "already_exited": True, "returncode": proc.poll()}
        try:
            if req.hard:
                proc.kill()
            else:
                if os.name == "nt":
                    proc.send_signal(signal.CTRL_BREAK_EVENT)
                else:
                    proc.send_signal(signal.SIGINT)
                t0 = time.time()
                while proc.poll() is None and time.time() - t0 < req.timeout_s:
                    time.sleep(0.3)
                if proc.poll() is None:
                    proc.terminate()
                    time.sleep(1.0)
                if proc.poll() is None:
                    proc.kill()
        except Exception as e:  # noqa: BLE001
            raise HTTPException(500, f"stop failed: {e}")
        return {"ok": True, "run_id": run.run_id, "returncode": proc.poll()}

    # No registry entry -> stop an external process by pid.
    if req.pid is None:
        raise HTTPException(404, "no run_id or pid provided")
    if psutil is None:
        raise HTTPException(500, "psutil unavailable; cannot stop external pid")
    try:
        p = psutil.Process(req.pid)
    except psutil.NoSuchProcess:
        return {"ok": True, "already_exited": True}
    try:
        if req.hard:
            p.kill()
        else:
            p.terminate()
            try:
                p.wait(timeout=req.timeout_s)
            except psutil.TimeoutExpired:
                p.kill()
    except Exception as e:  # noqa: BLE001
        raise HTTPException(500, f"stop failed: {e}")
    return {"ok": True, "pid": req.pid}


# ------------------------- process / run discovery ------------------------- #
def _classify(name: str) -> Optional[str]:
    n = (name or "").lower()
    # strip a trailing .exe so matching works on both Windows + posix
    stem = n[:-4] if n.endswith(".exe") else n
    if n in {x.lower() for x in DP_TRAIN_IMAGES} or stem == "deep_protagonist_train":
        return "dp"
    if n.startswith(PE_IMAGE_PREFIX) or stem.startswith(PE_IMAGE_PREFIX):
        return "pe"
    return None


def _classify_proc(name: str, cmdline: list[str]) -> Optional[str]:
    """Classify by image name AND cmdline[0] basename.

    psutil's name() is the full image on Windows but truncated to 15 chars in
    /proc/comm on Linux; cmdline[0] keeps the full path so we check both. On
    Linux comm-truncation, also fall back to a prefix-of-prefix match.
    """
    proj = _classify(name)
    if proj:
        return proj
    n = (name or "").lower()
    nstem = n[:-4] if n.endswith(".exe") else n
    # If the image is a launcher/interpreter (cmd, powershell, python, env, sh)
    # the real training exe is one of the cmdline tokens — scan them.
    LAUNCHERS = {"cmd", "cmd.exe", "powershell", "powershell.exe", "pwsh",
                 "python", "python3", "python.exe", "env", "sh", "bash", "conhost"}
    if nstem in LAUNCHERS or not name:
        for tok in (cmdline or []):
            proj = _classify(Path(tok).name)
            if proj:
                return proj
    # last-resort: truncated /proc/comm that is a prefix of a known image name
    if n and "neural-eco-protagonist".startswith(n):
        return "pe"
    if n and "deep_protagonist_train".startswith(n):
        return "dp"
    return None


def _registry_pids() -> dict[int, TrainRun]:
    return {r.proc.pid: r for r in RUNS.values()}


@app.get("/api/processes")
def processes() -> dict[str, Any]:
    """All training processes on the box: panel-launched AND external.

    The training sweep is often started outside the panel (PowerShell/WMI), so
    we discover by executable image name and merge in registry metadata for the
    ones we launched.
    """
    reg = _registry_pids()
    found: list[dict[str, Any]] = []
    seen: set[int] = set()

    if psutil is not None:
        for p in psutil.process_iter(["pid", "name", "cmdline", "create_time", "memory_info"]):
            try:
                cmd_list = p.info.get("cmdline") or []
                proj = _classify_proc(p.info.get("name") or "", cmd_list)
                if not proj:
                    continue
                pid = p.info["pid"]
                seen.add(pid)
                cmdline = " ".join(cmd_list)
                mem = p.info.get("memory_info")
                run = reg.get(pid)
                # extract a config/checkpoint hint from the cmdline
                cfg = None
                m = re.search(r"--config\s+(\S+)", cmdline)
                if m:
                    cfg = m.group(1)
                else:
                    m = re.search(r"--load\s+(\S+)", cmdline)
                    if m:
                        cfg = m.group(1)
                found.append({
                    "pid": pid,
                    "project": proj,
                    "image": p.info.get("name"),
                    "cmdline": cmdline,
                    "config": cfg,
                    "elapsed_s": round(time.time() - (p.info.get("create_time") or time.time()), 1),
                    "mem_mb": round(mem.rss / 1e6, 1) if mem else None,
                    "source": "panel" if run else "external",
                    "run_id": run.run_id if run else None,
                    "log": str(run.log_path) if run else None,
                    "kind": run.kind if run else "train",
                })
            except (psutil.NoSuchProcess, psutil.AccessDenied):
                continue

    # Registry runs still alive but whose pid wasn't found via psutil
    # (e.g. psutil unavailable). Exited runs belong in /api/runs history, not
    # in the live "running processes" view, so skip them here.
    for run in RUNS.values():
        if run.proc.pid in seen or not run.running:
            continue
        found.append({
            "pid": run.proc.pid,
            "project": run.project,
            "image": Path(run.cmd.split()[0]).name if run.cmd else None,
            "cmdline": run.cmd,
            "config": run.meta.get("config") or run.meta.get("checkpoint"),
            "elapsed_s": round(time.time() - run.started, 1),
            "mem_mb": None,
            "source": "panel",
            "run_id": run.run_id,
            "log": str(run.log_path),
            "kind": run.kind,
            "running": run.running,
            "returncode": run.proc.poll(),
        })

    found.sort(key=lambda d: (d["project"], -(d.get("elapsed_s") or 0)))
    dp_n = sum(1 for f in found if f["project"] == "dp")
    pe_n = sum(1 for f in found if f["project"] == "pe")
    return {"processes": found, "dp_count": dp_n, "pe_count": pe_n,
            "total": len(found)}


@app.get("/api/runs")
def runs() -> list[dict[str, Any]]:
    out = []
    for r in RUNS.values():
        out.append({
            "run_id": r.run_id, "project": r.project, "kind": r.kind,
            "pid": r.proc.pid, "running": r.running, "returncode": r.proc.poll(),
            "cmd": r.cmd, "log": str(r.log_path),
            "elapsed_s": round(time.time() - r.started, 1),
            "meta": r.meta, "tail": r.lines[-30:],
        })
    out.sort(key=lambda d: -d["elapsed_s"])
    return out


# -------------------------------- configs ---------------------------------- #
@app.get("/api/pe/configs")
def pe_configs() -> list[dict[str, Any]]:
    out = []
    for f in sorted(PE_ROOT.glob("*.toml")):
        seed = resume = None
        if tomllib is not None:
            try:
                data = tomllib.loads(f.read_text(encoding="utf-8", errors="ignore"))
                rt = data.get("runtime", {}) if isinstance(data, dict) else {}
                seed = rt.get("random_seed", data.get("random_seed"))
                resume = (rt.get("resume_checkpoint_path")
                          or data.get("resume_checkpoint_path"))
            except Exception:  # noqa: BLE001
                pass
        out.append({"name": f.name, "seed": seed,
                    "has_resume": bool(resume),
                    "is_panel_clone": f.name.startswith("panel_"),
                    "size_kb": round(f.stat().st_size / 1024, 1)})
    return out


# ------------------------------ checkpoints -------------------------------- #
def _dp_resolve(name: str) -> Path:
    """Resolve a DP file by name across the split category dirs (back-compat)."""
    for base in (DP_CKPT, DP_METRICS, DP_LOGS, DP_TRACES, DP_RUNS, DP_ROOT):
        try:
            p = _safe_join(base, name)
        except Exception:  # noqa: BLE001
            continue
        if p.exists():
            return p
    return _safe_join(DP_CKPT, name)


@app.get("/api/dp/checkpoints")
def dp_checkpoints() -> list[dict[str, Any]]:
    out = []
    if DP_CKPT.exists():
        for f in sorted(DP_CKPT.glob("*.pt")):
            st = f.stat()
            out.append({
                "name": f.name,
                "size_mb": round(st.st_size / 1e6, 2),
                "mtime": datetime.fromtimestamp(st.st_mtime).isoformat(timespec="seconds"),
                "protected": _is_protected(f.name),
            })
    return out


@app.get("/api/pe/checkpoints")
def pe_checkpoints() -> list[dict[str, Any]]:
    out = []
    if PE_RUNS.exists():
        for scenario in sorted(PE_RUNS.iterdir()):
            if not scenario.is_dir():
                continue
            for ts_dir in sorted(scenario.iterdir()):
                if not ts_dir.is_dir():
                    continue
                gens = sorted(ts_dir.glob("checkpoint_gen*"))
                if gens:
                    def _gnum(p: Path) -> int:
                        m = re.search(r"(\d+)", p.name)
                        return int(m.group(1)) if m else -1
                    latest = max(gens, key=_gnum)
                    out.append({
                        "scenario": scenario.name,
                        "run": ts_dir.name,
                        "count": len(gens),
                        "latest": latest.name,
                        "latest_path": str(latest).replace("\\", "/"),
                    })
    return out


class DeleteReq(BaseModel):
    names: list[str]
    hard: bool = False                   # False = move to _trash (safe), True = rm


@app.post("/api/dp/checkpoints/clear")
def dp_clear_checkpoints(req: DeleteReq) -> dict[str, Any]:
    removed, skipped = [], []
    for name in req.names:
        if _is_protected(name):
            skipped.append({"name": name, "reason": "protected red-line asset"})
            continue
        f = _safe_join(DP_CKPT, name)
        if not f.exists():
            skipped.append({"name": name, "reason": "not found"})
            continue
        if req.hard:
            f.unlink()
        else:
            _trash(f, "dp_runs")
        removed.append(name)
    return {"ok": True, "removed": removed, "skipped": skipped,
            "mode": "hard" if req.hard else "trash"}


# --------------------------------- logs ------------------------------------ #
LOG_EXTS = {".log", ".err", ".stderr", ".stdout"}


def _list_logs(base: Path) -> list[dict[str, Any]]:
    out = []
    if not base.exists():
        return out
    for f in sorted(base.iterdir()):
        if not f.is_file():
            continue
        ext = f.suffix.lower()
        is_log = ext in LOG_EXTS or f.name.endswith(".stdout.log") or f.name.endswith(".stderr.log")
        is_jsonl = ext == ".jsonl"
        if not (is_log or is_jsonl):
            continue
        st = f.stat()
        out.append({
            "name": f.name,
            "kind": "jsonl" if is_jsonl else "log",
            "protected": _is_protected(f.name),
            "size_kb": round(st.st_size / 1024, 1),
            "mtime": datetime.fromtimestamp(st.st_mtime).isoformat(timespec="seconds"),
        })
    return out


@app.get("/api/dp/logs")
def dp_logs() -> list[dict[str, Any]]:
    return _list_logs(DP_LOGS)


@app.get("/api/pe/logs")
def pe_logs() -> list[dict[str, Any]]:
    # PE console logs live both in panel_logs and as *.log next to configs.
    out = _list_logs(PE_PANEL_LOGS)
    for f in sorted(PE_ROOT.glob("*.log")):
        st = f.stat()
        out.append({"name": f.name, "kind": "log", "protected": False,
                    "size_kb": round(st.st_size / 1024, 1),
                    "mtime": datetime.fromtimestamp(st.st_mtime).isoformat(timespec="seconds"),
                    "in_root": True})
    return out


class ClearLogsReq(BaseModel):
    project: str
    names: Optional[list[str]] = None    # None = all console logs (jsonl kept)
    hard: bool = False


@app.post("/api/logs/clear")
def logs_clear(req: ClearLogsReq) -> dict[str, Any]:
    bases = [DP_LOGS] if req.project == "dp" else [PE_PANEL_LOGS, PE_ROOT]
    live_logs = {r.log_path.resolve() for r in RUNS.values() if r.running}
    removed, skipped = [], []

    def _candidates() -> list[Path]:
        if req.names:
            cands = []
            for n in req.names:
                for b in bases:
                    p = (b / n)
                    if p.exists():
                        cands.append(_safe_join(b, n))
                        break
            return cands
        # default: console logs only (never auto-delete jsonl metrics)
        cands = []
        for b in bases:
            if not b.exists():
                continue
            for f in b.iterdir():
                if not f.is_file():
                    continue
                if (f.suffix.lower() in LOG_EXTS
                        or f.name.endswith(".stdout.log")
                        or f.name.endswith(".stderr.log")):
                    cands.append(f)
        return cands

    for f in _candidates():
        if not f.exists():
            skipped.append({"name": f.name, "reason": "not found"})
            continue
        if _is_protected(f.name):
            skipped.append({"name": f.name, "reason": "protected"})
            continue
        if f.resolve() in live_logs:
            skipped.append({"name": f.name, "reason": "in use (live run)"})
            continue
        if req.hard:
            f.unlink()
        else:
            _trash(f, f"{req.project}_logs")
        removed.append(f.name)
    return {"ok": True, "removed": removed, "skipped": skipped,
            "mode": "hard" if req.hard else "trash"}


# -------------------------------- metrics ---------------------------------- #
def _load_jsonl(path: Path, limit: Optional[int] = None) -> list[dict]:
    rows = []
    for ln in path.read_text(encoding="utf-8", errors="ignore").splitlines():
        ln = ln.strip()
        if not ln:
            continue
        try:
            rows.append(json.loads(ln))
        except json.JSONDecodeError:
            pass
        if limit and len(rows) >= limit:
            break
    return rows


def _smooth(xs: list[float], w: int = 20) -> list[float]:
    out = []
    for i in range(len(xs)):
        lo = max(0, i - w + 1)
        seg = xs[lo:i + 1]
        out.append(sum(seg) / len(seg))
    return out


@app.get("/api/metrics")
def metrics(project: str, file: str) -> dict[str, Any]:
    base = DP_METRICS if project == "dp" else PE_RUNS
    path = _safe_join(base, file)
    if not path.exists():
        raise HTTPException(404, f"log not found: {file}")
    rows = _load_jsonl(path)
    if not rows:
        return {"ok": True, "n": 0}

    reward = [float(r.get("reward", 0.0)) for r in rows]
    score = [float(r.get("score", 0.0)) for r in rows]
    unlocks = [sum(1 for x in r.get("unlocks", []) if x) for r in rows]

    n_act = len(ACTION_NAMES)
    act_tot = [0.0] * n_act
    for r in rows:
        a = r.get("act", [])
        for i, v in enumerate(a[:n_act]):
            act_tot[i] += float(v)

    n_ms = len(MILESTONE_NAMES)
    ms_fired = [0] * n_ms
    for r in rows:
        u = r.get("unlocks", [])
        for i, x in enumerate(u[:n_ms]):
            if x:
                ms_fired[i] += 1
    ms_rate = [f / len(rows) for f in ms_fired]

    deaths = {label: sum(int(r.get(k, 0)) for r in rows) for k, label in DEATH_CAUSES}
    survival = sum(1 for r in rows if int(r.get("deaths", 0)) == 0) / len(rows)
    dead_actions = [ACTION_NAMES[i] for i in range(n_act)
                    if i not in UNTRACKED_ACT and act_tot[i] == 0]

    return {
        "ok": True,
        "n": len(rows),
        "reward": reward,
        "reward_smooth": _smooth(reward),
        "score": score,
        "score_smooth": _smooth(score),
        "unlocks": unlocks,
        "wander_ref": WANDER_REWARD,
        "action_names": ACTION_NAMES,
        "action_totals": act_tot,
        "action_mean": [t / len(rows) for t in act_tot],
        "milestone_names": MILESTONE_NAMES,
        "milestone_rate": ms_rate,
        "deaths": deaths,
        "survival_rate": survival,
        "dead_actions": dead_actions,
    }


_PE_GEN_RE = re.compile(
    r"Gen\s+(\d+)\s*\|\s*protagonist\s+avg=([\-\d.]+)\s+best=([\-\d.]+).*?"
    r"me_cov=(\d+)/(\d+)\s+qd=([\-\d.]+)")


@app.get("/api/pe/curve")
def pe_curve(file: str) -> dict[str, Any]:
    """Per-generation series parsed from a PE console .log (for live charts).

    PE writes plain-text 'Gen N | protagonist avg=.. best=.. | ... me_cov=X/Y
    qd=..' lines (there is no jsonl like DP), so we parse the log directly and
    return the avg/best/qd/me_cov series. Works while training is in progress —
    the log grows and each poll re-parses the freshest tail.
    """
    path = None
    for base in (PE_ROOT, PE_PANEL_LOGS):
        cand = base / file
        if cand.exists():
            path = cand
            break
    if path is None:
        raise HTTPException(404, f"PE log not found: {file}")
    gens: list[int] = []
    avg: list[float] = []
    best: list[float] = []
    qd: list[float] = []
    me_cov: list[int] = []
    me_total = 0
    extinct = 0
    try:
        text = path.read_text(encoding="utf-8", errors="ignore")
    except OSError as e:
        raise HTTPException(500, f"read failed: {e}") from e
    for ln in text.splitlines():
        if "extinct" in ln.lower():
            extinct += 1
        m = _PE_GEN_RE.search(ln)
        if not m:
            continue
        gens.append(int(m.group(1)))
        avg.append(float(m.group(2)))
        best.append(float(m.group(3)))
        me_cov.append(int(m.group(4)))
        me_total = int(m.group(5))
        qd.append(float(m.group(6)))
    return {"ok": True, "file": file, "n": len(gens), "gen": gens,
            "avg": avg, "best": best, "qd": qd,
            "me_cov": me_cov, "me_total": me_total, "extinct_hits": extinct}


# ----------------------- behavioural panorama (spectrum) ------------------- #
_TS_TAIL_RE = re.compile(r"_\d{8}-\d{6}$")


def _read_csv(path: Path) -> list[dict[str, str]]:
    """Tolerant CSV -> list[dict] (empty list if missing/unreadable)."""
    if not path.exists():
        return []
    try:
        import csv
        with open(path, "r", encoding="utf-8", errors="ignore", newline="") as fh:
            return list(csv.DictReader(fh))
    except (OSError, csv.Error):  # type: ignore[name-defined]
        return []


def _pe_csv_dir(file: str) -> Optional[Path]:
    """Map a PE console-log filename to its run CSV directory.

    Console logs are '<scenario>_<ts>.stdout.log'; the engine writes CSVs under
    PE_RUNS/<scenario>/<run_ts>/. The panel ts and run ts differ, so we resolve
    the scenario name then take the newest timestamp subdir that has a
    fitness.csv. Falls back to stripping the trailing _YYYYMMDD-HHMMSS so a
    cloned-config stem still resolves."""
    if not PE_RUNS.exists():
        return None
    stem = file
    for suf in (".stdout.log", ".stderr.log", ".log"):
        if stem.endswith(suf):
            stem = stem[: -len(suf)]
            break
    candidates = [stem]
    stripped = _TS_TAIL_RE.sub("", stem)
    if stripped != stem:
        candidates.append(stripped)
    for name in candidates:
        scen = PE_RUNS / name
        if not scen.is_dir():
            continue
        subs = [d for d in scen.iterdir() if d.is_dir()
                and (d / "fitness.csv").exists()]
        if subs:
            return max(subs, key=lambda d: d.stat().st_mtime)
        if (scen / "fitness.csv").exists():
            return scen
    return None


def dp_spectrum_payload(file: str, window: int = 0) -> dict[str, Any]:
    """Read a DP metrics jsonl and return the full behavioural panorama+axes."""
    path = _safe_join(DP_METRICS, file)
    if not path.exists():
        return {"ok": False, "error": f"DP jsonl not found: {file}"}
    rows = _load_jsonl(path)
    if window:
        rows = rows[-int(window):]
    spec = spectrum.dp_spectrum(rows)
    if spec.get("ok"):
        spec["axes"] = spectrum.dp_axes(rows)
    return spec


def pe_spectrum_payload(file: str) -> dict[str, Any]:
    """Resolve a PE run's CSVs and return the full behavioural panorama+axes."""
    d = _pe_csv_dir(file)
    if d is None:
        return {"ok": False, "error": f"PE CSV dir not found for: {file}"}
    behaviors = _read_csv(d / "protagonist_behaviors.csv")
    fitness = _read_csv(d / "fitness.csv")
    evolution = _read_csv(d / "evolution.csv")
    spec = spectrum.pe_spectrum(behaviors, fitness, evolution)
    if spec.get("ok"):
        spec["axes"] = spectrum.pe_axes(behaviors, fitness, evolution)
        spec["csv_dir"] = str(d)
    return spec


@app.get("/api/spectrum")
def spectrum_endpoint(project: str, file: str, window: int = 0) -> dict[str, Any]:
    """Behavioural panorama for a run — the multi-dimensional view the agent and
    panel read to see 'the direction' (not just avg/score)."""
    if project == "dp":
        out = dp_spectrum_payload(file, window)
    elif project == "pe":
        out = pe_spectrum_payload(file)
    else:
        raise HTTPException(400, "project must be 'dp' or 'pe'")
    if not out.get("ok"):
        raise HTTPException(404, out.get("error", "spectrum unavailable"))
    # attach scene narration (画面 captions + verdict hint) for panel + agent
    try:
        nar = spectrum.narrate_dp(out) if project == "dp" else spectrum.narrate_pe(out)
        if nar.get("ok"):
            out["narration"] = {"headline": nar["headline"], "scenes": nar["scenes"],
                                "story": nar["story"],
                                "verdict_hint": {"action": nar["verdict_hint"][0],
                                                 "reason": nar["verdict_hint"][1]}}
    except Exception as e:  # noqa: BLE001
        out["narration_error"] = str(e)
    return out


# ----------------------------- traces / replay ----------------------------- #
def _count_jsonl_lines(path: Path) -> int:
    n = 0
    with open(path, "r", encoding="utf-8", errors="ignore") as fh:
        for _ in fh:
            n += 1
    return n


@app.get("/api/traces")
def traces() -> dict[str, Any]:
    """List replay traces for both viewers.

    PE: data/runs/<scenario>/<ts>/trace/generation_X/episode_Y/ with a
        viewer_frames.jsonl (+ viewer_world.json).
    DP: dump dirs (viewer_dump*, viewer_newworld, ...) each holding a
        frames.json + world.json straight from deep_protagonist_dump.
    """
    pe: list[dict[str, Any]] = []
    if PE_RUNS.exists():
        for jl in PE_RUNS.glob("*/*/trace/generation_*/episode_*/viewer_frames.jsonl"):
            try:
                rel = jl.relative_to(PE_RUNS)
                parts = rel.parts  # scenario/ts/trace/generation_X/episode_Y/file
                ep_dir = jl.parent
                pe.append({
                    "id": str(rel).replace("\\", "/"),
                    "scenario": parts[0],
                    "run": parts[1],
                    "generation": parts[3] if len(parts) > 3 else "",
                    "episode": parts[4] if len(parts) > 4 else "",
                    "frames_mb": round(jl.stat().st_size / 1e6, 2),
                    "has_world": (ep_dir / "viewer_world.json").exists(),
                    "dir": str(ep_dir).replace("\\", "/"),
                })
            except Exception:  # noqa: BLE001
                continue
        pe.sort(key=lambda d: (d["scenario"], d["run"], d["generation"], d["episode"]))

    dp: list[dict[str, Any]] = []
    if DP_ROOT.exists():
        dp_dirs = set(DP_ROOT.glob("viewer*"))
        if DP_TRACES.exists():
            dp_dirs |= set(DP_TRACES.glob("viewer*"))
        for d in sorted(dp_dirs):
            fj = d / "frames.json"
            wj = d / "world.json"
            if fj.exists() and wj.exists():
                dp.append({
                    "id": d.name,
                    "dir": str(d).replace("\\", "/"),
                    "frames_mb": round(fj.stat().st_size / 1e6, 2),
                    "world_mb": round(wj.stat().st_size / 1e6, 2),
                })

    return {"pe": pe[:500], "dp": dp,
            "pe_total": len(pe),
            "viewers": {"dp": "/viewer/dp/", "pe": "/viewer/pe/"}}


@app.get("/api/pe/live_trace")
def pe_live_trace(match: str = "") -> dict[str, Any]:
    """Newest PE replay trace (by mtime), for live-during-training 3D.

    PE writes a fresh viewer_frames.jsonl per generation/episode while training
    runs, so the most recently modified one is the closest-to-live world. The
    modal polls this and loads it into the viewer (few-generations latency, as
    the user accepts). `match` (optional) filters by scenario/run substring so a
    card shows ITS run; empty = newest across all PE runs.
    """
    if not PE_RUNS.exists():
        return {"ok": True, "trace_id": None, "n": 0}
    best = None
    best_mt = -1.0
    n = 0
    for jl in PE_RUNS.glob("*/*/trace/generation_*/episode_*/viewer_frames.jsonl"):
        rel = str(jl.relative_to(PE_RUNS)).replace("\\", "/")
        if match and match not in rel:
            continue
        n += 1
        try:
            mt = jl.stat().st_mtime
        except OSError:
            continue
        if mt > best_mt:
            best_mt = mt
            best = {"trace_id": rel,
                    "dir": str(jl.parent).replace("\\", "/"),
                    "has_world": (jl.parent / "viewer_world.json").exists(),
                    "mtime": datetime.fromtimestamp(mt).isoformat(timespec="seconds")}
    if best is None:
        return {"ok": True, "trace_id": None, "n": n, "match": match}
    return {"ok": True, "n": n, "match": match, **best}


class TraceClearReq(BaseModel):
    # PE: ids are <scenario>/<ts> run roots OR full trace ids; we delete the
    # whole trace/ subtree for a run by default. DP: viewer dir names.
    project: str
    ids: list[str]
    hard: bool = False


@app.post("/api/traces/clear")
def traces_clear(req: TraceClearReq) -> dict[str, Any]:
    removed, skipped = [], []
    if req.project == "pe":
        for rid in req.ids:
            # accept "<scenario>/<ts>" (delete its trace/ dir) or a deeper id
            target = _safe_join(PE_RUNS, rid)
            if target.name != "trace" and (target / "trace").exists():
                target = target / "trace"
            if not target.exists():
                skipped.append({"id": rid, "reason": "not found"})
                continue
            sz = _dir_size(target)
            if req.hard:
                shutil.rmtree(target)
            else:
                _trash(target, "pe_traces")
            removed.append({"id": rid, "freed_mb": round(sz / 1e6, 1)})
    else:
        for rid in req.ids:
            target = _safe_join(DP_ROOT, rid)
            # never delete the live viewer's data dir
            if target.resolve() == (DP_WEB3D / "data").resolve():
                skipped.append({"id": rid, "reason": "active viewer data"})
                continue
            if not target.exists():
                skipped.append({"id": rid, "reason": "not found"})
                continue
            sz = _dir_size(target)
            if req.hard:
                shutil.rmtree(target)
            else:
                _trash(target, "dp_traces")
            removed.append({"id": rid, "freed_mb": round(sz / 1e6, 1)})
    return {"ok": True, "removed": removed, "skipped": skipped,
            "mode": "hard" if req.hard else "trash"}


class ReplayLoadReq(BaseModel):
    project: str
    trace_id: str                        # PE: trace id (…/viewer_frames.jsonl); DP: viewer dir name
    downsample: int = 1                  # keep every Nth frame (1 = all)
    max_frames: int = 6000               # auto-downsample target ceiling


@app.post("/api/replay/load")
def replay_load(req: ReplayLoadReq) -> dict[str, Any]:
    """Load a chosen trace into the viewer's data dir (with optional downsample).

    PE: convert viewer_frames.jsonl -> {"frames":[...]} and copy viewer_world.json.
    DP: copy the dump dir's frames.json + world.json verbatim (already in the
        viewer's format), applying downsample if requested.
    """
    if req.project == "pe":
        jl = _safe_join(PE_RUNS, req.trace_id)
        if jl.name != "viewer_frames.jsonl":
            jl = jl / "viewer_frames.jsonl"
        if not jl.exists():
            raise HTTPException(404, f"trace not found: {req.trace_id}")
        total = _count_jsonl_lines(jl)
        step = max(req.downsample, 1)
        if total // step > req.max_frames:
            step = max(step, (total // req.max_frames) + 1)
        frames = []
        with open(jl, "r", encoding="utf-8", errors="ignore") as fh:
            for i, ln in enumerate(fh):
                if i % step:
                    continue
                ln = ln.strip()
                if ln:
                    try:
                        frames.append(json.loads(ln))
                    except json.JSONDecodeError:
                        pass
        data_dir = PE_WEB3D / "data"
        data_dir.mkdir(parents=True, exist_ok=True)
        (data_dir / "frames.json").write_text(json.dumps({"frames": frames}),
                                              encoding="utf-8")
        world_src = jl.parent / "viewer_world.json"
        world_ok = False
        if world_src.exists():
            shutil.copyfile(world_src, data_dir / "viewer_world.json")
            world_ok = True
        return {"ok": True, "project": "pe", "loaded_frames": len(frames),
                "total_frames": total, "downsample_step": step,
                "world_copied": world_ok, "viewer": "/viewer/pe/"}

    # DP
    d = _safe_join(DP_TRACES, req.trace_id)
    if not (d / "frames.json").exists():
        d = _safe_join(DP_ROOT, req.trace_id)
    fj, wj = d / "frames.json", d / "world.json"
    if not (fj.exists() and wj.exists()):
        raise HTTPException(404, f"DP trace dir missing frames/world: {req.trace_id}")
    data_dir = DP_WEB3D / "data"
    data_dir.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(wj, data_dir / "world.json")
    raw = json.loads(fj.read_text(encoding="utf-8", errors="ignore"))
    arr = raw.get("frames", raw) if isinstance(raw, dict) else raw
    total = len(arr)
    step = max(req.downsample, 1)
    if total // step > req.max_frames:
        step = max(step, (total // req.max_frames) + 1)
    out = arr[::step] if step > 1 else arr
    (data_dir / "frames.json").write_text(json.dumps({"frames": out}), encoding="utf-8")
    return {"ok": True, "project": "dp", "loaded_frames": len(out),
            "total_frames": total, "downsample_step": step, "viewer": "/viewer/dp/"}


# ------------------------------ new-world eval ----------------------------- #
class DPNewWorldReq(BaseModel):
    checkpoint: str                      # file under runs/
    seed: int = 3_000_000                # new world
    steps: int = 4000
    frames: int = 1500
    out_dir: str = "viewer_newworld"     # under DP_ROOT


@app.post("/api/dp/newworld")
def dp_newworld(req: DPNewWorldReq) -> dict[str, Any]:
    """Inference-only rollout of a checkpoint in a fresh world (no training).

    Uses deep_protagonist_dump (tools/scene_dump.cpp). If that exe isn't built
    yet we surface the exact cmake command instead of failing opaquely.
    """
    exe = _resolve(DP_DUMP_EXE)
    if not exe:
        return {"ok": False, "needs_build": True,
                "message": "deep_protagonist_dump not built",
                "build_hint": ("cmake --build routes/deep_protagonist/build_d122_v2 "
                               "--target deep_protagonist_dump")}
    ckpt = _dp_resolve(req.checkpoint)
    if not ckpt.exists():
        raise HTTPException(404, f"checkpoint not found: {req.checkpoint}")
    DP_TRACES.mkdir(parents=True, exist_ok=True)
    out_rel = str(Path("traces") / req.out_dir)
    out = _safe_join(DP_TRACES, req.out_dir)
    argv = [str(exe), "--load", str(ckpt.relative_to(DP_ROOT)),
            "--seed", str(req.seed), "--steps", str(req.steps),
            "--frames", str(req.frames), "--out-dir", out_rel]
    run_id = _new_run_id("dp")
    log_path = DP_LOGS / f"newworld_{datetime.now().strftime('%Y%m%d-%H%M%S')}.log"
    run = _spawn(run_id, "dp", argv, DP_ROOT, dict(os.environ), log_path,
                 kind="newworld",
                 meta={"checkpoint": req.checkpoint, "seed": req.seed,
                       "out_dir": req.out_dir})
    return {"ok": True, "run_id": run_id, "pid": run.proc.pid, "cmd": run.cmd,
            "out_dir": str(out).replace("\\", "/"), "log": str(log_path)}


# PE new-world is just /api/pe/train with seed + resume_checkpoint set, which
# clones the toml (new world + same model). The frontend posts there.


# ------------------------------ system status ------------------------------ #
@app.get("/api/system/status")
def system_status() -> dict[str, Any]:
    gpu = None
    try:
        out = subprocess.run(
            ["nvidia-smi", "--query-gpu=utilization.gpu,memory.used,memory.total",
             "--format=csv,noheader,nounits"],
            capture_output=True, text=True, timeout=4)
        if out.returncode == 0 and out.stdout.strip():
            rows = []
            for line in out.stdout.strip().splitlines():
                parts = [p.strip() for p in line.split(",")]
                rows.append({"util_pct": float(parts[0]),
                             "mem_used_mb": float(parts[1]),
                             "mem_total_mb": float(parts[2])})
            gpu = rows[0] if len(rows) == 1 else rows
    except Exception:  # noqa: BLE001
        gpu = None

    disk = None
    try:
        total, used, free = shutil.disk_usage(str(ROUTES_DIR))
        disk = {"total_gb": round(total / 1e9, 1), "used_gb": round(used / 1e9, 1),
                "free_gb": round(free / 1e9, 1)}
    except Exception:  # noqa: BLE001
        pass

    procs = processes()
    return {"gpu": gpu, "disk": disk,
            "dp_count": procs["dp_count"], "pe_count": procs["pe_count"],
            "time": datetime.now().isoformat(timespec="seconds")}


# ----------------------------- websocket logs ------------------------------ #
@app.websocket("/ws/logs")
async def ws_logs(ws: WebSocket) -> None:
    await ws.accept()
    run_id = ws.query_params.get("run_id", "")
    last = 0
    try:
        while True:
            run = RUNS.get(run_id)
            if run:
                buf = run.lines
                if len(buf) > last:
                    chunk = buf[last:]
                    last = len(buf)
                    await ws.send_json({"lines": chunk, "running": run.running})
                elif not run.running:
                    await ws.send_json({"lines": [], "running": False})
            else:
                await ws.send_json({"lines": [], "running": False, "missing": True})
            await asyncio.sleep(0.6)
    except WebSocketDisconnect:
        return
    except Exception:  # noqa: BLE001
        return


# Static assets + the two web3d viewers (mounted so the iframes can load them).
app.mount("/static", StaticFiles(directory=str(PANEL_DIR / "static")), name="static")
if DP_WEB3D.exists():
    app.mount("/viewer/dp", StaticFiles(directory=str(DP_WEB3D), html=True), name="dp_viewer")
if PE_WEB3D.exists():
    app.mount("/viewer/pe", StaticFiles(directory=str(PE_WEB3D), html=True), name="pe_viewer")


# ------------------------- LLM training agent ------------------------------ #
# The autonomous LLM orchestrator lives in agent.py and attaches its own
# /api/agent/* routes. Imported here so a single `uvicorn server:app` serves
# both the panel and the agent. Kept optional so the panel still boots if the
# agent module is absent.
try:
    import agent as _agent  # noqa: E402
    _agent.register_agent(app)
except Exception as _e:  # noqa: BLE001
    print(f"[panel] agent module not loaded: {_e}")


if __name__ == "__main__":
    import uvicorn
    port = int(os.environ.get("PANEL_PORT", "8070"))
    uvicorn.run(app, host="127.0.0.1", port=port)
