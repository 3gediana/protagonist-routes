"""protagonist_communication_mi.py

A3.S1 - offline mutual-information report for the protagonist signal channels.

Reads a run directory's `_communication_metrics.csv` (per-channel emit / response /
policy-shift summary written by ProtagonistEcologyRuntime.evaluatePopulation)
and `agent_trace.csv` (per-tick agent state from TraceRecorder) and computes
MI between each signal channel's "active in companion radius this tick" indicator
and four task-relevant environment-state variables:

  - food_nearby (anything > 0 in `nearest_food_dist` within food_threshold)
  - water_nearby (`hydration_ratio` below threshold)
  - danger_nearby (`predator_nearby_count` > 0)
  - worksite_nearby (`worksite_distance` below threshold)

Output: `<run_dir>/communication_mi_report.csv` with columns:
  scenario_name,channel,variable,mutual_information_bits,
  active_count,silent_count,positive_count,negative_count

Usage::

    python3 scripts/protagonist_communication_mi.py <run_dir>
    python3 scripts/protagonist_communication_mi.py data/runs/protagonist_train_*/

Notes:
  - Pure-stdlib implementation; no sklearn / numpy / pandas dependency.
  - MI is computed in bits (log base 2).
  - If `agent_trace.csv` columns do not exist (e.g. older traces), the
    corresponding env-state variable rows are skipped silently.
  - This is an OFFLINE diagnostic; it is read-only on the input files.

WORLD_UNDERSTANDING § 7.4 maps each channel to a hypothesised meaning
(food/water/danger/worksite), but A3 explicitly uses task-pressure-driven
semantic emergence rather than hard-coded channel assignment, so we report
MI for every (channel, variable) pair and let the reader pick the strongest
pair per variable as evidence of emergent semantics.
"""

from __future__ import annotations

import csv
import math
import sys
from collections import Counter, defaultdict
from pathlib import Path
from typing import Iterable

REPO_ROOT = Path(__file__).resolve().parent.parent

# Thresholds for "nearby" classification (tunable per task; defaults match
# the audit's spatial perception radii).
FOOD_NEAR_M = 18.0
WATER_LOW_RATIO = 0.5
WORKSITE_NEAR_M = 28.0


def mutual_information_bits(x: list[int], y: list[int]) -> float:
    """MI in bits between two equal-length sequences of small ints.

    H(X) + H(Y) - H(X, Y). Uses joint histogram, no smoothing.
    """
    if len(x) != len(y) or not x:
        return 0.0
    n = len(x)
    cx = Counter(x)
    cy = Counter(y)
    cxy = Counter(zip(x, y))
    hx = sum(-(c / n) * math.log2(c / n) for c in cx.values() if c > 0)
    hy = sum(-(c / n) * math.log2(c / n) for c in cy.values() if c > 0)
    hxy = sum(-(c / n) * math.log2(c / n) for c in cxy.values() if c > 0)
    return max(0.0, hx + hy - hxy)


def parse_trace_csv(path: Path) -> dict[int, dict[str, list[float]]]:
    """Group agent_trace.csv rows by agent_id, return per-agent column lists."""
    out: dict[int, dict[str, list[float]]] = defaultdict(lambda: defaultdict(list))
    if not path.exists():
        return out
    with path.open("r", newline="") as f:
        reader = csv.DictReader(f)
        for row in reader:
            try:
                agent_id = int(row.get("agent_id", -1))
            except (TypeError, ValueError):
                continue
            for k, v in row.items():
                if k == "agent_id":
                    continue
                try:
                    out[agent_id][k].append(float(v))
                except (TypeError, ValueError):
                    out[agent_id][k].append(float("nan"))
    return out


def derive_env_states(per_agent: dict[int, dict[str, list[float]]]) -> dict[str, list[int]]:
    """Build env-state indicator sequences (concatenated across agents)."""
    keys_of_interest = {
        "food_nearby": ("nearest_food_dist", lambda d: 1 if 0.0 < d < FOOD_NEAR_M else 0),
        "water_nearby": ("hydration_ratio", lambda h: 1 if h < WATER_LOW_RATIO else 0),
        "danger_nearby": ("predator_nearby_count", lambda c: 1 if c > 0.5 else 0),
        "worksite_nearby": ("worksite_distance", lambda d: 1 if 0.0 < d < WORKSITE_NEAR_M else 0),
    }
    env_seqs: dict[str, list[int]] = {name: [] for name in keys_of_interest}
    for agent_id, cols in per_agent.items():
        for env_name, (col_name, predicate) in keys_of_interest.items():
            if col_name not in cols:
                continue
            for v in cols[col_name]:
                if math.isnan(v):
                    continue
                env_seqs[env_name].append(predicate(v))
    return env_seqs


