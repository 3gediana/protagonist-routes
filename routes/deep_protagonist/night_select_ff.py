import json, glob, os, struct, sys
d = sys.argv[1] if len(sys.argv)>1 else r"D:\claude-code\c++\routes\deep_protagonist\runs\night_dagger_ff"
def a17(binp):
    with open(binp,'rb') as f: data=f.read()
    if data[:4]!=b'DPB1': return -1
    _,od,cd=struct.unpack_from('<III',data,4); rec=od*4+cd*4+8; body=data[24:]
    n=len(body)//rec; off=od*4+cd*4; c=0
    for i in range(n):
        if struct.unpack_from('<I',body,i*rec+off)[0]==17: c+=1
    return c
def metrics(jpath):
    eps=[]
    for line in open(jpath,'r',errors='replace'):
        line=line.strip()
        if not line.startswith('{'): continue
        try: o=json.loads(line)
        except: continue
        nt=o.get('night_total',0); ns=o.get('night_shelter',0)
        eps.append(dict(nshel=(ns/nt if nt>0 else 0.0),bldg=o.get('bldg_ticks',0),
                        atk=(o.get('act',[0]*18)[2] if len(o.get('act',[]))>2 else 0)))
    if not eps: return None
    n=len(eps)
    return dict(n=n,nshel=sum(e['nshel'] for e in eps)/n,bldg=sum(e['bldg'] for e in eps)/n,atk=sum(e['atk'] for e in eps))
keep=[]
print(f"{'seed':>10}{'nShel':>7}{'bldg':>8}{'atk':>5}{'a17':>5}  -> sel(nShel<0.6 & bldg>0 & atk==0 & a17==0)")
for j in sorted(glob.glob(os.path.join(d,"night_*.jsonl"))):
    seed=os.path.basename(j).replace("night_","").replace(".jsonl","")
    m=metrics(j)
    if m is None: continue
    binp=os.path.join(d,f"night_{seed}.bin"); fire=a17(binp) if os.path.exists(binp) else -1
    sel=(m['nshel']<0.6 and m['bldg']>0 and m['atk']==0 and fire==0 and os.path.exists(binp))
    if sel: keep.append(binp)
    print(f"{seed:>10}{m['nshel']:>7.2f}{m['bldg']:>8.0f}{m['atk']:>5}{fire:>5}  -> {'KEEP' if sel else 'drop'}")
print(f"\nCLEAN_RESIDUAL_KEEP={len(keep)}")
print("KEEP_LIST="+",".join(keep))
