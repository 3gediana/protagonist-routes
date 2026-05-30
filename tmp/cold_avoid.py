import csv, sys, math
from collections import defaultdict

# Compare gen0 vs gen79: under cold stress, do protagonists seek fire/worksite more?
# Usage: cold_avoid.py <rundir>
rd = sys.argv[1]
COLD = 30.0  # cold_temp_threshold used in this run

def load(gen):
    path = f"{rd}/trace/generation_{gen}/episode_0/agent_trace.csv"
    rows = []
    with open(path, newline='') as f:
        r = csv.DictReader(f)
        for row in rows_iter(r):
            rows.append(row)
    return rows

def rows_iter(r):
    for row in r:
        yield row

def fnum(s):
    try: return float(s)
    except: return None

def analyze(gen):
    path = f"{rd}/trace/generation_{gen}/episode_0/agent_trace.csv"
    # protagonists = agents that have a goal_name (brained). background species have goal_name 'na'/empty
    cold = defaultdict(list); warm = defaultdict(list)
    n_cold=0; n_warm=0
    with open(path, newline='') as f:
        for row in csv.DictReader(f):
            gn = row.get('goal_name','')
            if gn in ('','na'):  # skip background / non-brained
                continue
            if row.get('alive','1') in ('0','false','False'):
                continue
            ct = fnum(row.get('core_temperature'))
            if ct is None: continue
            fire = fnum(row.get('spatial_fire_cells')) or 0.0
            wd   = fnum(row.get('worksite_distance'))
            speed = math.hypot(fnum(row.get('vx')) or 0.0, fnum(row.get('vy')) or 0.0)
            bucket = cold if ct <= COLD else warm
            if ct <= COLD: n_cold+=1
            else: n_warm+=1
            bucket['fire'].append(fire)
            if wd is not None and wd >= 0: bucket['wd'].append(wd)
            bucket['speed'].append(speed)
            bucket['health'].append(fnum(row.get('health_ratio')) or 0.0)
    def mean(xs): return sum(xs)/len(xs) if xs else float('nan')
    return {
        'n_cold': n_cold, 'n_warm': n_warm,
        'cold_frac': n_cold/(n_cold+n_warm) if (n_cold+n_warm) else 0,
        'fire_cold': mean(cold['fire']), 'fire_warm': mean(warm['fire']),
        'wd_cold': mean(cold['wd']), 'wd_warm': mean(warm['wd']),
        'speed_cold': mean(cold['speed']), 'speed_warm': mean(warm['speed']),
        'health_cold': mean(cold['health']), 'health_warm': mean(warm['health']),
    }

print(f"# Cold-avoidance analysis (cold_thr={COLD}), brained protagonists only")
for gen in (0, 79):
    try:
        a = analyze(gen)
    except FileNotFoundError:
        print(f"gen{gen}: no trace"); continue
    print(f"\n## gen {gen}")
    print(f"  cold ticks={a['n_cold']} warm ticks={a['n_warm']} cold_frac={a['cold_frac']:.2f}")
    print(f"  fire_cells   cold={a['fire_cold']:.3f}  warm={a['fire_warm']:.3f}  (delta cold-warm={a['fire_cold']-a['fire_warm']:+.3f})")
    print(f"  worksite_dist cold={a['wd_cold']:.2f}  warm={a['wd_warm']:.2f}  (delta={a['wd_cold']-a['wd_warm']:+.2f})")
    print(f"  speed        cold={a['speed_cold']:.3f}  warm={a['speed_warm']:.3f}")
    print(f"  health       cold={a['health_cold']:.3f}  warm={a['health_warm']:.3f}")
