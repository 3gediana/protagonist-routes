"""protagonist_ecology trajectory analyzer.

Reads `agent_trace.csv` (per-tick per-agent state from TraceRecorder)
for one episode and produces a structured text report characterising
agent behaviour. Designed to answer the question: "are the trained
agents wandering as 'headless flies' or do they exhibit coherent,
recognisable behaviour loops?".

Usage:
    python protagonist_trajectory_analyze.py <trace_dir> [--out report.md]

Where <trace_dir> is e.g.
    data/runs/long_run_80gen_stage4b_s58193/<ts>/trace/generation_79/episode_0

Outputs:
  - Per-agent behavioural summary: spatial extent, goal commitment,
    velocity coherence, social activity, productive output
  - Top-K most "focused" / most "exploring" / most "social" agents
  - Aggregate "headless flies" diagnostic: are agents purposeful?

This tool is the data-only end of the pipeline -- it doesn't show
visualisations to the user. Designed per the 2026-05-29 directive
to keep raw trace files transient and persist only the analysis.
"""

from __future__ import annotations

import argparse
import json
import math
import sys
from collections import Counter
from dataclasses import dataclass
from pathlib import Path

import numpy as np

try:
    import pandas as pd
except ImportError:
    print("ERROR: pandas not installed. pip install pandas", file=sys.stderr)
    sys.exit(2)


# ============================================================
# Data model
# ============================================================

ARCHETYPE_NAMES = {
    # background species ids in route config (see ProtagonistEcologyConfig)
    # Note: agent_trace.csv has species_id, not archetype_tag, so we map
    # species_id->name based on the protagonist_ecology convention.
    0: "protagonist",
    1: "small_forager",
    2: "large_herbivore",
    3: "rabbit",
    4: "scavenger",
    5: "omnivore",
    6: "pack_hunter",
    7: "apex_predator",
    8: "ambush_predator",
}


@dataclass
class AgentSummary:
    agent_id: int
    species_id: int
    species_name: str
    genome_id: int

    # lifetime
    first_t: float
    last_t: float
    lifetime_s: float
    final_alive: bool
    final_fitness: float

    # spatial
    n_samples: int
    mean_x: float
    mean_y: float
    bbox_w: float
    bbox_h: float
    bbox_area: float
    path_length: float          # cumulative Euclidean distance
    mean_speed: float           # path_length / lifetime
    visited_cells: int          # from world model (10x10 grid by default)

    # purposefulness signals
    velocity_autocorr: float    # mean dot(v_t, v_{t+1}) / (|v_t||v_{t+1}|),
                                # high = consistent direction, low = jittery
    radius_of_gyration: float   # sqrt(mean((x - mean_x)^2 + (y - mean_y)^2))
    direction_persistence: float  # fraction of samples whose velocity
                                  # heading is within +-45deg of the
                                  # previous sample's heading

    # goal commitment
    goal_switches: int          # number of changes in goal_name
    dominant_goal: str
    dominant_goal_frac: float
    goal_distribution: dict     # goal_name -> fraction

    # social
    social_relations_peak: int
    best_coop_peak: float
    best_hostility_peak: float

    # productivity
    food_eaten_final: float
    kills_final: int
    damage_dealt_final: float

    # memory
    episodic_events_final: int
    dominant_episodic_event: str

    # health
    health_min: float
    hydration_min: float


# ============================================================
# Loading
# ============================================================

def load_agent_trace(trace_dir: Path) -> pd.DataFrame:
    csv = trace_dir / "agent_trace.csv"
    if not csv.exists():
        raise FileNotFoundError(f"agent_trace.csv not found in {trace_dir}")
    df = pd.read_csv(csv)
    # 2026-05-29: protect against an empty trace file (CAP'd or zero ticks).
    if df.empty:
        return df
    df = df.sort_values(["agent_id", "step_index"]).reset_index(drop=True)
    return df


def load_manifest(trace_dir: Path) -> dict:
    p = trace_dir / "replay_manifest.json"
    if not p.exists():
        return {}
    return json.loads(p.read_text())


