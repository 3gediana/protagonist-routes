#!/usr/bin/env python3
"""deep_protagonist training dashboard (matplotlib).

Reads one or more runs/*.jsonl produced by deep_protagonist_train and renders:
  fig1  lineage comparison  : reward / score / r_bites / steps / unlocks / milestone
  fig2  latest-run deep-dive: reward-component stack / deaths-by-cause / action hist / nutrition

Standalone: reads only the metrics jsonl (does not touch training code).

Usage:
  python3 dp_dashboard.py v1.jsonl v2.jsonl ... --labels v1,v2,... --out-prefix dp
"""
from __future__ import annotations
import argparse, json
from pathlib import Path
import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

WANDER_REWARD = -37.38  # docs/BASELINE.md wander ref

# Authoritative labels (must stay in sync with the C++ source):
#   ACTION_NAMES  -> main_train.cpp  disc_action_label() / ActionStats (N=23)
#   MILESTONE_NAMES -> include/env/Environment.hpp  EpisodeMilestones::name() (N=10)
ACTION_NAMES = [
    "eat", "drink", "HUNT", "collect", "shelter", "spear", "sleep",
    "grassdress", "furcloak", "wear", "blueprint", "cyclebld", "deposit",
    "plant", "water", "feedtame", "NOOP", "tendfire", "COOK", "MINE",
    "axe", "pickaxe", "monument",
]
N_ACT = len(ACTION_NAMES)            # 23
MILESTONE_NAMES = [
    "drink", "eat", "collect", "spear", "shelter",
    "clothing", "seed", "house", "crop", "follower",
]
N_MS = len(MILESTONE_NAMES)          # 10
# act[16]=NOOP is never tracked (always 0); exclude it from "dead action" tally.
UNTRACKED_ACT = {16}
# D-122 gated tech-tree action ids (the causal tech-chain the panel highlights):
# COOK, MINE, axe, pickaxe, monument. (HUNT(2) is core hunting, not the tech tree.)
TECH_ACT_IDS = [18, 19, 20, 21, 22]
DEATH_CAUSES = [
    ("deaths_cold", "cold", "tab:blue"),
    ("deaths_food", "food", "tab:orange"),
    ("deaths_protein", "protein", "tab:red"),
    ("deaths_vitamin", "vitamin", "tab:green"),
]


def load(path: Path) -> list[dict]:
    rows = []
    for ln in path.read_text(encoding="utf-8", errors="ignore").splitlines():
        ln = ln.strip()
        if not ln:
            continue
        try:
            rows.append(json.loads(ln))
        except json.JSONDecodeError:
            pass
    return rows


def smooth(xs, w=20):
    xs = np.asarray(xs, dtype=float)
    if len(xs) == 0:
        return xs
    out = np.empty_like(xs)
    for i in range(len(xs)):
        lo = max(0, i - w + 1)
        out[i] = xs[lo:i + 1].mean()
    return out


def col(rows, key, default=0.0):
    return [float(r.get(key, default)) for r in rows]


def unlock_count(rows):
    out = []
    for r in rows:
        u = r.get("unlocks", [])
        out.append(sum(1 for x in u if x))
    return out


def action_totals(rows):
    """Summed action counts over all episodes -> np.array(N_ACT)."""
    acts = np.zeros(N_ACT)
    for r in rows:
        a = r.get("act", [])
        for i, v in enumerate(a[:N_ACT]):
            acts[i] += float(v)
    return acts


def action_mean_per_ep(rows):
    return action_totals(rows) / max(1, len(rows))


def milestone_rate(rows):
    """Fraction of episodes in which each milestone fired -> np.array(N_MS)."""
    fired = np.zeros(N_MS)
    for r in rows:
        u = r.get("unlocks", [])
        for i, x in enumerate(u[:N_MS]):
            if x:
                fired[i] += 1
    return fired / max(1, len(rows))


def death_cause_totals(rows):
    return np.array([sum(int(r.get(k, 0)) for r in rows) for k, _, _ in DEATH_CAUSES], dtype=float)


