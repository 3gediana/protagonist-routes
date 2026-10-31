#!/usr/bin/env python3
"""protagonist_ecology  MULTI-BEHAVIOR early-generation predictor (D-123).

PE's analogue of deep_protagonist's few-minutes early-warning dashboard. A full
80-gen long-run takes hours; this reads only the EARLY generations (default
0..15, a few-minute smoke) and predicts the gen-79 verdict for EVERY behavior
channel at once, so a parameter change can be screened in minutes instead of a
full run.

Behavior channels (multi-behavior, not just fire):
  RHYTHM  (the Phase-B bottleneck, from per-tick traces, harmonic regression):
    fire_amp   amplitude of the daily fire-seeking oscillation  (ceiling ~0.009)
    fire_r2    how day-locked the fire behavior is              (gate >0.05)
    fire_lead  seconds the fire peak LEADS the cold trough      (>+6s = anticip)
    speed_amp / speed_r2 / speed_lead  (daily activity rhythm vs light)
  HARMONY C1-C7 (the "don't break the ecosystem" floors, from per-gen CSVs):
    C1 hunters active   apex/pack/ambush predator populations
    C2 herbivore attrition  rabbit/large_herb/small_forager present under predation
    C3 coop co-presence cooperative_co_presence_seconds
    C4 scavengers fed   omnivore/scavenger populations
    C5 worksites        completed_worksites
    C6 coverage         me_coverage   (gate >=0.30)
    C7 no extinction    every archetype population > 0
  PLUS raw behavior rates: hunt_successes, chop/craft, deposits, signal_response.

Two modes:
  report   <run_dir>           print early(0..K)->gen79 forecast for one run.
  validate <run_dir> [...]     for each run, FORECAST gen79 from gens 0..K and
                               compare to the ACTUAL gen79 (which we already
                               have), plus leave-one-seed-out calibration, and
                               report the prediction error so we know how much
                               to trust a K-gen smoke. THIS is what tells us the
                               predictor is safe to use before any new long run.

Pure stdlib (matches analysis/phase_harmonic.py). day_length=200s, agg row=2s.
"""
import os, sys, glob, csv, math, json

DAY_LEN_S = 200.0
EARLY_K   = 15          # last early generation used for the forecast window
TARGET    = 79          # generation whose verdict we forecast
FIRE_AMP_CEIL = 0.009   # owner's amplitude "ceiling" to break (knowledge note)
FIRE_AMP_GATE = 0.003   # phase_harmonic.py rhythm-present gate
FIRE_R2_GATE  = 0.05

# agent_trace.csv FIXED column indices (from analysis/rhythm_agg.ps1)
I_step, I_vx, I_vy, I_alive, I_fire, I_tod, I_light, I_temp = 0, 7, 8, 9, 41, 60, 61, 63

ARCHETYPES = ["large_herbivore","small_forager","apex_predator","pack_hunter",
              "ambush_predator","scavenger","omnivore","rabbit"]

# ----------------------------------------------------------------------------
# math helpers (no numpy; same harmonic regression as phase_harmonic.py)
# ----------------------------------------------------------------------------
def solve3(M, r):
    A = [row[:] + [r[i]] for i, row in enumerate(M)]
    for col in range(3):
        piv = max(range(col, 3), key=lambda i: abs(A[i][col]))
        if abs(A[piv][col]) < 1e-12: return None
        A[col], A[piv] = A[piv], A[col]
        f = A[col][col]; A[col] = [x / f for x in A[col]]
        for i in range(3):
            if i != col:
                f = A[i][col]; A[i] = [A[i][k] - f * A[col][k] for k in range(4)]
    return [A[0][3], A[1][3], A[2][3]]

