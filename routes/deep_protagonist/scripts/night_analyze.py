#!/usr/bin/env python3
# /// script
# requires-python = ">=3.10"
# dependencies = []
# ///
"""night_analyze.py — 早上一口气验收：遍历一轮夜跑的 metrics/*.jsonl，
出一张同口径对比表 + YES/PARTIAL/NO 判定（survive-first mask 主题）。

只用标准库（无依赖）出文字表；--png 时若装了 matplotlib 再额外画对比柱状图。

判定口径（对照交接文档 第五节）：
  - 吃喝占比 eat+drink%   目标：处理臂明显高于对照、向 20-40% 回升
  - 动作熵 ent%（归一化）  目标：从 ~96% 往 40-50% 掉
  - 饿死/局 deaths_food    目标：从 ~15 往 2-5 掉
  - 里程碑 unlocks         看早期是否更稳；后期异常刷高=gaming 嫌疑
  - score / reward         处理臂应 >= 对照；远低于=mask 太狠

用法：
  python3 scripts/night_analyze.py --metrics-dir metrics --arms ab_ctrl,ab_treat035,ab_treat025,ab_treat050
  python3 scripts/night_analyze.py --metrics-dir metrics --manifest night_manifest.txt --control ab_ctrl
  # 只看后段策略（去掉前期失败的统计拖累）：加 --last-n 100
  # 画对比图（需 matplotlib）：加 --png night_cmp.png
"""
from __future__ import annotations

import argparse
import json
import math
import os
import sys

# 默认离散动作类目数（D-122：16 触发 + NOOP + fire + 5 科技树 = 23）。
# 归一化熵 = H / ln(NUM_CATS)，即"相对均匀随机的百分比"。
NUM_CATS = 23
EAT, DRINK = 0, 1


def load(path: str, last_n: int) -> list[dict]:
    eps = []
    with open(path, "r", encoding="utf-8", errors="replace") as f:
        for line in f:
            line = line.strip()
            if not line.startswith("{"):
                continue
            try:
                eps.append(json.loads(line))
            except json.JSONDecodeError:
                continue
    return eps[-last_n:] if last_n > 0 else eps


def norm_entropy(act: list[int]) -> float:
    """归一化动作熵（% of uniform-random）。
    act 是跨 n_envs 累加的 23 桶直方图（已含 NOOP=idx16、fire=idx17、5 科技树），
    分母用 sum(act) 本身，不再按 steps 补桶。"""
    counts = [max(0, int(x)) for x in act]
    tot = sum(counts)
    if tot <= 0:
        return 0.0
    h = 0.0
    for c in counts:
        if c > 0:
            p = c / tot
            h -= p * math.log(p)
    nbins = max(len(counts), NUM_CATS)
    return 100.0 * h / math.log(nbins)


def summarize(eps: list[dict]) -> dict | None:
    if not eps:
        return None
    n = len(eps)
    tot_steps = sum(e.get("steps", 0) for e in eps)
    # act 跨 n_envs 累加：动作占比的分母是 sum(act)（动作选择总次数），不是 steps。
    act_eat = sum(e.get("act", [0, 0])[EAT] for e in eps if e.get("act"))
    act_drink = sum(e.get("act", [0, 0])[DRINK] for e in eps if len(e.get("act", [])) > 1)
    act_total = sum(sum(e.get("act", [])) for e in eps)
    # 每局熵的均值（按局算再平均，避免长局主导）
    ents = [norm_entropy(e.get("act", [])) for e in eps if e.get("act")]
    # deaths_food 是跨所有 env 、跨整个 run 的**累计**计数器（逐局单调不减）。
    # 不能 sum（那是对累计值再求和=无意义）。用最终值/总局数 = 每局饿死率。
    dfs = [int(e.get("deaths_food", 0)) for e in eps]
    df_final = max(dfs) if dfs else 0
    unlocks = [sum(e.get("unlocks", [])) for e in eps]
    return dict(
        n=n,
        steps=tot_steps / n if n else 0,
        eatdrink=100.0 * (act_eat + act_drink) / act_total if act_total else 0.0,
        ent=sum(ents) / len(ents) if ents else 0.0,
        deaths_food=df_final / n if n else 0.0,  # 每局饿死率（累计总数/局数）
        unlocks=sum(unlocks) / n if n else 0.0,
        score=sum(e.get("score", 0.0) for e in eps) / n if n else 0.0,
        reward=sum(e.get("reward", 0.0) for e in eps) / n if n else 0.0,
    )


