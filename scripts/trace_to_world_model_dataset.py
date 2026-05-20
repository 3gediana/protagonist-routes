"""trace_to_world_model_dataset.py

Stage 4 / P6 - World Model trace dataset builder.

Reads a protagonist training run's agent_trace.csv (which is sampled every
agent_interval ticks during training) and emits transition tuples suitable
for offline pretraining of a forward-prediction world model:

    (obs_t, action_t, obs_{t+1}, reward_delta, dt)

The "action" is reconstructed implicitly because protagonist routes do not
emit per-tick action vectors to disk (that would explode trace size).
Instead, we infer a coarse action encoding from the next-step velocity
delta and from is_carrying / goal_index transitions. That is good enough
for a recurrent world-model that learns "given the state and a small
abstract action, predict the next state".

Usage::

    python3 scripts/trace_to_world_model_dataset.py <run_dir> [--out path]

Where <run_dir> contains trace/generation_*/episode_*/agent_trace.csv files
(e.g. data/runs/protagonist_smoke/<timestamp>). The script aggregates
transitions across all episodes and writes a single CSV at
<run_dir>/world_model_transitions.csv (or the --out path).

This is deliberately format-agnostic - no torch/parquet dependency - so it
runs anywhere. A downstream pipeline can ingest the CSV however it likes.
"""

from __future__ import annotations

import argparse
import csv
import math
from pathlib import Path
from typing import Iterable


# Observation columns we keep, in the order they appear in agent_trace.csv.
# These define the state vector dimension. Episodic_top_type is excluded
# (string column) but episodic_event_count is kept as a scalar.
OBS_COLUMNS = [
    "x", "y", "vx", "vy",
    "energy", "age_seconds",
    "health_ratio", "hydration_ratio",
    "food_eaten", "plant_food", "meat_food",
    "visited_cells",
    "damage_dealt", "kills",
    "goal_index", "goal_commitment", "goal_elapsed_seconds", "goal_progress",
    "episodic_event_count", "episodic_top_salience",
    "social_relation_count", "social_best_coop", "social_best_comm", "social_best_hostility",
    "spatial_water_cells", "spatial_food_cells", "spatial_danger_cells",
    "spatial_fire_cells", "spatial_worksite_cells",
    "dnc_usage_mean", "dnc_read_peak",
]


# Action columns (mostly inferred / coarse). We add these as next_state -
# state deltas so a world model can learn to invert them in pretraining.
ACTION_COLUMNS = ["action_dvx", "action_dvy", "action_is_carrying", "action_goal_idx"]


def find_agent_traces(run_dir: Path) -> list[Path]:
    return sorted(run_dir.glob("trace/generation_*/episode_*/agent_trace.csv"))


def parse_float(s: str, default: float = 0.0) -> float:
    try:
        return float(s)
    except (ValueError, TypeError):
        return default


def parse_int(s: str, default: int = 0) -> int:
    try:
        return int(float(s))
    except (ValueError, TypeError):
        return default


def build_transitions(trace_paths: Iterable[Path], out_path: Path) -> tuple[int, int]:
    """Walk every (gen, episode) trace and emit transitions per agent_id.

    Returns (rows_written, episodes_consumed).
    """
    episodes = 0
    rows_written = 0
    out_path.parent.mkdir(parents=True, exist_ok=True)

    header = (
        ["episode_path", "agent_id", "t_step", "t_seconds", "dt_seconds"]
        + [f"obs_{c}" for c in OBS_COLUMNS]
        + ACTION_COLUMNS
        + [f"next_obs_{c}" for c in OBS_COLUMNS]
        + ["reward_delta"]
    )

    with out_path.open("w", encoding="utf-8", newline="") as out_f:
        writer = csv.writer(out_f)
        writer.writerow(header)

        for path in trace_paths:
            episodes += 1
            # Group rows by agent_id, then sort by step_index.
            per_agent: dict[int, list[dict]] = {}
            with path.open(encoding="utf-8") as f:
                reader = csv.DictReader(f)
                for row in reader:
                    aid = parse_int(row.get("agent_id", "0"))
                    per_agent.setdefault(aid, []).append(row)

            for aid, rows in per_agent.items():
                rows.sort(key=lambda r: parse_int(r.get("step_index", "0")))
                for i in range(len(rows) - 1):
                    cur, nxt = rows[i], rows[i + 1]
                    # Skip dead-state transitions.
                    if parse_int(cur.get("alive", "1")) == 0:
                        continue
                    t_cur = parse_float(cur.get("time_seconds", "0"))
                    t_nxt = parse_float(nxt.get("time_seconds", "0"))
                    dt = max(1e-4, t_nxt - t_cur)
                    if dt <= 0:
                        continue

                    obs_cur = [parse_float(cur.get(c, "0")) for c in OBS_COLUMNS]
                    obs_nxt = [parse_float(nxt.get(c, "0")) for c in OBS_COLUMNS]

                    # Inferred action signal: velocity delta + is_carrying +
                    # current goal as a coarse label. World model can ignore
                    # action_goal_idx if it only wants kinematics.
                    action = [
                        parse_float(nxt.get("vx", "0")) - parse_float(cur.get("vx", "0")),
                        parse_float(nxt.get("vy", "0")) - parse_float(cur.get("vy", "0")),
                        parse_float(cur.get("is_carrying", "0")),
                        parse_float(cur.get("goal_index", "0")),
                    ]

                    reward_delta = parse_float(nxt.get("fitness", "0")) - parse_float(cur.get("fitness", "0"))

                    writer.writerow([
                        str(path),
                        aid,
                        parse_int(cur.get("step_index", "0")),
                        t_cur,
                        dt,
                        *obs_cur,
                        *action,
                        *obs_nxt,
                        reward_delta,
                    ])
                    rows_written += 1

    return rows_written, episodes


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("run_dir", help="Protagonist run dir (contains trace/gen*/ep*)")
    ap.add_argument("--out", default=None,
                    help="Output CSV path. Default: <run_dir>/world_model_transitions.csv")
    args = ap.parse_args()

    run_dir = Path(args.run_dir).resolve()
    out_path = Path(args.out).resolve() if args.out else run_dir / "world_model_transitions.csv"

    paths = find_agent_traces(run_dir)
    if not paths:
        print(f"[trace2wm] no agent_trace.csv found under {run_dir}")
        return 1

    rows, episodes = build_transitions(paths, out_path)
    print(f"[trace2wm] consumed {episodes} episode trace(s) from {run_dir}")
    print(f"[trace2wm] wrote {rows} transition rows to {out_path}")
    print(f"[trace2wm] obs_dim={len(OBS_COLUMNS)} action_dim={len(ACTION_COLUMNS)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
