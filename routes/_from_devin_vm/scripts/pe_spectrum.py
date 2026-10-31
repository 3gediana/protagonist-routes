import csv, glob, os

SEEDS = ["50021","50022","50023","50024"]
DIR = "/home/ubuntu/pe_csv"

def load(path):
    with open(path) as f:
        rows = list(csv.DictReader(f))
    return rows

def fnum(x):
    try: return float(x)
    except: return 0.0

print("="*78)
print("PE 涌现谱 P0  (4 seeds, this run gen range)")
print("="*78)

# ---- behavior mechanisms: attempts vs successes at LAST gen, averaged over seeds
beh_last = []
for s in SEEDS:
    rows = load(f"{DIR}/{s}_protagonist_behaviors.csv")
    if rows: beh_last.append((s, rows[0], rows[-1]))

g0 = beh_last[0][1]["generation"]; g1 = beh_last[0][2]["generation"]
print(f"\n[gen {g0} -> {g1}]  per-seed last-gen, then mean")

mechs = [
 ("chop","chop_attempts","chop_successes"),
 ("craft","craft_attempts","craft_successes"),
 ("throw","throw_attempts","throw_hits"),
 ("hunt","hunt_attempts","hunt_successes"),
]
print(f"\n{'mechanism':<10}{'attempts(mean)':>16}{'success(mean)':>15}{'succ_rate':>11}")
for name,ak,sk in mechs:
    a = sum(fnum(l[ak]) for _,_,l in beh_last)/len(beh_last)
    sc = sum(fnum(l[sk]) for _,_,l in beh_last)/len(beh_last)
    rate = (sc/a*100) if a>0 else 0
    print(f"{name:<10}{a:>16.0f}{sc:>15.1f}{rate:>10.3f}%")

# single-count behaviors (events)
print(f"\n{'event':<26}{'mean(last gen)':>16}")
for k in ["deposits_total","deposits_stick","deposits_stone","completed_worksites",
          "worksite_completion_events","active_fires","drank_water_events",
          "signal_emits","signal_response_events","cooperative_co_presence_seconds",
          "survived_protagonists","total_protagonists"]:
    v = sum(fnum(l[k]) for _,_,l in beh_last)/len(beh_last)
    print(f"{k:<26}{v:>16.1f}")

# ---- MAP-Elites coverage trajectory (already built into fitness.csv!)
print("\n" + "="*78)
print("MAP-Elites 内置指标 (fitness.csv): coverage = 行为格被填的比例")
print("="*78)
for s in SEEDS:
    rows = load(f"{DIR}/{s}_fitness.csv")
    if not rows: continue
    f0, f1 = rows[0], rows[-1]
    print(f"seed {s}: gen {f0['generation']}->{f1['generation']}  "
          f"me_coverage {f0.get('me_coverage','?')}->{f1.get('me_coverage','?')}  "
          f"qd_score {fnum(f0.get('me_qd_score',0)):.0f}->{fnum(f1.get('me_qd_score',0)):.0f}")

# ---- niche occupancy (evolution.csv best per niche, last gen, mean over seeds)
print("\n" + "="*78)
print("生态位 best fitness (evolution.csv, last gen, mean over seeds)")
print("="*78)
niches = ["large_herbivore","small_forager","apex_predator","pack_hunter",
          "ambush_predator","scavenger","omnivore","rabbit"]
evo_last = []
for s in SEEDS:
    rows = load(f"{DIR}/{s}_evolution.csv")
    if rows: evo_last.append((rows[0], rows[-1]))
print(f"{'niche':<18}{'best_avg(g0)':>14}{'best_avg(g1)':>14}")
for n in niches:
    b0 = sum(fnum(a[f'{n}_best']) for a,_ in evo_last)/len(evo_last)
    b1 = sum(fnum(b[f'{n}_best']) for _,b in evo_last)/len(evo_last)
    print(f"{n:<18}{b0:>14.1f}{b1:>14.1f}")