def binned_curve(eps: list[dict], nbins: int = 30) -> dict | None:
    """把一条臂的 episodes 按累计训练步数切成 nbins 个窗口，每窗口算同口径指标，
    用于画"指标 vs 训练步数"学习曲线（比终态柱状图更能体现起效过程）。"""
    if not eps:
        return None
    # 累计 env-steps 作为 x 轴；按 episode 顺序切成等量窗口。
    cum, run = [], 0
    for e in eps:
        run += int(e.get("steps", 0))
        cum.append(run)
    # deaths_food 是累计计数器：窗口内饿死率 = 该窗口 df 增量 / 窗口局数。
    dfs = [int(e.get("deaths_food", 0)) for e in eps]
    n = len(eps)
    nbins = max(1, min(nbins, n))
    xs, eatdrink, ent, deaths, score = [], [], [], [], []
    for b in range(nbins):
        lo = (b * n) // nbins
        hi = ((b + 1) * n) // nbins
        if hi <= lo:
            continue
        win = eps[lo:hi]
        a_eat = sum(e.get("act", [0, 0])[EAT] for e in win if e.get("act"))
        a_drk = sum(e.get("act", [0, 0])[DRINK] for e in win if len(e.get("act", [])) > 1)
        a_tot = sum(sum(e.get("act", [])) for e in win)
        ents = [norm_entropy(e.get("act", [])) for e in win if e.get("act")]
        xs.append(cum[hi - 1])
        eatdrink.append(100.0 * (a_eat + a_drk) / a_tot if a_tot else 0.0)
        ent.append(sum(ents) / len(ents) if ents else 0.0)
        prev_df = dfs[lo - 1] if lo > 0 else 0
        deaths.append(max(0, dfs[hi - 1] - prev_df) / (hi - lo))
        score.append(sum(e.get("score", 0.0) for e in win) / len(win))
    return dict(x=xs, eatdrink=eatdrink, ent=ent, deaths_food=deaths, score=score)


def verdict(ctrl: dict, t: dict) -> str:
    if ctrl is None:
        return "-"
    eat_up = t["eatdrink"] > ctrl["eatdrink"] + 3.0
    ent_dn = t["ent"] < ctrl["ent"] - 5.0
    food_dn = t["deaths_food"] < ctrl["deaths_food"] - 0.2
    score_ok = t["score"] >= ctrl["score"] * 0.8
    hits = sum([eat_up, ent_dn, food_dn])
    if not score_ok:
        return "NO(score-collapse:mask-too-hard)"
    if hits >= 3:
        return "YES(structure>shaping)"
    if hits >= 1:
        return "PARTIAL(tune-thresh)"
    return "NO(not-action-set)"


