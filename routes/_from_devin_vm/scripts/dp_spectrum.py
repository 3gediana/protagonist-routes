import json

LABELS = ["eat","drink","HUNT","collect","shelter","spear","sleep","grassdress",
          "furcloak","wear","blueprint","cyclebld","deposit","plant","water",
          "feedtame","NOOP","tendfire","COOK","MINE","axe","pickaxe","monument"]
MS = ["drink","eat","collect","spear","shelter","clothing","seed","house","crop","follower"]

def analyze(path, name):
    eps = [json.loads(l) for l in open(path) if l.strip()]
    n = len(eps)
    print("="*74)
    print(f"DP 涌现谱: {name}   (eps={n})")
    print("="*74)
    # action sampling: mean count + % of eps used
    print(f"\n{'action':<12}{'mean/ep':>10}{'eps_used%':>11}")
    for i,lab in enumerate(LABELS):
        cnts = [e['act'][i] for e in eps if len(e.get('act',[]))>i]
        if not cnts: continue
        mean = sum(cnts)/n
        used = 100*sum(1 for c in cnts if c>0)/n
        flag = "  <-- DEAD" if used==0 else ""
        print(f"{lab:<12}{mean:>10.1f}{used:>10.0f}%{flag}")
    # milestone unlock rates
    print(f"\n{'milestone':<12}{'unlock%':>10}")
    for i,m in enumerate(MS):
        rate = 100*sum(1 for e in eps if e.get('unlocks',[0]*10)[i]==1)/n
        flag = "  <-- NEVER" if rate==0 else ""
        print(f"{m:<12}{rate:>9.0f}%{flag}")
    avg_unlocks = sum(sum(e.get('unlocks',[])) for e in eps)/n
    print(f"\navg unlocks/ep = {avg_unlocks:.2f} / 10")
    # outcomes
    kills = sum(e.get('r_kills',0)!=0 for e in eps)
    houses = sum(e.get('houses_built',0) for e in eps)
    dc = sum(e.get('deaths_cold',0) for e in eps)
    df = sum(e.get('deaths_food',0) for e in eps)
    dp_ = sum(e.get('deaths_protein',0) for e in eps)
    dv = sum(e.get('deaths_vitamin',0) for e in eps)
    surv = sum(1 for e in eps if e.get('deaths',0)==0)
    print(f"houses_built(total)={houses}  full_survive_eps={surv}/{n}")
    print(f"death_causes: cold={dc} food={df} protein={dp_} vitamin={dv}")

analyze("/home/ubuntu/dp_p0_bc10.jsonl", "ft_bc10 (BC+hunt finetune)")
print()
analyze("/home/ubuntu/dp_p0_champ.jsonl", "r1_cook_s24 (champion)")
