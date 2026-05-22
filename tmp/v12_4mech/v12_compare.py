import csv, statistics

runs = {
    "v8 (legacy)":              "/mnt/d/claude-code/c++/data/runs/protagonist_train_a3_v8/20260522-142545",
    "v9 (legacy)":              "/mnt/d/claude-code/c++/data/runs/protagonist_train_a3_v9/20260522-185307",
    "v10 (drive+autocurr)":     "/mnt/d/claude-code/c++/data/runs/v10_species_v2/20260522-234315",
    "v11 (stable curr)":        "/mnt/d/claude-code/c++/data/runs/v11_stable/20260523-002022",
    "v12 (4-mech literature)":  "/mnt/d/claude-code/c++/data/runs/v12_4mech/20260523-005304",
}

def load(path, fn):
    with open(f"{path}/{fn}") as f:
        return list(csv.DictReader(f))

print(f"=== Behaviour (last 1/3 generations, mean per episode) ===\n")
keys = [
    ("survived_protagonists", "alive"),
    ("completed_worksites", "worksite"),
    ("chop_successes", "chops"),
    ("hunt_successes", "hunts"),
    ("craft_successes", "craft"),
    ("drank_water_events", "drink"),
    ("cooperative_co_presence_seconds", "coop_s"),
]
print(f"{'run':<26} | " + " | ".join(f"{kn:<8}" for _, kn in keys))
print("-" * 110)
for label, p in runs.items():
    rows = load(p, "protagonist_behaviors.csv")
    n = len(rows)
    tail = rows[int(n*2/3):]
    line = f"{label:<26} | "
    for k, _ in keys:
        vals = [float(r[k]) for r in tail]
        line += f"{statistics.mean(vals):<8.1f} | "
    print(line)

print(f"\n=== Fitness (last 1/3 generations) ===")
print(f"{'run':<26} | {'avg_best':>10} {'peak_best':>10} {'avg_avg':>10} {'std_best':>10} {'cv%':>6}")
print("-" * 100)
for label, p in runs.items():
    rows = load(p, "fitness.csv")
    n = len(rows)
    tail = rows[int(n*2/3):]
    bests = [float(r["best_fitness"]) for r in tail]
    avgs = [float(r["avg_fitness"]) for r in tail]
    peak = max(float(r["best_fitness"]) for r in rows)
    s = statistics.stdev(bests) if len(bests) > 1 else 0
    m = statistics.mean(bests)
    cv = abs(s/m*100) if m != 0 else 0
    print(f"{label:<26} | {m:>+10.0f} {peak:>+10.0f} {statistics.mean(avgs):>+10.0f} {s:>10.0f} {cv:>5.0f}%")

