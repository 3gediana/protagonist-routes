import csv, statistics

def load(p):
    with open(p) as f:
        return list(csv.DictReader(f))

for label, path in [
    ("v11", "/mnt/d/claude-code/c++/data/runs/v11_stable/20260523-002022/fitness.csv"),
    ("v12", "/mnt/d/claude-code/c++/data/runs/v12_4mech/20260523-005304/fitness.csv"),
]:
    rows = load(path)
    print(f"\n=== {label} best_fitness rolling (10-gen mean) ===")
    by_gen = {int(r["generation"]): float(r["best_fitness"]) for r in rows}
    gens = sorted(by_gen.keys())
    print(f"{'gen_range':<10} {'mean':>10} {'std':>10}")
    for i in range(0, len(gens), 10):
        chunk = gens[i:i+10]
        vals = [by_gen[g] for g in chunk]
        m = statistics.mean(vals)
        s = statistics.stdev(vals) if len(vals)>1 else 0
        print(f"{chunk[0]:>3}-{chunk[-1]:<3}    {m:>+10.0f} {s:>10.0f}")
