import os, glob, csv
base = r"D:\claude-code\c++\routes\protagonist_ecology\data\runs"
for run in ["onepot_sC_long_50001","onepot_sC_long_50002","onepot_sC_long_50003"]:
    tsdirs = sorted(glob.glob(os.path.join(base, run, "2*")), key=os.path.getmtime)
    ts = tsdirs[-1]
    fcsv = os.path.join(ts, "fitness.csv")
    gens = []
    with open(fcsv) as f:
        for row in csv.DictReader(f):
            gens.append(int(row["generation"]))
    trdir = os.path.join(ts, "trace")
    tgens = sorted([int(d.split("_")[-1]) for d in os.listdir(trdir) if d.startswith("generation_")])
    at0 = os.path.join(trdir, "generation_0", "episode_0", "agent_trace.csv")
    sz = os.path.getsize(at0) if os.path.exists(at0) else -1
    # count lines + columns in gen0 trace
    nrow = 0; ncol = 0
    with open(at0) as f:
        hdr = f.readline(); ncol = len(hdr.split(","))
        for _ in f: nrow += 1
    print(f"{run}: csv_gens={gens[0]}..{gens[-1]} n={len(gens)} | trace_gens={tgens} | gen0 trace {sz}B rows={nrow} cols={ncol}")