# ============================================================
# Per-agent analytics
# ============================================================

def summarise_agent(df_agent: pd.DataFrame) -> AgentSummary:
    n = len(df_agent)
    species_id = int(df_agent["species_id"].iloc[0])

    # lifetime
    first_t = float(df_agent["time_seconds"].iloc[0])
    last_t = float(df_agent["time_seconds"].iloc[-1])
    lifetime_s = last_t - first_t
    final_alive = bool(df_agent["alive"].iloc[-1])
    final_fitness = float(df_agent["fitness"].iloc[-1])

    # spatial
    xs = df_agent["x"].to_numpy(dtype=float)
    ys = df_agent["y"].to_numpy(dtype=float)
    bbox_w = float(xs.max() - xs.min())
    bbox_h = float(ys.max() - ys.min())
    bbox_area = bbox_w * bbox_h

    if n >= 2:
        path_length = float(np.hypot(np.diff(xs), np.diff(ys)).sum())
    else:
        path_length = 0.0
    mean_speed = path_length / max(lifetime_s, 1e-6)

    mean_x = float(xs.mean())
    mean_y = float(ys.mean())
    radius_of_gyration = float(
        math.sqrt(((xs - mean_x) ** 2 + (ys - mean_y) ** 2).mean())
        if n > 0
        else 0.0
    )

    # velocity autocorrelation: mean of dot(v_t, v_{t+1}) normalised by
    # |v_t||v_{t+1}|. NaN-safe when an agent is stationary.
    vx = df_agent["vx"].to_numpy(dtype=float)
    vy = df_agent["vy"].to_numpy(dtype=float)
    if n >= 3:
        v1x, v1y = vx[:-1], vy[:-1]
        v2x, v2y = vx[1:], vy[1:]
        dots = v1x * v2x + v1y * v2y
        mags = np.hypot(v1x, v1y) * np.hypot(v2x, v2y)
        mask = mags > 1e-6
        if mask.any():
            velocity_autocorr = float((dots[mask] / mags[mask]).mean())
        else:
            velocity_autocorr = 0.0
        # direction persistence: fraction of consecutive vectors whose
        # cosine similarity > 0.707 (i.e. within ~45deg)
        cos = np.where(mags > 1e-6, dots / np.maximum(mags, 1e-9), 0.0)
        direction_persistence = float((cos > 0.707).mean())
    else:
        velocity_autocorr = 0.0
        direction_persistence = 0.0

    # goal commitment
    goal_names = df_agent["goal_name"].astype(str).to_numpy()
    goal_counts = Counter(goal_names)
    total_goal_samples = goal_counts.total() if hasattr(goal_counts, "total") else sum(goal_counts.values())
    goal_distribution = {g: c / total_goal_samples for g, c in goal_counts.items()}
    dominant_goal, dom_count = goal_counts.most_common(1)[0]
    dominant_goal_frac = dom_count / total_goal_samples
    # goal switches: number of times the goal name changes between
    # consecutive samples
    goal_switches = int((goal_names[:-1] != goal_names[1:]).sum()) if n >= 2 else 0

    # social
    social_relations_peak = int(df_agent["social_relation_count"].max())
    best_coop_peak = float(df_agent["social_best_coop"].max())
    best_hostility_peak = float(df_agent["social_best_hostility"].max())

    # productivity (final values)
    food_eaten_final = float(df_agent["food_eaten"].iloc[-1])
    kills_final = int(df_agent["kills"].iloc[-1])
    damage_dealt_final = float(df_agent["damage_dealt"].iloc[-1])

    # memory
    episodic_events_final = int(df_agent["episodic_event_count"].iloc[-1])
    ep_types = df_agent["episodic_top_type"].astype(str)
    ep_types = ep_types[ep_types != "0"]  # drop "no event" placeholder
    if len(ep_types) > 0:
        dominant_episodic_event = Counter(ep_types).most_common(1)[0][0]
    else:
        dominant_episodic_event = "(none)"

    # health
    health_min = float(df_agent["health_ratio"].min())
    hydration_min = float(df_agent["hydration_ratio"].min())

    # visited_cells: final value is the running maximum of unique cells
    # the agent has touched.
    visited_cells = int(df_agent["visited_cells"].iloc[-1])

    return AgentSummary(
        agent_id=int(df_agent["agent_id"].iloc[0]),
        species_id=species_id,
        species_name=ARCHETYPE_NAMES.get(species_id, f"species_{species_id}"),
        genome_id=int(df_agent["genome_id"].iloc[0]),
        first_t=first_t,
        last_t=last_t,
        lifetime_s=lifetime_s,
        final_alive=final_alive,
        final_fitness=final_fitness,
        n_samples=n,
        mean_x=mean_x,
        mean_y=mean_y,
        bbox_w=bbox_w,
        bbox_h=bbox_h,
        bbox_area=bbox_area,
        path_length=path_length,
        mean_speed=mean_speed,
        visited_cells=visited_cells,
        velocity_autocorr=velocity_autocorr,
        radius_of_gyration=radius_of_gyration,
        direction_persistence=direction_persistence,
        goal_switches=goal_switches,
        dominant_goal=dominant_goal,
        dominant_goal_frac=dominant_goal_frac,
        goal_distribution=goal_distribution,
        social_relations_peak=social_relations_peak,
        best_coop_peak=best_coop_peak,
        best_hostility_peak=best_hostility_peak,
        food_eaten_final=food_eaten_final,
        kills_final=kills_final,
        damage_dealt_final=damage_dealt_final,
        episodic_events_final=episodic_events_final,
        dominant_episodic_event=dominant_episodic_event,
        health_min=health_min,
        hydration_min=hydration_min,
    )