def arms_from_manifest(path: str) -> list[str]:
    names = []
    with open(path, "r", encoding="utf-8", errors="replace") as f:
        for raw in f:
            line = raw.strip()
            if not line or line.startswith("#") or "|" not in line:
                continue
            names.append(line.split("|", 1)[0].strip())
    return names


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--metrics-dir", default="metrics")
    ap.add_argument("--arms", default="", help="逗号分隔的臂名（= metrics/<名>.jsonl）")
    ap.add_argument("--manifest", default="", help="从 manifest 读臂名（与 --arms 二选一）")
    ap.add_argument("--control", default="", help="对照臂名（默认 = 第一个臂）")
    ap.add_argument("--last-n", type=int, default=0, help="只统计每个文件最后 N 局")
    ap.add_argument("--png", default="", help="额外输出对比柱状图（需 matplotlib）")
    ap.add_argument("--curves", default="", help="额外输出学习曲线（指标 vs 训练步数，需 matplotlib）")
    ap.add_argument("--curve-bins", type=int, default=30, help="学习曲线窗口数")
    ap.add_argument("--groups", default="",
                    help="多种子聚合曲线：'标签=臂1+臂2+臂3;标签2=...'，每组画 mean±std 带状曲线")
    ap.add_argument("--groups-out", default="groups_ci.png", help="--groups 的输出 PNG")
    args = ap.parse_args()

    if args.arms:
        names = [a.strip() for a in args.arms.split(",") if a.strip()]
    elif args.manifest:
        names = arms_from_manifest(args.manifest)
    else:
        print("需要 --arms 或 --manifest", file=sys.stderr)
        return 1

    rows = []
    raw_eps: dict[str, list[dict]] = {}
    for name in names:
        path = os.path.join(args.metrics_dir, f"{name}.jsonl")
        if not os.path.exists(path):
            print(f"  (缺) {name}: 无 {path}")
            rows.append((name, None))
            continue
        eps_all = load(path, args.last_n)
        if args.curves:
            raw_eps[name] = load(path, 0)  # 曲线要全程，不受 --last-n 影响
        rows.append((name, summarize(eps_all)))

    control_name = args.control or names[0]
    ctrl = dict(rows).get(control_name)

    suffix = f"  (last {args.last_n} ep/arm)" if args.last_n else ""
    print(f"\n=== night verdict  control={control_name}{suffix} ===")
    print("(eat+drink% 越高越好 / ent% 越低越好 / food_deaths/ep 越低越好)")
    hdr = f"{'arm':<22}{'eps':>5}{'eat+drink%':>11}{'ent%':>7}{'food_d/ep':>10}{'mlstn':>7}{'score':>8}{'reward':>9}  verdict"
    print(hdr)
    print("-" * len(hdr))
    for name, s in rows:
        if s is None:
            print(f"{name:<22}{'-':>5}")
            continue
        v = "CONTROL" if name == control_name else verdict(ctrl, s)
        print(f"{name:<22}{s['n']:>5}{s['eatdrink']:>11.1f}{s['ent']:>7.1f}"
              f"{s['deaths_food']:>9.2f}{s['unlocks']:>7.2f}{s['score']:>8.3f}"
              f"{s['reward']:>9.2f}  {v}")

    if args.png:
        render_png(rows, control_name, args.png)
    if args.curves:
        render_curves(raw_eps, control_name, args.curves, args.curve_bins)
    if args.groups:
        render_groups(args.groups, args.metrics_dir, args.groups_out, args.curve_bins)
    return 0


def parse_groups(spec: str) -> list[tuple[str, list[str]]]:
    """'标签=臂1+臂2;标签2=臂3+臂4' -> [(标签,[臂1,臂2]),...]"""
    groups = []
    for chunk in spec.split(";"):
        chunk = chunk.strip()
        if not chunk or "=" not in chunk:
            continue
        label, arms = chunk.split("=", 1)
        members = [a.strip() for a in arms.split("+") if a.strip()]
        if members:
            groups.append((label.strip(), members))
    return groups


def aggregate_group(arms_eps: list[list[dict]], nbins: int):
    """把同一条件的多个种子臂按 bin 序号对齐，逐 bin 求 mean/std（pstdev）。
    bin 是等局数窗口，同 bin 序号≈同训练进度，故按序号对齐即可、无需插值。"""
    import statistics
    curves = [binned_curve(e, nbins) for e in arms_eps if e]
    curves = [c for c in curves if c]
    if not curves:
        return None
    L = min(len(c["x"]) for c in curves)
    keys = ["eatdrink", "ent", "deaths_food", "score"]
    out = {"x": [], "nseed": len(curves)}
    for k in keys:
        out[k + "_mean"], out[k + "_std"] = [], []
    for b in range(L):
        out["x"].append(statistics.mean(c["x"][b] for c in curves))
        for k in keys:
            vals = [c[k][b] for c in curves]
            out[k + "_mean"].append(statistics.mean(vals))
            out[k + "_std"].append(statistics.pstdev(vals) if len(vals) > 1 else 0.0)
    return out