def harmonic_fit(tod, y):
    """y = a + b cos(2pi tod) + c sin(2pi tod). Returns (amp, peak_phi, R2)."""
    n = len(y)
    if n < 4: return float('nan'), float('nan'), float('nan')
    th = [2*math.pi*t for t in tod]
    cs = [math.cos(x) for x in th]; sn = [math.sin(x) for x in th]
    S1=n; Sc=sum(cs); Ss=sum(sn)
    Scc=sum(x*x for x in cs); Sss=sum(x*x for x in sn); Scs=sum(cs[i]*sn[i] for i in range(n))
    Sy=sum(y); Scy=sum(cs[i]*y[i] for i in range(n)); Ssy=sum(sn[i]*y[i] for i in range(n))
    abc = solve3([[S1,Sc,Ss],[Sc,Scc,Scs],[Ss,Scs,Sss]], [Sy,Scy,Ssy])
    if abc is None: return float('nan'), float('nan'), float('nan')
    a,b,c = abc
    amp = math.sqrt(b*b+c*c)
    phi = math.atan2(c,b)/(2*math.pi)
    if phi < 0: phi += 1.0
    my = Sy/n
    sst = sum((v-my)**2 for v in y)
    pred = [a+b*cs[i]+c*sn[i] for i in range(n)]
    sse = sum((y[i]-pred[i])**2 for i in range(n))
    r2 = 1 - sse/sst if sst > 1e-12 else float('nan')
    return amp, phi, r2

def wrap_half(x):
    while x > 0.5: x -= 1.0
    while x < -0.5: x += 1.0
    return x

def linfit(xs, ys):
    """Least-squares slope/intercept of ys vs xs. Returns (slope, intercept)."""
    n = len(xs)
    if n < 2: return 0.0, (ys[0] if ys else 0.0)
    mx = sum(xs)/n; my = sum(ys)/n
    sxx = sum((x-mx)**2 for x in xs); sxy = sum((xs[i]-mx)*(ys[i]-my) for i in range(n))
    if sxx < 1e-12: return 0.0, my
    slope = sxy/sxx
    return slope, my - slope*mx

# ----------------------------------------------------------------------------
# per-run extraction
# ----------------------------------------------------------------------------
def latest_ts_dir(run_dir):
    sub = [d for d in glob.glob(os.path.join(run_dir, "2*")) if os.path.isdir(d)]
    if not sub:  # run_dir may itself already be a timestamp dir
        return run_dir
    return max(sub, key=os.path.getmtime)

def read_csv_col(path, key):
    out = {}
    if not os.path.exists(path): return out
    with open(path) as f:
        for row in csv.DictReader(f):
            try: out[int(row["generation"])] = float(row[key])
            except (KeyError, ValueError): pass
    return out

def read_species_pop(path):
    """gen -> {species: population}."""
    out = {}
    if not os.path.exists(path): return out
    with open(path) as f:
        for row in csv.DictReader(f):
            try:
                g = int(row["generation"]); sp = row["species"]; pop = float(row["population"])
            except (KeyError, ValueError): continue
            out.setdefault(g, {})[sp] = pop
    return out

def rhythm_for_gen(ts_dir, gen):
    """Aggregate one generation's trace per step (mean over alive agents) and
    harmonic-fit fire & speed vs time_of_day. Returns dict or None."""
    at = os.path.join(ts_dir, "trace", f"generation_{gen}", "episode_0", "agent_trace.csv")
    if not os.path.exists(at): return None
    agg = {}  # step -> [n, tod, light, temp, sum_speed, sum_fire]
    with open(at) as f:
        f.readline()
        for ln in f:
            c = ln.split(",")
            if len(c) <= I_temp: continue
            if c[I_alive] != "1": continue
            s = c[I_step]
            try:
                vx = float(c[I_vx]); vy = float(c[I_vy]); fire = float(c[I_fire])
                tod = float(c[I_tod]); light = float(c[I_light]); temp = float(c[I_temp])
            except ValueError: continue
            a = agg.get(s)
            if a is None:
                agg[s] = [1, tod, light, temp, math.hypot(vx,vy), fire]
            else:
                a[0]+=1; a[1]=tod; a[2]=light; a[3]=temp
                a[4]+=math.hypot(vx,vy); a[5]+=fire
    if len(agg) < 4: return None
    tod=[]; light=[]; temp=[]; speed=[]; fire=[]
    for s in sorted(agg, key=lambda x: int(x)):
        n,td,li,tp,sp,fr = agg[s]
        tod.append(td); light.append(li); temp.append(tp)
        speed.append(sp/n); fire.append(fr/n)
    fa, fp, fr2 = harmonic_fit(tod, fire)
    sa, sp_, sr2 = harmonic_fit(tod, speed)
    la, lp, lr2 = harmonic_fit(tod, light)
    ta, tp, tr2 = harmonic_fit(tod, temp)
    dark_phi = (lp+0.5) % 1.0
    temp_trough = (tp+0.5) % 1.0
    fire_lead = wrap_half(temp_trough - fp) * DAY_LEN_S   # fire peak vs cold trough
    speed_lead = wrap_half(lp - sp_) * DAY_LEN_S          # speed peak vs light
    return dict(fire_amp=fa, fire_r2=fr2, fire_lead=fire_lead,
                speed_amp=sa, speed_r2=sr2, speed_lead=speed_lead,
                ticks=len(tod))