def survival_rate(rows):
    """Fraction of episodes that ended with zero deaths."""
    if not rows:
        return 0.0
    return sum(1 for r in rows if int(r.get("deaths", 0)) == 0) / len(rows)


def dead_action_ids(rows):
    """Tracked action ids that were never used (mechanism the policy abandoned)."""
    tot = action_totals(rows)
    return [i for i in range(N_ACT) if i not in UNTRACKED_ACT and tot[i] == 0]


def emergence_figure(runs, labels, colors, out_path):
    """fig3: mechanism-emergence panel — what mechanisms actually fire, how many
    milestones unlock, which of the 23 actions are 'dead', and why agents die.
    Pure read-out of the metrics jsonl (no training code touched)."""
    fig, ax = plt.subplots(2, 2, figsize=(17, 10))
    fig.suptitle("deep_protagonist · mechanism-emergence panel  (which mechanisms truly fire)",
                 fontsize=14, fontweight="bold")
    nz = [(r, l, c) for r, l, c in zip(runs, labels, colors) if r]

    # (a) milestone trigger rate (10 milestones, grouped bars per run)
    a0 = ax[0, 0]
    x = np.arange(N_MS)
    bw = 0.8 / max(1, len(nz))
    for j, (r, lab, c) in enumerate(nz):
        a0.bar(x + j * bw, 100.0 * milestone_rate(r), width=bw, color=c, label=lab)
    a0.set_xticks(x + bw * (len(nz) - 1) / 2)
    a0.set_xticklabels([f"{i}:{MILESTONE_NAMES[i]}" for i in range(N_MS)], rotation=45, ha="right", fontsize=8)
    a0.set_ylabel("% of episodes fired"); a0.set_ylim(0, 105)
    a0.set_title("milestone trigger rate (unlocks /10)", fontsize=11)
    a0.grid(alpha=0.25, axis="y"); a0.legend(fontsize=8)

    # (b) action liveness for the latest run: mean ticks/episode, dead=red
    a1 = ax[0, 1]
    rows = nz[-1][0] if nz else []
    lab = nz[-1][1] if nz else ""
    mean = action_mean_per_ep(rows)
    dead = set(dead_action_ids(rows))
    bar_colors = ["#c0c0c0" if i in UNTRACKED_ACT else ("crimson" if i in dead else "tab:green")
                  for i in range(N_ACT)]
    a1.bar(np.arange(N_ACT), np.maximum(mean, 1e-3), color=bar_colors)
    a1.set_yscale("log")
    a1.set_xticks(np.arange(N_ACT))
    a1.set_xticklabels([f"{i}:{ACTION_NAMES[i]}" for i in range(N_ACT)], rotation=90, fontsize=7)
    a1.set_ylabel("mean uses / episode (log)")
    a1.set_title(f"action liveness · {lab}  "
                 f"(dead={len(dead)}/{N_ACT - len(UNTRACKED_ACT)} tracked, red)", fontsize=11)
    a1.grid(alpha=0.25, axis="y")

    # (c) death-cause distribution + survival rate per run
    a2 = ax[1, 0]
    x = np.arange(len(nz))
    bottoms = np.zeros(len(nz))
    cause_mat = np.array([death_cause_totals(r) for r, _, _ in nz])  # (runs, 4)
    frac = cause_mat / np.maximum(cause_mat.sum(axis=1, keepdims=True), 1)
    for k, (_, cname, ccol) in enumerate(DEATH_CAUSES):
        a2.bar(x, frac[:, k], bottom=bottoms, color=ccol, label=cname)
        bottoms += frac[:, k]
    for i, (r, _, _) in enumerate(nz):
        a2.text(i, 1.02, f"surv {100 * survival_rate(r):.0f}%\nn={len(r)}",
                ha="center", va="bottom", fontsize=8)
    a2.set_xticks(x); a2.set_xticklabels([l for _, l, _ in nz])
    a2.set_ylim(0, 1.18); a2.set_ylabel("share of deaths")
    a2.set_title("death cause distribution (why they die)", fontsize=11)
    a2.legend(fontsize=8, ncol=4, loc="lower center"); a2.grid(alpha=0.25, axis="y")

    # (d) collapse scorecard: mechanism width per run
    a3 = ax[1, 1]
    metrics = ["milestones\nsolved /10", "live actions\n/%d" % (N_ACT - len(UNTRACKED_ACT)),
               "tech-tree\nused /%d" % len(TECH_ACT_IDS), "survival %"]
    x = np.arange(len(metrics))
    bw = 0.8 / max(1, len(nz))
    for j, (r, lab, c) in enumerate(nz):
        tot = action_totals(r)
        live = sum(1 for i in range(N_ACT) if i not in UNTRACKED_ACT and tot[i] > 0)
        tech = sum(1 for i in TECH_ACT_IDS if tot[i] > 0)
        ms_solved = milestone_rate(r) > 0.5  # "solved" = fires in majority of eps
        vals = [ms_solved.sum(), live, tech, 100 * survival_rate(r)]
        bars = a3.bar(x + j * bw, vals, width=bw, color=c, label=lab)
        for b, v in zip(bars, vals):
            a3.text(b.get_x() + b.get_width() / 2, v, f"{v:.0f}", ha="center", va="bottom", fontsize=7)
    a3.set_xticks(x + bw * (len(nz) - 1) / 2); a3.set_xticklabels(metrics, fontsize=9)
    a3.set_title("mechanism-width scorecard (collapse view)", fontsize=11)
    a3.grid(alpha=0.25, axis="y"); a3.legend(fontsize=8)

    fig.tight_layout(rect=(0, 0, 1, 0.96))
    fig.savefig(out_path, dpi=130)
    print("wrote", out_path)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("paths", nargs="+", type=Path)
    ap.add_argument("--labels", default="")
    ap.add_argument("--out-prefix", default="dp")
    ap.add_argument("--window", type=int, default=20)
    args = ap.parse_args()

    labels = [s.strip() for s in args.labels.split(",")] if args.labels else [p.stem for p in args.paths]
    runs = [load(p) for p in args.paths]
    w = args.window
    colors = plt.cm.viridis(np.linspace(0.1, 0.85, len(runs)))

    # ---- fig1: lineage comparison ----
    fig1, axes = plt.subplots(2, 3, figsize=(16, 8.5))
    fig1.suptitle("deep_protagonist · nutrition lineage (PPO, BC warm-start)  —  smoothed window=%d" % w,
                  fontsize=14, fontweight="bold")
    panels = [
        ("reward", "reward", "episode reward", True),
        ("score", "score", "score (normalized)", False),
        ("r_bites", "r_bites", "predator damage  (r_bites, higher=safer)", False),
        ("steps", "steps", "episode length (steps survived)", False),
        ("__unlocks__", None, "unlocks achieved (count /10)", False),
        ("r_milestone", "r_milestone", "milestone reward", False),
    ]
    for ax, (name, key, title, show_wander) in zip(axes.flat, panels):
        for r, lab, c in zip(runs, labels, colors):
            if not r:
                continue
            ys = unlock_count(r) if name == "__unlocks__" else col(r, key)
            ax.plot(smooth(ys, w), color=c, label=lab, lw=1.8)
        if show_wander:
            ax.axhline(WANDER_REWARD, color="crimson", ls="--", lw=1, alpha=0.7, label="wander ref")
            ax.axhline(0, color="grey", ls=":", lw=0.8)
        ax.set_title(title, fontsize=11)
        ax.set_xlabel("episode")
        ax.grid(alpha=0.25)
    axes.flat[0].legend(fontsize=9, loc="best")
    fig1.tight_layout(rect=(0, 0, 1, 0.96))
    out1 = f"{args.out_prefix}_lineage.png"
    fig1.savefig(out1, dpi=130)
    print("wrote", out1)

    # ---- fig2: latest-run deep dive ----
    rows = runs[-1]
    lab = labels[-1]
    fig2, ax = plt.subplots(2, 2, figsize=(16, 9))
    fig2.suptitle(f"deep_protagonist · {lab} deep-dive  (n={len(rows)} episodes)",
                  fontsize=14, fontweight="bold")

    # (a) reward component stack (smoothed)
    comp_keys = ["r_alive", "r_food", "r_water", "r_collect", "r_craft",
                 "r_milestone", "r_potential", "r_shelter", "r_kills"]
    neg_keys = ["r_bites", "r_death", "r_vital"]
    a0 = ax[0, 0]
    x = np.arange(len(rows))
    pos_stack = np.vstack([smooth(col(rows, k), w) for k in comp_keys])
    a0.stackplot(x, pos_stack, labels=comp_keys, alpha=0.85)
    for k in neg_keys:
        a0.plot(x, smooth(col(rows, k), w), lw=1.5, ls="--", label=k)
    a0.plot(x, smooth(col(rows, "reward"), w), color="black", lw=2.2, label="reward(total)")
    a0.axhline(0, color="grey", lw=0.8)
    a0.set_title("reward composition (smoothed)", fontsize=11)
    a0.set_xlabel("episode"); a0.grid(alpha=0.25)
    a0.legend(fontsize=7, ncol=2, loc="upper left")

    # (b) deaths by cause cumulative
    a1 = ax[0, 1]
    for k, c in [("deaths_cold", "tab:blue"), ("deaths_food", "tab:orange"),
                 ("deaths_protein", "tab:red"), ("deaths_vitamin", "tab:green")]:
        a1.plot(np.cumsum(col(rows, k)), label=k, lw=1.8, color=c)
    a1.set_title("cumulative deaths by cause", fontsize=11)
    a1.set_xlabel("episode"); a1.grid(alpha=0.25); a1.legend(fontsize=9)

    # (c) action histogram (summed over all episodes, all 23 actions, labeled)
    a2 = ax[1, 0]
    acts = action_totals(rows)
    dead = set(dead_action_ids(rows))
    bar_colors = ["#c0c0c0" if i in UNTRACKED_ACT else ("crimson" if i in dead else "tab:purple")
                  for i in range(N_ACT)]
    a2.bar(np.arange(N_ACT), np.maximum(acts, 0.5), color=bar_colors)
    a2.set_yscale("log")
    a2.set_xticks(np.arange(N_ACT))
    a2.set_xticklabels([f"{i}:{ACTION_NAMES[i]}" for i in range(N_ACT)], rotation=90, fontsize=7)
    a2.set_title("action usage (summed counts, all 23; dead=red, NOOP=grey)", fontsize=11)
    a2.grid(alpha=0.25, axis="y")

    # (d) nutrition levels + score
    a3 = ax[1, 1]
    a3.plot(smooth(col(rows, "protein", 100.0), w), label="protein", color="tab:red")
    a3.plot(smooth(col(rows, "vitamin", 100.0), w), label="vitamin", color="tab:green")
    a3.set_ylabel("nutrition level"); a3.set_xlabel("episode")
    a3b = a3.twinx()
    a3b.plot(smooth(col(rows, "score"), w), label="score", color="black", lw=1.5, ls=":")
    a3b.set_ylabel("score")
    a3.set_title("nutrition reserves + score", fontsize=11)
    a3.grid(alpha=0.25); a3.legend(fontsize=9, loc="lower left")
    fig2.tight_layout(rect=(0, 0, 1, 0.96))
    out2 = f"{args.out_prefix}_{lab}_deepdive.png"
    fig2.savefig(out2, dpi=130)
    print("wrote", out2)

    # ---- fig3: mechanism-emergence panel ----
    emergence_figure(runs, labels, colors, f"{args.out_prefix}_emergence.png")


if __name__ == "__main__":
    main()