def render_groups(spec: str, metrics_dir: str, out: str, nbins: int):
    try:
        import matplotlib
        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
    except ImportError:
        print(f"(跳过分组曲线：未装 matplotlib) {out}")
        return
    groups = parse_groups(spec)
    agg = []
    for label, members in groups:
        eps_list = []
        for m in members:
            p = os.path.join(metrics_dir, f"{m}.jsonl")
            if os.path.exists(p):
                eps_list.append(load(p, 0))
        a = aggregate_group(eps_list, nbins)
        if a is not None:
            agg.append((label, a))
    if not agg:
        print("(分组曲线：无有效数据)")
        return
    panels = [("eat+drink%", "eatdrink"), ("ent%", "ent"),
              ("deaths_food/ep", "deaths_food"), ("score", "score")]
    fig, axes = plt.subplots(1, len(panels), figsize=(4.5 * len(panels), 4))
    for ax, (title, key) in zip(axes, panels):
        for label, a in agg:
            xs = [x / 1e6 for x in a["x"]]
            mean = a[key + "_mean"]
            std = a[key + "_std"]
            lo = [m - s for m, s in zip(mean, std)]
            hi = [m + s for m, s in zip(mean, std)]
            line, = ax.plot(xs, mean, label=f"{label} (n={a['nseed']})", linewidth=2.0)
            ax.fill_between(xs, lo, hi, color=line.get_color(), alpha=0.18)
        ax.set_title(title)
        ax.set_xlabel("env-steps (M)")
        ax.grid(alpha=0.3)
    axes[-1].legend(fontsize=7, loc="best")
    fig.tight_layout()
    fig.savefig(out, dpi=120)
    print(f"多种子带状曲线已存: {out}")


def render_curves(raw_eps, control_name, out, nbins):
    try:
        import matplotlib
        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
    except ImportError:
        print(f"(跳过曲线：未装 matplotlib) {out}")
        return
    curves = [(n, binned_curve(e, nbins)) for n, e in raw_eps.items()]
    curves = [(n, c) for n, c in curves if c is not None]
    if not curves:
        return
    panels = [("eat+drink%", "eatdrink"), ("ent%", "ent"),
              ("deaths_food/ep", "deaths_food"), ("score", "score")]
    fig, axes = plt.subplots(1, len(panels), figsize=(4.5 * len(panels), 4))
    for ax, (title, key) in zip(axes, panels):
        for n, c in curves:
            xs = [x / 1e6 for x in c["x"]]  # 百万步
            lw = 2.4 if n == control_name else 1.6
            ls = "--" if n == control_name else "-"
            ax.plot(xs, c[key], label=n, linewidth=lw, linestyle=ls)
        ax.set_title(title)
        ax.set_xlabel("env-steps (M)")
        ax.grid(alpha=0.3)
    axes[-1].legend(fontsize=7, loc="best")
    fig.tight_layout()
    fig.savefig(out, dpi=120)
    print(f"学习曲线已存: {out}")


def render_png(rows, control_name, out):
    try:
        import matplotlib
        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
    except ImportError:
        print(f"(跳过 PNG：未装 matplotlib) {out}")
        return
    data = [(n, s) for n, s in rows if s is not None]
    if not data:
        return
    names = [n for n, _ in data]
    metrics = [("eat+drink%", "eatdrink"), ("ent%", "ent"),
               ("deaths_food/ep", "deaths_food"), ("score", "score")]
    fig, axes = plt.subplots(1, len(metrics), figsize=(4 * len(metrics), 4))
    for ax, (title, key) in zip(axes, metrics):
        vals = [s[key] for _, s in data]
        colors = ["tab:gray" if n == control_name else "tab:blue" for n in names]
        ax.bar(range(len(names)), vals, color=colors)
        ax.set_xticks(range(len(names)))
        ax.set_xticklabels(names, rotation=90, fontsize=7)
        ax.set_title(title)
    fig.tight_layout()
    fig.savefig(out, dpi=120)
    print(f"对比图已存: {out}")


if __name__ == "__main__":
    sys.exit(main())
