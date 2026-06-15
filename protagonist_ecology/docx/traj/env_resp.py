import csv, math, sys
from collections import defaultdict

# arg1 = trace dir; default = ON lineage L1
BASE = sys.argv[1] if len(sys.argv) > 1 else r"D:/claude-code/c++/data/runs/realism_envperc_L1/20260530-125523/trace"
DAY_LEN = 200.0
SEASON_LEN = 150.0
SEASON = ["spring","summer","autumn","winter"]

NAMED = {'emit_signal','respond_signal','find_food','build_worksite','explore','gather_stick'}

def all_rows(gen, ep=0):
    path = f"{BASE}/generation_{gen}/episode_{ep}/agent_trace.csv"
    with open(path, newline='') as f:
        for row in csv.DictReader(f):
            yield row

def protagonist_ids(gen, ep=0):
    ids = set()
    for r in all_rows(gen, ep):
        if r.get('goal_name') in NAMED:
            ids.add(r['agent_id'])
    return ids

def rows_for(gen, eps=(0,1)):
    for ep in eps:
        try:
            ids = protagonist_ids(gen, ep)
        except FileNotFoundError:
            continue
        for r in all_rows(gen, ep):
            if r['agent_id'] in ids:   # the brained protagonists only
                yield r

def fnum(x, d=0.0):
    try: return float(x)
    except: return d

def analyze(gen):
    # accumulate per day-phase quarter (folded over the 3 daily cycles)
    # and per season quarter (monotonic - drift confounded)
    day_bins = [defaultdict(float) for _ in range(4)]
    day_cnt  = [0]*4
    sea_bins = [defaultdict(float) for _ in range(4)]
    sea_cnt  = [0]*4
    # eat-rate needs per-agent food_eaten delta
    last_food = {}
    eat_day = [0.0]*4
    for r in rows_for(gen):
        t = fnum(r['time_seconds'])
        phase = (t % DAY_LEN) / DAY_LEN          # 0..1 within a day
        dq = min(3, int(phase*4))
        sq = min(3, int((t // SEASON_LEN)))      # 0..3 over the 600s episode
        spd = math.hypot(fnum(r['vx']), fnum(r['vy']))
        goal = r.get('goal_name','na')
        food = fnum(r['food_eaten'])
        aid = r['agent_id']
        d_food = max(0.0, food - last_food.get(aid, food))
        last_food[aid] = food
        for bins, cnt, q in ((day_bins, day_cnt, dq), (sea_bins, sea_cnt, sq)):
            bins[q]['speed'] += spd
            bins[q]['find_food'] += 1.0 if goal=='find_food' else 0.0
            bins[q]['emit_signal'] += 1.0 if goal=='emit_signal' else 0.0
            bins[q]['build'] += 1.0 if goal=='build_worksite' else 0.0
            bins[q]['nf_dist'] += fnum(r['nearest_food_dist'])
            bins[q]['pred'] += fnum(r['predator_nearby_count'])
            cnt[q] += 1
        eat_day[dq] += d_food
    def norm(bins, cnt):
        out=[]
        for q in range(4):
            c = max(1, cnt[q])
            out.append({k: bins[q][k]/c for k in bins[q]})
        return out
    return norm(day_bins, day_cnt), day_cnt, norm(sea_bins, sea_cnt), sea_cnt, eat_day

def modul(profile, key):
    vals=[p.get(key,0.0) for p in profile]
    mx,mn=max(vals),min(vals); mean=sum(vals)/len(vals) or 1e-9
    return (mx-mn)/mean if mean else 0.0, vals

for gen in (0,79):
    day, dc, sea, sc, eat = analyze(gen)
    print(f"\n===== GEN {gen} (protagonists) =====")
    print("protagonist rows total:", sum(dc), "day-phase counts:", dc)
    for key in ('speed','find_food','emit_signal','nf_dist','pred'):
        m,vals = modul(day,key)
        print(f"  DAY  {key:12s} byQuarter={['%.3f'%v for v in vals]}  modul={m:.2f}")
    print("  DAY  eat_by_quarter=", ['%.3f'%v for v in eat])
    print("season counts:", sc, "(NOTE: season monotonic w/ episode time -> drift-confounded)")
    for key in ('speed','find_food','emit_signal','nf_dist','pred'):
        m,vals = modul(sea,key)
        print(f"  SEAS {key:12s} byQuarter={['%.3f'%v for v in vals]}  modul={m:.2f}")
