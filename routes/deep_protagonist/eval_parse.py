import json, glob, os, sys

# usage: python eval_parse.py <tag> [seed1,seed2,...]
tag = sys.argv[1] if len(sys.argv) > 1 else "v11"
d = r"D:\claude-code\c++\routes\deep_protagonist\runs\eval_v11"

def seed_stats(path):
    eps=[]
    for line in open(path, 'r', errors='replace'):
        line=line.strip()
        if not line or not line.startswith('{'): continue
        try: o=json.loads(line)
        except: continue
        nt=o.get('night_total',0); ns=o.get('night_shelter',0)
        nshel = (ns/nt) if nt>0 else 0.0
        bldg = o.get('bldg_ticks',0)
        act = o.get('act',[0]*18)
        fire = act[17] if len(act)>17 else 0
        atk  = act[2]  if len(act)>2  else 0
        passed = 1 if (bldg>=5000 and nshel>=0.80) else 0
        eps.append(dict(nshel=nshel,bldg=bldg,fire=fire,atk=atk,passed=passed,
                        deaths=o.get('deaths',0),df=o.get('deaths_food',0),
                        dc=o.get('deaths_cold',0),houses=o.get('houses_built',0)))
    n=len(eps) or 1
    return dict(
        n=len(eps),
        passpct=round(100*sum(e['passed'] for e in eps)/n,0),
        nshel=round(sum(e['nshel'] for e in eps)/n,2),
        fire=sum(e['fire'] for e in eps),
        atk=sum(e['atk'] for e in eps),
        deaths=sum(e['deaths'] for e in eps),
        df=sum(e['df'] for e in eps),
        dc=sum(e['dc'] for e in eps),
        houses=round(sum(e['houses'] for e in eps)/n,1),
    )

files = sorted(glob.glob(os.path.join(d, f"{tag}_*.jsonl")))
print(f"{'seed':>9} {'pass%':>5} {'nShel':>5} {'fire':>5} {'atk':>4} {'deaths':>6} {'food':>5} {'cold':>5} {'houses':>6}")
passes=[]
for f in files:
    seed=os.path.basename(f).replace(f"{tag}_","").replace(".jsonl","")
    s=seed_stats(f)
    passes.append(s['passpct'])
    print(f"{seed:>9} {s['passpct']:>5.0f} {s['nshel']:>5.2f} {s['fire']:>5} {s['atk']:>4} {s['deaths']:>6} {s['df']:>5} {s['dc']:>5} {s['houses']:>6.1f}")
if passes:
    passes_sorted=sorted(passes); mid=passes_sorted[len(passes_sorted)//2]
    print(f"\nmedian pass% = {mid}  (n_seeds={len(passes)})")
