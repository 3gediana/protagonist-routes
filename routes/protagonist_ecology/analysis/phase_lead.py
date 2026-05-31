#!/usr/bin/env python3
"""Phase-lead / anticipation rhythm metric (decision round_4 §3-C).

Idea: a system that PERCEIVES the environment should ANTICIPATE the cycle
(act BEFORE the stimulus) -> behavior LEADS the env cycle (positive lag).
A purely mechanical system only REACTS (behavior lags or is in-phase).

We cross-correlate behavior B(t) with the exogenous day-night driver at
shifted times and find the lag tau* that maximizes alignment.

Convention: r(tau) = corr( B(t), DRIVER(t+tau) ).
  tau* > 0  => B(t) aligns with FUTURE driver => behavior LEADS  => anticipation
  tau* ~ 0  => in-phase                         => mechanical/instant coupling
  tau* < 0  => B(t) aligns with PAST driver    => behavior LAGS  => reactive

Two channels:
  ACTIVITY : B = mean_speed,  DRIVER = light_level   (active by day)
  FIRE     : B = mean_fire,   DRIVER = darkness=1-light (seek fire by night/cold)
Each row = 2 sim-seconds; day_length=200s => ~100 rows/day.
"""
import csv, glob, os, math

STEP_SECONDS = 2.0          # each agg row is 2 sim-seconds
MAX_LAG = 30                # +/- 30 steps = +/- 60 s  (< half day of 100 steps)

def pearson(a, b):
    n = len(a)
    if n < 8:
        return float('nan')
    ma = sum(a)/n; mb = sum(b)/n
    va = sum((x-ma)**2 for x in a); vb = sum((y-mb)**2 for y in b)
    if va <= 1e-12 or vb <= 1e-12:
        return float('nan')
    cov = sum((a[i]-ma)*(b[i]-mb) for i in range(n))
    return cov/math.sqrt(va*vb)

def load(path):
    cols = {}
    with open(path) as f:
        r = csv.DictReader(f)
        for row in r:
            for k,v in row.items():
                cols.setdefault(k, []).append(float(v))
    return cols

def xcorr_peak(behavior, driver):
    """Return (tau*_steps, r_at_peak) maximizing |r(tau)|, sign kept.
    r(tau) = corr(behavior(t), driver(t+tau))."""
    N = len(behavior)
    best_tau, best_r, best_abs = 0, float('nan'), -1.0
    for tau in range(-MAX_LAG, MAX_LAG+1):
        bs, ds = [], []
        for t in range(N):
            j = t + tau
            if 0 <= j < N:
                bs.append(behavior[t]); ds.append(driver[j])
        r = pearson(bs, ds)
        if not math.isnan(r) and abs(r) > best_abs:
            best_abs, best_r, best_tau = abs(r), r, tau
    return best_tau, best_r

def analyze(path):
    c = load(path)
    light = c['light_level']
    darkness = [1.0 - x for x in light]
    speed = c['mean_speed']
    fire  = c['mean_fire']
    # ACTIVITY vs light (positive coupling expected)
    a_tau, a_r = xcorr_peak(speed, light)
    # FIRE vs darkness (positive coupling expected: more fire when dark/cold)
    f_tau, f_r = xcorr_peak(fire, darkness)
    return {
        'rows': len(light),
        'act_tau_s': a_tau*STEP_SECONDS, 'act_r': a_r,
        'fire_tau_s': f_tau*STEP_SECONDS, 'fire_r': f_r,
    }

def lead_label(tau_s):
    if tau_s > 4:  return 'LEAD(anticip)'
    if tau_s < -4: return 'LAG(reactive)'
    return 'in-phase(mech)'

files = sorted(glob.glob('/home/ubuntu/pe_analysis/agg_*.csv'))
print(f"{'label':22} {'rows':>4} | {'ACTIVITY(speed~light)':>34} | {'FIRE(fire~darkness)':>34}")
print(f"{'':22} {'':>4} | {'tau*(s)':>10} {'r':>8} {'verdict':>14} | {'tau*(s)':>10} {'r':>8} {'verdict':>14}")
print('-'*120)
rows = []
for p in files:
    lab = os.path.basename(p).replace('agg_','').replace('.csv','')
    d = analyze(p)
    rows.append((lab,d))
    print(f"{lab:22} {d['rows']:>4} | {d['act_tau_s']:>10.1f} {d['act_r']:>8.3f} {lead_label(d['act_tau_s']):>14} | "
          f"{d['fire_tau_s']:>10.1f} {d['fire_r']:>8.3f} {lead_label(d['fire_tau_s']):>14}")