def derive_channel_active_from_signal_cols(
    per_agent: dict[int, dict[str, list[float]]],
    num_channels: int = 4,
) -> dict[int, list[int]]:
    """If agent_trace exposes per-channel signal columns, derive active sequence.

    Searches for columns named `signal_ch{c}_intensity` or `recent_signal_salience`
    (the audit's listener-side readback). Falls back to a single channel-0
    sequence built from `recent_signal_salience` if no per-channel columns
    exist.
    """
    by_channel: dict[int, list[int]] = {c: [] for c in range(num_channels)}
    for agent_id, cols in per_agent.items():
        for c in range(num_channels):
            col_name = f"signal_ch{c}_intensity"
            if col_name in cols:
                for v in cols[col_name]:
                    by_channel[c].append(1 if (not math.isnan(v) and v > 0.1) else 0)
        # Fallback: use recent_signal_salience as channel-0 proxy if no
        # per-channel intensity columns exist (older trace format).
        if not by_channel[0] and "recent_signal_salience" in cols:
            for v in cols["recent_signal_salience"]:
                by_channel[0].append(1 if (not math.isnan(v) and v > 0.05) else 0)
    return by_channel


def truncate_to_min_len(*seqs: Iterable[int]) -> tuple[list[int], ...]:
    """Trim all sequences to the shortest length so MI sees equal-length pairs."""
    lists = [list(s) for s in seqs]
    if not lists:
        return tuple()
    m = min(len(s) for s in lists)
    return tuple(s[:m] for s in lists)


def process_run(run_dir: Path, num_channels: int = 4) -> Path | None:
    """Process one run; write report; return report path or None on failure."""
    trace_paths = list((run_dir / "trace").glob("generation_*/episode_*/agent_trace.csv"))
    if not trace_paths:
        # Memory ablation eval writes trace under
        #   trace/<task>/<variant>/episode_*/agent_trace.csv
        # so we fall back to a recursive glob if the training-style
        # generation_* layout is empty.
        trace_paths = list((run_dir / "trace").rglob("agent_trace.csv"))
    if not trace_paths:
        # Allow callers to point directly at a leaf <task>/<variant>
        # subtree (so per-(task,variant) MI reports stay separable
        # instead of being averaged together at run-root level).
        trace_paths = list(run_dir.rglob("agent_trace.csv"))
    if not trace_paths:
        print(f"[skip] no trace found under {run_dir}/trace/")
        return None

    # Aggregate per-agent columns across all episodes in this run.
    aggregated: dict[int, dict[str, list[float]]] = defaultdict(lambda: defaultdict(list))
    for tp in trace_paths:
        partial = parse_trace_csv(tp)
        for agent_id, cols in partial.items():
            for k, vs in cols.items():
                aggregated[agent_id][k].extend(vs)

    env_seqs = derive_env_states(aggregated)
    channel_seqs = derive_channel_active_from_signal_cols(aggregated, num_channels)

    out_rows: list[dict[str, str]] = []
    scenario_name = run_dir.name
    for ch in range(num_channels):
        ch_seq = channel_seqs.get(ch, [])
        if not ch_seq:
            continue
        active_count = sum(ch_seq)
        silent_count = len(ch_seq) - active_count
        for var_name, env_seq in env_seqs.items():
            if not env_seq:
                continue
            ch_trim, env_trim = truncate_to_min_len(ch_seq, env_seq)
            mi = mutual_information_bits(ch_trim, env_trim)
            positive_count = sum(env_trim)
            negative_count = len(env_trim) - positive_count
            out_rows.append({
                "scenario_name": scenario_name,
                "channel": str(ch),
                "variable": var_name,
                "mutual_information_bits": f"{mi:.5f}",
                "active_count": str(active_count),
                "silent_count": str(silent_count),
                "positive_count": str(positive_count),
                "negative_count": str(negative_count),
            })

    if not out_rows:
        print(f"[skip] {run_dir.name}: no usable trace columns")
        return None

    report_path = run_dir / "communication_mi_report.csv"
    with report_path.open("w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=list(out_rows[0].keys()))
        writer.writeheader()
        writer.writerows(out_rows)
    print(f"[ok] wrote {report_path} ({len(out_rows)} rows)")
    return report_path


def main(argv: list[str]) -> int:
    if len(argv) < 2:
        print(__doc__)
        return 1
    for arg in argv[1:]:
        run_dir = Path(arg).resolve()
        if not run_dir.is_dir():
            # Glob support: data/runs/protagonist_train_*/
            for candidate in REPO_ROOT.glob(arg.replace("\\", "/")):
                if candidate.is_dir():
                    process_run(candidate)
            continue
        process_run(run_dir)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
