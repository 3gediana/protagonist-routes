#!/usr/bin/env python3
"""Early-verdict gate: predict a 10M-run's final quality from its first ~10%
of episodes, so overnight batches can be cut short instead of burning the
full budget.

Empirical basis (2026-09-30, 46 historical >=800-episode runs in metrics/):
mean score over the first 10% of episodes correlates with the mean score of
the last 20% at Pearson r=+0.91 / Spearman rho=+0.79; rank-based keep-top-25%
preserved 8/8 of the final top-8 arms. Verdicts:

  PROMOTE  early mean score in the top half of the batch  (always let finish)
  BORDER   within one batch-MAD of the median             (let finish, cheap)
  KILL_CANDIDATE  bottom 25% of the batch                 (candidate to stop)

Absolute thresholds would not transfer across reward-recipe changes; ranking
against the same night's sibling arms does. Usage:
  python scripts/early_verdict.py --arms d4_base_s7,d4_ss_s7,...
  python scripts/early_verdict.py --manifest night_manifest_d4.txt
"""
import argparse
import glob
import json
import os
import statistics as st


def load_scores(path, frac=0.10, min_eps=80):
    try:
        with open(path) as f:
            rows = [json.loads(l) for l in f]
    except (OSError, json.JSONDecodeError):
        return None, 0
    if len(rows) < min_eps:
        return None, len(rows)
    cut = max(min_eps, int(len(rows) * frac))
    scores = [float(r["score"]) for r in rows[:cut] if r.get("score") is not None]
    if not scores:
        return None, len(rows)
    return st.mean(scores), len(rows)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--metrics-dir", default="metrics")
    ap.add_argument("--arms", default="", help="comma-separated arm names")
    ap.add_argument("--manifest", default="", help="read arm names from manifest")
    ap.add_argument("--frac", type=float, default=0.10)
    ap.add_argument("--min-eps", type=int, default=80)
    args = ap.parse_args()

    arms = []
    if args.arms:
        arms = [a.strip() for a in args.arms.split(",") if a.strip()]
    elif args.manifest:
        with open(args.manifest) as f:
            for line in f:
                line = line.strip()
                if not line or line.startswith("#"):
                    continue
                arms.append(line.split("|")[0].strip())
    if not arms:
        print("no arms given")
        return 1

    entries = []
    for arm in arms:
        path = os.path.join(args.metrics_dir, arm + ".jsonl")
        score, eps = load_scores(path, args.frac, args.min_eps)
        if score is None:
            print(f"{arm:24s}  (insufficient data: {eps} eps)")
        else:
            entries.append((arm, score, eps))
    if len(entries) < 2:
        print("need >=2 scored arms to rank; nothing to compare")
        return 1

    entries.sort(key=lambda e: -e[1])
    scores = [e[1] for e in entries]
    med = st.median(scores)
    mad = st.median([abs(s - med) for s in scores]) or 1e-9
    print(f"early window = first {args.frac:.0%} of episodes per arm "
          f"(min {args.min_eps} eps), ranked by mean score\n")
    print(f"{'arm':24s} {'early':>7} {'eps':>6}  verdict")
    for arm, score, eps in entries:
        if score >= med:
            verdict = "PROMOTE"
        elif score >= med - mad:
            verdict = "BORDER"
        else:
            verdict = "KILL_CANDIDATE"
        print(f"{arm:24s} {score:7.3f} {eps:6d}  {verdict}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
