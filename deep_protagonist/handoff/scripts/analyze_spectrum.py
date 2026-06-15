#!/usr/bin/env python3
"""
Read a frozen-eval jsonl (from eval_frozen.ps1) and print the DP "emergence spectrum":
which of the 23 actions are actually sampled, which of the 10 milestones unlock,
survival rate, and death causes. Run on the box (python) or copy the jsonl back to your VM.

Usage:
    python analyze_spectrum.py runs/eval_frozen.jsonl
    python analyze_spectrum.py runs/eval_frozen.jsonl "ft_bc10"
"""
import json, sys

LABELS = ["eat","drink","HUNT","collect","shelter","spear","sleep","grassdress",
          "furcloak","wear","blueprint","cyclebld","deposit","plant","water",
          "feedtame","NOOP","tendfire","COOK","MINE","axe","pickaxe","monument"]
MS = ["drink","eat","collect","spear","shelter","clothing","seed","house","crop","follower"]

def analyze(path, name=None):
    eps = [json.loads(l) for l in open(path) if l.strip()]
    n = len(eps)
    if n == 0:
        print(f"(no episodes in {path})"); return
    print("="*74); print(f"DP emergence spectrum: {name or path}   (eps={n})"); print("="*74)
    used_count = 0
    print(f"\n{'action':<12}{'mean/ep':>10}{'eps_used%':>11}")
    for i,lab in enumerate(LABELS):
        cnts = [e['act'][i] for e in eps if len(e.get('act',[]))>i]
        if not cnts: continue
        mean = sum(cnts)/n
        used = 100*sum(1 for c in cnts if c>0)/n
        if used>0: used_count += 1
        flag = "  <-- DEAD" if used==0 else ""
        print(f"{lab:<12}{mean:>10.1f}{used:>10.0f}%{flag}")
    print(f"\n--> {used_count}/23 actions used")
    print(f"\n{'milestone':<12}{'unlock%':>10}")
    unlocked = 0
    for i,m in enumerate(MS):
        rate = 100*sum(1 for e in eps if e.get('unlocks',[0]*10)[i]==1)/n
        if rate>0: unlocked += 1
        flag = "  <-- NEVER" if rate==0 else ""
        print(f"{m:<12}{rate:>9.0f}%{flag}")
    avg = sum(sum(e.get('unlocks',[])) for e in eps)/n
    print(f"\navg unlocks/ep = {avg:.2f}/10   ({unlocked}/10 ever unlocked)")
    surv = sum(1 for e in eps if e.get('deaths',0)==0)
    dc = sum(e.get('deaths_cold',0) for e in eps); df = sum(e.get('deaths_food',0) for e in eps)
    dpr= sum(e.get('deaths_protein',0) for e in eps); dv = sum(e.get('deaths_vitamin',0) for e in eps)
    print(f"full_survive_eps={surv}/{n}   death_causes: cold={dc} food={df} protein={dpr} vitamin={dv}")

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print(__doc__); sys.exit(1)
    analyze(sys.argv[1], sys.argv[2] if len(sys.argv) > 2 else None)