# ============================================================
# Aggregate / report
# ============================================================

def fmt_goal_dist(goal_dist: dict, limit: int = 4) -> str:
    items = sorted(goal_dist.items(), key=lambda kv: kv[1], reverse=True)[:limit]
    return ", ".join(f"{g}={f*100:.0f}%" for g, f in items)


def write_biography(summary: AgentSummary, fh) -> None:
    fh.write(f"### agent_id={summary.agent_id} "
             f"({summary.species_name}, genome_id={summary.genome_id})\n")
    fh.write(f"- lifetime: {summary.lifetime_s:.1f}s "
             f"(samples={summary.n_samples}, "
             f"alive_at_end={summary.final_alive}, "
             f"final_fitness={summary.final_fitness:.1f})\n")
    fh.write(f"- spatial: bbox={summary.bbox_w:.0f}x{summary.bbox_h:.0f} "
             f"(area={summary.bbox_area:.0f} u^2 / map 360000), "
             f"R_gyration={summary.radius_of_gyration:.1f}, "
             f"path_length={summary.path_length:.1f}, "
             f"mean_speed={summary.mean_speed:.2f} u/s, "
             f"visited_cells={summary.visited_cells}\n")
    fh.write(f"- purposefulness: vel_autocorr={summary.velocity_autocorr:+.3f} "
             f"(1.0=straight line, 0=random walk, -1=oscillates), "
             f"direction_persistence={summary.direction_persistence:.2f}\n")
    fh.write(f"- goals: dominant='{summary.dominant_goal}' "
             f"({summary.dominant_goal_frac*100:.0f}%), "
             f"switches={summary.goal_switches}, "
             f"distribution={{{fmt_goal_dist(summary.goal_distribution)}}}\n")
    fh.write(f"- social: relations_peak={summary.social_relations_peak}, "
             f"best_coop={summary.best_coop_peak:.3f}, "
             f"best_hostility={summary.best_hostility_peak:.3f}\n")
    fh.write(f"- output: food_eaten={summary.food_eaten_final:.1f}, "
             f"kills={summary.kills_final}, "
             f"damage_dealt={summary.damage_dealt_final:.1f}\n")
    fh.write(f"- memory: episodic_events={summary.episodic_events_final}, "
             f"dominant_event='{summary.dominant_episodic_event}'\n")
    fh.write(f"- health: min_health={summary.health_min:.2f}, "
             f"min_hydration={summary.hydration_min:.2f}\n")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("trace_dir", type=Path,
                        help="path to a generation_X/episode_Y trace dir")
    parser.add_argument("--out", type=Path, default=None,
                        help="output report path (default: stdout)")
    parser.add_argument("--top-k", type=int, default=3,
                        help="how many agents per category to highlight")
    parser.add_argument("--protagonist-only", action="store_true",
                        help="restrict per-agent reports to species_id=0")
    parser.add_argument("--brained-only", action="store_true",
                        help="restrict to agents whose goal_name was ever != 'na' "
                             "(i.e. the 24 NEAT-evolved brains, excluding NPCs)")
    args = parser.parse_args()

    trace_dir = args.trace_dir.resolve()
    if not trace_dir.exists():
        print(f"ERROR: {trace_dir} does not exist", file=sys.stderr)
        return 1

    df = load_agent_trace(trace_dir)
    manifest = load_manifest(trace_dir)

    if df.empty:
        print(f"WARNING: agent_trace.csv in {trace_dir} is empty", file=sys.stderr)
        return 1

    # group by agent_id
    summaries: list[AgentSummary] = []
    brained_ids: set[int] = set()
    if args.brained_only:
        # Cache: an agent_id is considered "brained" iff it ever
        # carried a non-'na' goal_name during the episode. Excludes
        # NPCs/environmental agents that show up in AgentLayer but
        # do not run a NEAT brain.
        brained_ids = set(
            df.loc[df["goal_name"].astype(str) != "na", "agent_id"]
              .astype(int)
              .unique()
              .tolist()
        )
    for _, df_agent in df.groupby("agent_id", sort=True):
        if args.protagonist_only and int(df_agent["species_id"].iloc[0]) != 0:
            continue
        if args.brained_only and int(df_agent["agent_id"].iloc[0]) not in brained_ids:
            continue
        s = summarise_agent(df_agent)
        summaries.append(s)

    if not summaries:
        print("WARNING: no agents to summarise", file=sys.stderr)
        return 1

    # aggregate
    total_agents = len(summaries)
    by_species: dict[str, list[AgentSummary]] = {}
    for s in summaries:
        by_species.setdefault(s.species_name, []).append(s)

    mean_goal_switches = float(np.mean([s.goal_switches for s in summaries]))
    mean_vel_autocorr = float(np.mean([s.velocity_autocorr for s in summaries]))
    mean_dir_persistence = float(np.mean([s.direction_persistence for s in summaries]))
    mean_bbox_area = float(np.mean([s.bbox_area for s in summaries]))
    mean_dominant_frac = float(np.mean([s.dominant_goal_frac for s in summaries]))
    mean_path_per_sample = float(np.mean(
        [s.path_length / max(s.n_samples, 1) for s in summaries]
    ))

    # "headless flies" verdict
    headless_score = (
        (1.0 - mean_vel_autocorr) * 0.4 +       # low autocorr = jittery
        (1.0 - mean_dir_persistence) * 0.3 +    # low persistence = no commitment
        (1.0 - mean_dominant_frac) * 0.3        # low goal commitment
    )
    # mapping: 0.0 = perfectly purposeful; 1.0 = pure noise
    if headless_score < 0.35:
        verdict = "PURPOSEFUL"
    elif headless_score < 0.55:
        verdict = "MIXED"
    else:
        verdict = "HEADLESS_FLIES"

    # rank for representative pick
    summaries_focused = sorted(
        summaries,
        key=lambda s: (s.dominant_goal_frac, s.direction_persistence),
        reverse=True,
    )
    summaries_explorer = sorted(
        summaries,
        key=lambda s: (s.bbox_area, s.path_length),
        reverse=True,
    )
    summaries_productive = sorted(
        summaries,
        key=lambda s: (s.food_eaten_final, s.kills_final, s.damage_dealt_final),
        reverse=True,
    )
    summaries_social = sorted(
        summaries,
        key=lambda s: (s.social_relations_peak, s.best_coop_peak),
        reverse=True,
    )
    summaries_fittest = sorted(
        summaries,
        key=lambda s: s.final_fitness,
        reverse=True,
    )

    # render
    import io
    buf = io.StringIO()
    buf.write("# protagonist_ecology trajectory report\n\n")
    buf.write(f"- trace_dir: `{trace_dir}`\n")
    if manifest:
        buf.write(f"- generation: {manifest.get('generation', '?')}, "
                  f"episode: {manifest.get('episode', '?')}, "
                  f"world_role: {manifest.get('world_role', '?')}, "
                  f"scenario: {manifest.get('scenario_name', '?')}, "
                  f"seed: {manifest.get('random_seed', '?')}\n")
    buf.write(f"- total_agents_analysed: {total_agents}\n")
    buf.write(f"- by_species: { {k: len(v) for k, v in by_species.items()} }\n\n")

    buf.write("## Aggregate purposefulness diagnostic\n\n")
    buf.write(f"- mean velocity_autocorr: {mean_vel_autocorr:+.3f} "
              f"(>0.5 = strongly directional, ~0 = random walk, <0 = jittery)\n")
    buf.write(f"- mean direction_persistence: {mean_dir_persistence:.3f} "
              f"(fraction of consecutive heading changes <45deg)\n")
    buf.write(f"- mean dominant_goal_frac: {mean_dominant_frac:.3f} "
              f"(time spent on the most-frequent goal)\n")
    buf.write(f"- mean goal_switches: {mean_goal_switches:.1f}\n")
    buf.write(f"- mean bbox_area: {mean_bbox_area:.0f} (map=360000)\n")
    buf.write(f"- mean path_per_sample: {mean_path_per_sample:.2f} u\n")
    buf.write(f"- headless_score: {headless_score:.3f} -> **VERDICT: {verdict}**\n\n")

    buf.write("## Top representative agents\n\n")
    buf.write(f"### Most focused (top {args.top_k} by dominant_goal_frac)\n")
    for s in summaries_focused[:args.top_k]:
        write_biography(s, buf)
    buf.write(f"\n### Most exploring (top {args.top_k} by bbox_area)\n")
    for s in summaries_explorer[:args.top_k]:
        write_biography(s, buf)
    buf.write(f"\n### Most productive (top {args.top_k} by food/kills)\n")
    for s in summaries_productive[:args.top_k]:
        write_biography(s, buf)
    buf.write(f"\n### Most social (top {args.top_k} by social_relations_peak)\n")
    for s in summaries_social[:args.top_k]:
        write_biography(s, buf)
    buf.write(f"\n### Highest final fitness (top {args.top_k})\n")
    for s in summaries_fittest[:args.top_k]:
        write_biography(s, buf)

    # per-species summary table
    buf.write("\n## Per-species aggregate\n\n")
    buf.write("| species | n | mean_lifetime_s | mean_path | mean_vel_autocorr | mean_dom_goal_frac | mean_food_eaten |\n")
    buf.write("|---|---:|---:|---:|---:|---:|---:|\n")
    for sp_name, agents in sorted(by_species.items()):
        n = len(agents)
        ml = float(np.mean([a.lifetime_s for a in agents]))
        mp = float(np.mean([a.path_length for a in agents]))
        mva = float(np.mean([a.velocity_autocorr for a in agents]))
        mdg = float(np.mean([a.dominant_goal_frac for a in agents]))
        mfe = float(np.mean([a.food_eaten_final for a in agents]))
        buf.write(f"| {sp_name} | {n} | {ml:.1f} | {mp:.1f} | {mva:+.3f} | {mdg:.3f} | {mfe:.1f} |\n")

    out = args.out
    text = buf.getvalue()
    if out is None:
        print(text)
    else:
        out.write_text(text)
        print(f"wrote report to {out} ({len(text)} bytes)", file=sys.stderr)

    return 0


if __name__ == "__main__":
    sys.exit(main())
