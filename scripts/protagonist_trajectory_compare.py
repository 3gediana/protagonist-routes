"""Compare two trajectory_analyze JSON summaries (e.g. gen 3 vs gen 79).

Reads two JSONs produced by `protagonist_trajectory_analyze.py --out`
and emits a side-by-side diff of the aggregate diagnostics plus
top-K agents per category.

Usage:
    python protagonist_trajectory_compare.py <early.json> <late.json> \
        [--label-a "gen 3"] [--label-b "gen 79"] [--out cmp.md]
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("early_json", type=Path)
    parser.add_argument("late_json", type=Path)
    parser.add_argument("--label-a", default="EARLY")
    parser.add_argument("--label-b", default="LATE")
    parser.add_argument("--out", type=Path, default=None)
    args = parser.parse_args()

    if not args.early_json.exists() or not args.late_json.exists():
        print("missing input json", file=sys.stderr)
        return 1
    a = json.loads(args.early_json.read_text())
    b = json.loads(args.late_json.read_text())

    A, B = args.label_a, args.label_b
    import io
    buf = io.StringIO()
    buf.write(f"# trajectory comparison: {A} vs {B}\n\n")
    buf.write(f"- {A}: `{args.early_json}` (n={a['total_agents_analysed']}, "
              f"manifest_gen={a.get('manifest',{}).get('generation','?')})\n")
    buf.write(f"- {B}: `{args.late_json}` (n={b['total_agents_analysed']}, "
              f"manifest_gen={b.get('manifest',{}).get('generation','?')})\n\n")

    buf.write("## Aggregate diagnostic\n\n")
    buf.write(f"| metric | {A} | {B} | delta |\n")
    buf.write("|---|---:|---:|---:|\n")
    for key in ("mean_velocity_autocorr",
                "mean_direction_persistence",
                "mean_dominant_goal_frac",
                "mean_goal_switches",
                "mean_bbox_area",
                "mean_path_per_sample",
                "headless_score"):
        va = a["aggregate"][key]
        vb = b["aggregate"][key]
        delta = vb - va
        buf.write(f"| {key} | {va:.3f} | {vb:.3f} | {delta:+.3f} |\n")
    buf.write(f"\n- verdict: **{A}**={a['aggregate']['verdict']}, "
              f"**{B}**={b['aggregate']['verdict']}\n\n")

    # Goal distribution shift (aggregate across all agents)
    def agg_goal_dist(payload):
        merged: dict = {}
        for ag in payload["agents"]:
            for g, f in ag["goal_distribution"].items():
                merged[g] = merged.get(g, 0.0) + f
        total = sum(merged.values()) or 1.0
        return {g: v / total for g, v in merged.items()}

    buf.write("## Aggregate goal distribution shift\n\n")
    ag_a = agg_goal_dist(a)
    ag_b = agg_goal_dist(b)
    all_goals = sorted(set(ag_a.keys()) | set(ag_b.keys()))
    buf.write(f"| goal | {A} | {B} | delta |\n|---|---:|---:|---:|\n")
    for g in all_goals:
        va = ag_a.get(g, 0.0)
        vb = ag_b.get(g, 0.0)
        buf.write(f"| {g} | {va*100:.1f}% | {vb*100:.1f}% | {(vb-va)*100:+.1f}% |\n")
    buf.write("\n")

    # Distribution of metrics per agent (5th/50th/95th percentile)
    import statistics
    def pct(xs, p):
        if not xs:
            return float("nan")
        xs2 = sorted(xs)
        i = max(0, min(len(xs2) - 1, int(round(p * (len(xs2) - 1)))))
        return xs2[i]
    for metric in ("velocity_autocorr", "direction_persistence",
                   "dominant_goal_frac", "goal_switches",
                   "path_length", "food_eaten_final"):
        xs_a = [ag[metric] for ag in a["agents"]]
        xs_b = [ag[metric] for ag in b["agents"]]
        buf.write(f"### Distribution: {metric}\n")
        buf.write(f"| pct | {A} | {B} |\n|---|---:|---:|\n")
        for p, name in ((0.05, "5%"), (0.25, "25%"), (0.5, "50%"),
                        (0.75, "75%"), (0.95, "95%")):
            buf.write(f"| {name} | {pct(xs_a, p):.3f} | {pct(xs_b, p):.3f} |\n")
        buf.write(f"| mean | "
                  f"{statistics.mean(xs_a):.3f} | "
                  f"{statistics.mean(xs_b):.3f} |\n\n")

    out_text = buf.getvalue()
    if args.out:
        args.out.write_text(out_text)
        print(f"wrote {args.out} ({len(out_text)} bytes)", file=sys.stderr)
    else:
        print(out_text)
    return 0


if __name__ == "__main__":
    sys.exit(main())