def extract_run(run_dir):
    ts = latest_ts_dir(run_dir)
    cov  = read_csv_col(os.path.join(ts, "fitness.csv"), "me_coverage")
    qd   = read_csv_col(os.path.join(ts, "fitness.csv"), "me_qd_score")
    bf   = read_csv_col(os.path.join(ts, "fitness.csv"), "best_fitness")
    pb   = os.path.join(ts, "protagonist_behaviors.csv")
    hunt = read_csv_col(pb, "hunt_successes")
    work = read_csv_col(pb, "completed_worksites")
    coop = read_csv_col(pb, "cooperative_co_presence_seconds")
    sig  = read_csv_col(pb, "signal_response_events")
    pops = read_species_pop(os.path.join(ts, "species_metrics.csv"))
    # rhythm only at gens with traces
    trdir = os.path.join(ts, "trace")
    tgens = sorted([int(d.split("_")[-1]) for d in os.listdir(trdir)
                    if d.startswith("generation_")]) if os.path.isdir(trdir) else []
    rhythm = {}
    for g in tgens:
        r = rhythm_for_gen(ts, g)
        if r: rhythm[g] = r
    return dict(ts=ts, cov=cov, qd=qd, bf=bf, hunt=hunt, work=work,
                coop=coop, sig=sig, pops=pops, rhythm=rhythm, tgens=tgens)

# ----------------------------------------------------------------------------
# forecasting
# ----------------------------------------------------------------------------
def series_in_window(series, lo, hi):
    xs = sorted(g for g in series if lo <= g <= hi)
    return xs, [series[g] for g in xs]

def forecast_metric(series, k=EARLY_K, target=TARGET):
    """Two honest forecasts from the early window:
       persistence = mean of the window's last 5 points,
       linear      = LS line over the window, evaluated at target.
       Returns (persistence, linear, slope, early_level)."""
    xs, ys = series_in_window(series, 0, k)
    if not ys: return None
    last5 = ys[-5:] if len(ys) >= 5 else ys
    persistence = sum(last5)/len(last5)
    slope, intercept = linfit(xs, ys)
    linear = slope*target + intercept
    return dict(persistence=persistence, linear=linear, slope=slope,
                early_level=persistence, n=len(ys))

def actual_at(series, target=TARGET):
    if target in series: return series[target]
    # nearest available <= target
    cand = [g for g in series if g <= target]
    return series[max(cand)] if cand else None

CHANNELS_CSV = [("coverage","cov"),("qd_score","qd"),("best_fit","bf"),
                ("hunt_succ","hunt"),("worksites","work"),
                ("coop_seconds","coop"),("signal_resp","sig")]
CHANNELS_RHY = ["fire_amp","fire_r2","fire_lead","speed_amp","speed_r2","speed_lead"]

def rhythm_series(run, key):
    return {g: run["rhythm"][g][key] for g in run["rhythm"] if not math.isnan(run["rhythm"][g][key])}

