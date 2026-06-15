#!/usr/bin/env python3
"""Circadian phase-lead via HARMONIC REGRESSION (decision round_4 §3-C, robust).

Cross-correlation aliases on periodic signals. The standard circadian tool is
harmonic regression against the known day phase (time_of_day in [0,1)):

    B(t) ~ a + b*cos(2*pi*tod) + c*sin(2*pi*tod)

From the fit:
  amplitude A = sqrt(b^2+c^2)           -> strength of the daily oscillation
  acrophase  phi = atan2(c,b)/(2pi)     -> day-fraction at which B peaks
  R2 of the daily harmonic              -> how much of B's variance is day-locked

ANTICIPATION test = phase LEAD of behavior vs the exogenous light driver:
  lead(behavior) = wrap(phi_behavior - phi_light) in day-fraction, x day_len(s).
  >0 (small) => behavior peaks BEFORE the driver => anticipation (perception)
  ~0         => in-phase => mechanical/instant coupling
  <0         => behavior peaks AFTER the driver => reactive lag

Channels:
  ACTIVITY: B=mean_speed (expect peak near midday, in/around light peak)
  FIRE    : B=mean_fire  (expect peak near night, ~antiphase to light)
day_length_seconds=200; each agg row = 2 s.
"""
import csv, glob, os, math

DAY_LEN_S = 200.0

def load(path):
    cols = {}
    with open(path) as f:
        for row in csv.DictReader(f):
            for k,v in row.items():
                cols.setdefault(k, []).append(float(v))
    return cols

def harmonic_fit(tod, y):
    """LS fit y = a + b*cos(th) + c*sin(th), th=2pi*tod. Returns (amp, phi_dayfrac, R2)."""
    n = len(y)
    th = [2*math.pi*t for t in tod]
    cs = [math.cos(x) for x in th]
    sn = [math.sin(x) for x in th]
    # normal equations for [a,b,c]
    S1=n; Sc=sum(cs); Ss=sum(sn)
    Scc=sum(x*x for x in cs); Sss=sum(x*x for x in sn); Scs=sum(cs[i]*sn[i] for i in range(n))
    Sy=sum(y); Scy=sum(cs[i]*y[i] for i in range(n)); Ssy=sum(sn[i]*y[i] for i in range(n))
    # 3x3 matrix M * [a,b,c] = rhs
    M=[[S1,Sc,Ss],[Sc,Scc,Scs],[Ss,Scs,Sss]]
    rhs=[Sy,Scy,Ssy]
    abc=solve3(M,rhs)
    if abc is None:
        return float('nan'),float('nan'),float('nan')
    a,b,c=abc
    amp=math.sqrt(b*b+c*c)
    phi=math.atan2(c,b)/(2*math.pi)   # in [-0.5,0.5] day-fraction (peak location)
    if phi<0: phi+=1.0
    # R2 of harmonic
    my=Sy/n
    sst=sum((v-my)**2 for v in y)
    pred=[a+b*cs[i]+c*sn[i] for i in range(n)]
    sse=sum((y[i]-pred[i])**2 for i in range(n))
    r2=1-sse/sst if sst>1e-12 else float('nan')
    return amp,phi,r2

def solve3(M,r):
    import copy
    A=[row[:]+[r[i]] for i,row in enumerate(M)]
    for col in range(3):
        piv=max(range(col,3),key=lambda i:abs(A[i][col]))
        if abs(A[piv][col])<1e-12: return None
        A[col],A[piv]=A[piv],A[col]
        f=A[col][col]
        A[col]=[x/f for x in A[col]]
        for i in range(3):
            if i!=col:
                f=A[i][col]
                A[i]=[A[i][k]-f*A[col][k] for k in range(4)]
    return [A[0][3],A[1][3],A[2][3]]

def wrap_half(x):
    while x>0.5: x-=1.0
    while x<-0.5: x+=1.0
    return x

def verdict(lead_s, amp_ok):
    if not amp_ok: return 'no-rhythm'
    if lead_s>6:  return 'LEAD(anticip)'
    if lead_s<-6: return 'LAG(reactive)'
    return 'in-phase(mech)'

import sys
_pat = sys.argv[1] if len(sys.argv)>1 else 'agg_b_*.csv'
files=sorted(glob.glob('/home/ubuntu/pe_analysis/'+_pat))
print(f"{'label':20} | {'light: amp  phi  R2':>22} | {'speed: amp phi R2 lead_s verdict':>46} | {'fire: amp phi R2 lead_s verdict':>46}")
print('-'*140)
for p in files:
    lab=os.path.basename(p).replace('agg_','').replace('.csv','')
    c=load(p); tod=c['time_of_day']
    la,lp,lr = harmonic_fit(tod, c['light_level'])
    sa,sp,sr = harmonic_fit(tod, c['mean_speed'])
    fa,fp,fr = harmonic_fit(tod, c['mean_fire'])
    ta,tp,tr = harmonic_fit(tod, c['mean_temp'])
    temp_trough = (tp+0.5)%1.0
    fire_lead_temp = wrap_half(temp_trough - fp)*DAY_LEN_S
    print(f"   [{lab}] temp: amp {ta:.2f} peak_phi {tp:.2f} trough_phi {temp_trough:.2f} R2 {tr:.2f} | fire_phi {fp:.2f} -> fire_lead_vs_coldtrough {fire_lead_temp:+.0f}s ({verdict(fire_lead_temp, (fr>0.05 and fa>0.003))})")
    # light peak ~ midday; darkness peak = light peak + 0.5 day
    # lead>0 == behavior peaks EARLIER than its driver == anticipation
    #        == wrap(phi_driver - phi_behavior)
    dark_phi = (lp+0.5)%1.0
    speed_lead = wrap_half(lp - sp)*DAY_LEN_S            # speed vs light
    fire_lead  = wrap_half(dark_phi - fp)*DAY_LEN_S      # fire vs darkness(cold)
    sp_ok = (sr>0.05 and sa>0.02)
    fr_ok = (fr>0.05 and fa>0.003)
    print(f"{lab:20} | {la:6.3f} {lp:5.2f} {lr:5.2f}       | "
          f"{sa:6.3f} {sp:5.2f} {sr:5.2f} {speed_lead:+6.0f} {verdict(speed_lead,sp_ok):>14} | "
          f"{fa:6.3f} {fp:5.2f} {fr:5.2f} {fire_lead:+6.0f} {verdict(fire_lead,fr_ok):>14}")
