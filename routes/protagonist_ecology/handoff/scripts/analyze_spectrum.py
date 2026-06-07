#!/usr/bin/env python3
"""
Read PE behavior/evolution/fitness CSVs and print the emergence spectrum:
mechanism attempts-vs-successes, single-event counts, MAP-Elites coverage, niche best-fitness.

Point it at a directory containing per-seed CSVs named "<seed>_protagonist_behaviors.csv",
"<seed>_evolution.csv", "<seed>_fitness.csv". (Copy them off the box runs dir and rename,
or pass --raw with a single run dir's plain CSV names.)

Usage:
    python analyze_spectrum.py /path/to/csv_dir --seeds 50021 50022 50023 50024
"""
import csv, sys, argparse, os

def load(path):
    if not os.path.exists(path): return []
    with open(path) as f: return list(csv.DictReader(f))
def fnum(x):
    try: return float(x)
    except: return 0.0

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("dir")
    ap.add_argument("--seeds", nargs="+", default=["50021","50022","50023","50024"])
    a = ap.parse_args()
    D, SEEDS = a.dir, a.seeds

    beh = [(s, load(f"{D}/{s}_protagonist_behaviors.csv")) for s in SEEDS]
    beh = [(s, r[0], r[-1]) for s,r in beh if r]
    if not beh: print(f"no behavior CSVs in {D}"); return
    g0, g1 = beh[0][1]["generation"], beh[0][2]["generation"]
    print("="*78); print(f"PE emergence spectrum  (gen {g0} -> {g1}, mean over {len(beh)} seeds)"); print("="*78)

    print(f"\n{'mechanism':<10}{'attempts':>14}{'success':>12}{'succ_rate':>11}")
    for name,ak,sk in [("chop","chop_attempts","chop_successes"),("craft","craft_attempts","craft_successes"),
                       ("throw","throw_attempts","throw_hits"),("hunt","hunt_attempts","hunt_successes")]:
        att = sum(fnum(l[ak]) for _,_,l in beh)/len(beh)
        suc = sum(fnum(l[sk]) for _,_,l in beh)/len(beh)
        print(f"{name:<10}{att:>14.0f}{suc:>12.1f}{(suc/att*100 if att else 0):>10.3f}%")

    print(f"\n{'event':<28}{'mean(last gen)':>16}")
    for k in ["active_fires","drank_water_events","completed_worksites","signal_emits",
              "signal_response_events","cooperative_co_presence_seconds","survived_protagonists","total_protagonists"]:
        if k in beh[0][2]:
            print(f"{k:<28}{sum(fnum(l[k]) for _,_,l in beh)/len(beh):>16.1f}")

    print("\n" + "-"*78 + "\nMAP-Elites coverage (fitness.csv)")
    for s in SEEDS:
        r = load(f"{D}/{s}_fitness.csv")
        if not r: continue
        print(f"  seed {s}: gen {r[0]['generation']}->{r[-1]['generation']}  "
              f"coverage {r[0].get('me_coverage','?')}->{r[-1].get('me_coverage','?')}  "
              f"qd {fnum(r[0].get('me_qd_score',0)):.0f}->{fnum(r[-1].get('me_qd_score',0)):.0f}")

    print("\n" + "-"*78 + "\nniche best fitness (evolution.csv, mean over seeds)")
    evo = [(load(f"{D}/{s}_evolution.csv")) for s in SEEDS]; evo = [(r[0],r[-1]) for r in evo if r]
    if evo:
        print(f"{'niche':<18}{'g0':>12}{'g1':>12}")
        for n in ["large_herbivore","small_forager","apex_predator","pack_hunter","ambush_predator","scavenger","omnivore"]:
            k=f"{n}_best"
            if k in evo[0][0]:
                print(f"{n:<18}{sum(fnum(x[k]) for x,_ in evo)/len(evo):>12.1f}{sum(fnum(y[k]) for _,y in evo)/len(evo):>12.1f}")

if __name__ == "__main__":
    if len(sys.argv) < 2: print(__doc__); sys.exit(1)
    main()
