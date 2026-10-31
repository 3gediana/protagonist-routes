"""protagonist_ecology per-tick trajectory NARRATOR.

Reads agent_trace.csv for one episode and prints a tick-by-tick story of
ONE agent (picked from a chosen species), answering the literal question
"what was this creature doing at every point in time?".

Usage:
  python protagonist_trajectory_narrate.py <trace_dir> --species pack_hunter [--stride 10] [--agent <id>] [--out file.md]

Picks (by default) the most "interesting" agent of the species: the one
that is alive longest and has the most episodic events, so the timeline is
rich rather than an early-death stub.
"""
from __future__ import annotations
import argparse, math, sys
from collections import Counter
from pathlib import Path
import numpy as np
import pandas as pd

SPECIES = {0:"protagonist",1:"small_forager",2:"large_herbivore",3:"rabbit",
           4:"scavenger",5:"omnivore",6:"pack_hunter",7:"apex_predator",8:"ambush_predator"}
NAME2ID = {v:k for k,v in SPECIES.items()}

def heading(vx, vy):
    if abs(vx) < 1e-6 and abs(vy) < 1e-6: return "still"
    ang = math.degrees(math.atan2(vy, vx))
    dirs = ["E","NE","N","NW","W","SW","S","SE"]
    idx = int(((ang + 22.5) % 360) / 45)
    return dirs[idx]

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("trace_dir")
    ap.add_argument("--species", default=None)
    ap.add_argument("--agent", type=int, default=None)
    ap.add_argument("--stride", type=int, default=10)
    ap.add_argument("--out", default=None)
    args = ap.parse_args()

    df = pd.read_csv(Path(args.trace_dir) / "agent_trace.csv")
    df = df.sort_values(["agent_id","step_index"]).reset_index(drop=True)

    out = []
    def emit(s=""):
        out.append(s)

    # choose agent
    if args.agent is not None:
        target = args.agent
    else:
        sid = NAME2ID[args.species]
        sub = df[df["species_id"] == sid]
        if sub.empty:
            print(f"no agents of species {args.species} (id {sid}) in trace", file=sys.stderr)
            print("species present:", sorted(df["species_id"].unique()), file=sys.stderr)
            sys.exit(2)
        # rank by lifetime samples then episodic events
        rank = sub.groupby("agent_id").agg(
            n=("step_index","count"),
            ev=("episodic_event_count","max"),
            fit=("fitness","max"),
            alive=("alive","last"),
        ).reset_index().sort_values(["n","ev","fit"], ascending=False)
        target = int(rank.iloc[0]["agent_id"])

    a = df[df["agent_id"] == target].reset_index(drop=True)
    sid = int(a["species_id"].iloc[0])
    sname = SPECIES.get(sid, f"sp{sid}")
    n = len(a)
    t0, t1 = float(a["time_seconds"].iloc[0]), float(a["time_seconds"].iloc[-1])

    emit(f"# Tick-by-tick narrative: agent {target} ({sname}), genome {int(a['genome_id'].iloc[0])}")
    emit(f"trace: {args.trace_dir}")
    emit(f"samples={n}  t={t0:.1f}s..{t1:.1f}s  final_alive={bool(a['alive'].iloc[-1])}  final_fitness={float(a['fitness'].iloc[-1]):.1f}")
    xs=a["x"].to_numpy(float); ys=a["y"].to_numpy(float)
    path = float(np.hypot(np.diff(xs),np.diff(ys)).sum()) if n>=2 else 0.0
    emit(f"total path length={path:.0f}u  bbox=({xs.max()-xs.min():.0f} x {ys.max()-ys.min():.0f})u  start=({xs[0]:.0f},{ys[0]:.0f}) end=({xs[-1]:.0f},{ys[-1]:.0f})")
    gd = Counter(a["goal_name"].astype(str))
    emit("goal time-share: " + ", ".join(f"{g}={c/n*100:.0f}%" for g,c in gd.most_common()))
    emit(f"lifetime food_eaten={float(a['food_eaten'].iloc[-1]):.2f}  kills={int(a['kills'].iloc[-1])}  damage_dealt={float(a['damage_dealt'].iloc[-1]):.1f}  episodic_events={int(a['episodic_event_count'].iloc[-1])}")
    emit("")

    # ---- phase segmentation: consecutive runs of same goal_name ----
    goals = a["goal_name"].astype(str).to_numpy()
    emit("## Behavioral phases (consecutive same-goal segments)")
    emit("| phase | t_start..t_end (s) | dur | goal | move | displacement | health | hydration | events |")
    emit("|---|---|---|---|---|---|---|---|---|")
    i = 0; ph = 0
    while i < n:
        j = i
        while j+1 < n and goals[j+1] == goals[i]:
            j += 1
        seg = a.iloc[i:j+1]
        ts, te = float(seg["time_seconds"].iloc[0]), float(seg["time_seconds"].iloc[-1])
        sx0,sy0 = float(seg["x"].iloc[0]),float(seg["y"].iloc[0])
        sx1,sy1 = float(seg["x"].iloc[-1]),float(seg["y"].iloc[-1])
        disp = math.hypot(sx1-sx0, sy1-sy0)
        spd = float(np.hypot(seg["vx"].to_numpy(float), seg["vy"].to_numpy(float)).mean())
        hd = heading(float(seg["vx"].mean()), float(seg["vy"].mean()))
        hp0,hp1 = float(seg["health_ratio"].iloc[0]),float(seg["health_ratio"].iloc[-1])
        hy0,hy1 = float(seg["hydration_ratio"].iloc[0]),float(seg["hydration_ratio"].iloc[-1])
        evd = int(seg["episodic_event_count"].iloc[-1]) - int(seg["episodic_event_count"].iloc[0])
        evt = Counter(seg["episodic_top_type"].astype(str)).most_common(1)[0][0]
        ph += 1
        emit(f"| {ph} | {ts:.0f}..{te:.0f} | {te-ts:.0f}s | {goals[i]} | {hd}@{spd:.1f}u/s | {disp:.0f}u ({sx0:.0f},{sy0:.0f})->({sx1:.0f},{sy1:.0f}) | {hp0:.2f}->{hp1:.2f} | {hy0:.2f}->{hy1:.2f} | +{evd} {evt if evd>0 else ''} |")
        i = j+1
    emit("")

    # ---- strided per-tick timeline ----
    emit(f"## Per-tick timeline (every {args.stride} samples)")
    emit("| t(s) | pos | head@spd | goal | nearest_food | pred_near | food_eaten | kills | coop | event |")
    emit("|---|---|---|---|---|---|---|---|---|---|")
    prev_food = 0.0; prev_kills = 0; prev_ev = 0
    for k in range(0, n, args.stride):
        r = a.iloc[k]
        vx,vy = float(r["vx"]),float(r["vy"])
        fe = float(r["food_eaten"]); kl = int(r["kills"]); ev = int(r["episodic_event_count"])
        nf = float(r["nearest_food_dist"]); pn = int(r["predator_nearby_count"])
        coop = float(r["social_best_coop"])
        fed = "+%.2f"%(fe-prev_food) if fe>prev_food else ""
        kld = "+%d"%(kl-prev_kills) if kl>prev_kills else ""
        evd = r["episodic_top_type"] if ev>prev_ev else ""
        nfs = "%.0f"%nf if nf>=0 else "-"
        emit(f"| {float(r['time_seconds']):.0f} | ({float(r['x']):.0f},{float(r['y']):.0f}) | {heading(vx,vy)}@{math.hypot(vx,vy):.1f} | {r['goal_name']} | {nfs} | {pn} | {fed} | {kld} | {coop:.2f} | {evd} |")
        prev_food=fe; prev_kills=kl; prev_ev=ev

    text = "\n".join(out)
    if args.out:
        Path(args.out).write_text(text, encoding="utf-8")
        print(f"wrote {args.out} ({len(text)} chars)")
    else:
        print(text)

if __name__ == "__main__":
    main()