def harmony_at(run, gen):
    """Evaluate the 7 harmony floors at a generation (best-effort from CSVs)."""
    pops = run["pops"].get(gen, {})
    def p(name): return pops.get(name, 0.0)
    C1 = p("apex_predator")>=2 and p("pack_hunter")>=7 and p("ambush_predator")>=3
    C2 = p("rabbit")>0 and p("large_herbivore")>0 and p("small_forager")>0
    C3 = run["coop"].get(gen, 0.0) > 0
    C4 = p("omnivore")>0 and p("scavenger")>0
    C5 = run["work"].get(gen, 0.0) > 0
    C6 = run["cov"].get(gen, 0.0) >= 0.30
    C7 = all(p(a) > 0 for a in ARCHETYPES)
    flags = dict(C1=C1,C2=C2,C3=C3,C4=C4,C5=C5,C6=C6,C7=C7)
    return flags, sum(flags.values())

# ----------------------------------------------------------------------------
# reporting
# ----------------------------------------------------------------------------
def fmt(x, nd=3):
    if x is None or (isinstance(x,float) and math.isnan(x)): return "  n/a"
    return f"{x:+.{nd}f}"

def report_run(run_dir):
    run = extract_run(run_dir)
    name = os.path.basename(run_dir.rstrip("/\\"))
    print(f"\n=== EARLY->gen{TARGET} FORECAST  [{name}]  (window gen0..{EARLY_K}) ===")
    print(f"{'channel':14} | {'early_lvl':>10} {'slope/gen':>10} | {'pred(persist)':>13} {'pred(linear)':>12} | {'actual g'+str(TARGET):>12}")
    print("-"*92)
    # rhythm channels (the bottleneck)
    for key in CHANNELS_RHY:
        s = rhythm_series(run, key)
        if not s: continue
        f = forecast_metric(s)
        if not f: continue
        act = actual_at(s)
        print(f"{key:14} | {fmt(f['early_level']):>10} {fmt(f['slope'],4):>10} | "
              f"{fmt(f['persistence']):>13} {fmt(f['linear']):>12} | {fmt(act):>12}")
    print("-"*92)
    for label, fld in CHANNELS_CSV:
        s = run[fld]
        if not s: continue
        f = forecast_metric(s)
        if not f: continue
        act = actual_at(s)
        nd = 3 if label in ("coverage",) else 1
        print(f"{label:14} | {fmt(f['early_level'],nd):>10} {fmt(f['slope'],nd):>10} | "
              f"{fmt(f['persistence'],nd):>13} {fmt(f['linear'],nd):>12} | {fmt(act,nd):>12}")
    # rhythm verdict from EARLY window only
    fa = rhythm_series(run, "fire_amp"); fl = rhythm_series(run, "fire_lead"); fr = rhythm_series(run, "fire_r2")
    if fa:
        ef = forecast_metric(fa); efl = forecast_metric(fl); efr = forecast_metric(fr)
        pa = ef["persistence"]; pl = efl["persistence"]; pr = efr["persistence"]
        rhythm_present = (pa > FIRE_AMP_GATE and pr > FIRE_R2_GATE)
        anticip = pl > 6.0
        # Forecast the gen79 amplitude with the VALIDATED forecaster: persistence
        # (early gen11-15 mean) tracks final amp within ~0.002 across the 3 sC
        # seeds, whereas a linear fit over a 64-gen horizon overshoots wildly
        # (validation MAE 0.0037 vs 0.0020) and must NOT drive the verdict.
        breaks_ceiling = pa >= FIRE_AMP_CEIL
        print("-"*92)
        print(f"EARLY RHYTHM VERDICT: present={rhythm_present} (amp~{pa:.4f}>{FIRE_AMP_GATE}, R2~{pr:.3f}>{FIRE_R2_GATE}) | "
              f"anticipation={anticip} (fire_lead~{pl:+.1f}s {'LEAD' if pl>6 else 'LAG' if pl<-6 else 'in-phase'}) | "
              f"amp_breaks_ceiling({FIRE_AMP_CEIL})={breaks_ceiling} (persist->{pa:.4f}; linear {ef['linear']:.4f} unreliable@64gen)")
    # harmony floors at early vs target
    he, hen = harmony_at(run, min(run['cov'], key=lambda g: abs(g-EARLY_K)) if run['cov'] else EARLY_K)
    ht, htn = harmony_at(run, TARGET)
    print(f"HARMONY @early~{EARLY_K}: {hen}/7 {he}")
    print(f"HARMONY @gen{TARGET}:    {htn}/7 {ht}")
    return run

