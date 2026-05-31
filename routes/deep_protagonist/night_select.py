import json, glob, os, struct
d = r"D:\claude-code\c++\routes\deep_protagonist\runs\night_dagger"

def clone_metrics(jpath):
    eps=[]
    for line in open(jpath,'r',errors='replace'):
        line=line.strip()
        if not line.startswith('{'): continue
        try: o=json.loads(line)
        except: continue
        nt=o.get('night_total',0); ns=o.get('night_shelter',0)
        eps.append(dict(nshel=(ns/nt if nt>0 else 0.0),
                        houses=o.get('houses_built',0),
                        bldg=o.get('bldg_ticks',0),
                        atk=(o.get('act',[0]*18)[2] if len(o.get('act',[]))>2 else 0)))
    if not eps: return None
    n=len(eps)
    return dict(n=n,
                nshel=sum(e['nshel'] for e in eps)/n,
                houses=sum(e['houses'] for e in eps)/n,
                bldg=sum(e['bldg'] for e in eps)/n,
                atk=sum(e['atk'] for e in eps))

keep=[]
print(f"{'seed':>10} {'nShel':>5} {'houses':>6} {'bldg':>7} {'atk':>4} -> sel")
for j in sorted(glob.glob(os.path.join(d,"night_*.jsonl"))):
    seed=os.path.basename(j).replace("night_","").replace(".jsonl","")
    m=clone_metrics(j)
    if m is None: continue
    binp=os.path.join(d,f"night_{seed}.bin")
    # correction-rich = builds something but doesn't sleep in it, no wolf contamination
    sel = (m['nshel']<0.6 and m['bldg']>0 and m['atk']==0 and os.path.exists(binp))
    tag="KEEP" if sel else "drop"
    if sel: keep.append(binp)
    print(f"{seed:>10} {m['nshel']:>5.2f} {m['houses']:>6.1f} {m['bldg']:>7.0f} {m['atk']:>4} -> {tag}")
print(f"\nKEEP files={len(keep)}")
print("KEEP_LIST="+",".join(keep))