def validate(run_dirs):
    runs = [(os.path.basename(d.rstrip('/\\')), extract_run(d)) for d in run_dirs]
    print(f"\n################ VALIDATION  (forecast gen{TARGET} from gen0..{EARLY_K}) ################")
    # collect, per channel, predicted vs actual across seeds; report MAE of each
    # forecaster + leave-one-seed-out affine calibration.
    chans = [("fire_amp", lambda r: rhythm_series(r,"fire_amp"), 4),
             ("fire_r2",  lambda r: rhythm_series(r,"fire_r2"), 3),
             ("fire_lead",lambda r: rhythm_series(r,"fire_lead"),1),
             ("coverage", lambda r: r["cov"], 3),
             ("hunt_succ",lambda r: r["hunt"], 1),
             ("worksites",lambda r: r["work"], 1)]
    for cname, getter, nd in chans:
        rows = []
        for name, run in runs:
            s = getter(run)
            if not s: continue
            f = forecast_metric(s); act = actual_at(s)
            if f is None or act is None: continue
            rows.append((name, f, act))
        if len(rows) < 2:
            print(f"\n[{cname}] insufficient data"); continue
        # forecaster errors
        mae_p = sum(abs(f['persistence']-act) for _,f,act in rows)/len(rows)
        mae_l = sum(abs(f['linear']-act)     for _,f,act in rows)/len(rows)
        # leave-one-seed-out affine: actual ~ A*early_level + B fit on others
        loso_err = []
        for i in range(len(rows)):
            train = [rows[j] for j in range(len(rows)) if j != i]
            xs = [f['early_level'] for _,f,_ in train]; ys = [act for _,_,act in train]
            A, B = linfit(xs, ys)
            _, f_i, act_i = rows[i]
            loso_err.append(abs(A*f_i['early_level']+B - act_i))
        mae_loso = sum(loso_err)/len(loso_err)
        actmean = sum(abs(act) for _,_,act in rows)/len(rows) or 1.0
        print(f"\n[{cname}]  actual_g{TARGET} mean|val|={actmean:.{nd}f}")
        for name, f, act in rows:
            print(f"   {name:24} early_lvl={f['early_level']:.{nd}f} "
                  f"persist={f['persistence']:.{nd}f} linear={f['linear']:.{nd}f} "
                  f"ACTUAL={act:.{nd}f}")
        print(f"   MAE persistence={mae_p:.{nd}f}  linear={mae_l:.{nd}f}  "
              f"LOSO-affine={mae_loso:.{nd}f}  (rel persist={mae_p/actmean*100:.0f}%)")
    # harmony verdict agreement: does early 7/7 count predict gen79 count?
    print(f"\n[harmony]  early(~{EARLY_K}) vs gen{TARGET} pass-count agreement:")
    for name, run in runs:
        eg = min(run['cov'], key=lambda g: abs(g-EARLY_K)) if run['cov'] else EARLY_K
        _, hen = harmony_at(run, eg); _, htn = harmony_at(run, TARGET)
        print(f"   {name:24} early={hen}/7  gen{TARGET}={htn}/7  match={'Y' if hen==htn else 'N'}")

def main():
    if len(sys.argv) < 3:
        print("usage: pe_early_predictor.py [report|validate] <run_dir> [run_dir ...]")
        sys.exit(2)
    mode = sys.argv[1]; dirs = sys.argv[2:]
    if mode == "report":
        for d in dirs: report_run(d)
    elif mode == "validate":
        validate(dirs)
    else:
        print("unknown mode", mode); sys.exit(2)

if __name__ == "__main__":
    main()
