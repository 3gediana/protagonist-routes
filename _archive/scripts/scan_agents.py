import sys, pandas as pd
from collections import Counter
csv = sys.argv[1]
df = pd.read_csv(csv)
df = df.sort_values(["agent_id","step_index"])
brained = set(df.loc[df["goal_name"].astype(str)!="na","agent_id"].unique())
rows=[]
for aid,a in df.groupby("agent_id"):
    if aid not in brained: continue
    n=len(a); g=Counter(a["goal_name"].astype(str))
    dom,domc=g.most_common(1)[0]
    rows.append((aid,n,int(a["episodic_event_count"].iloc[-1]),
                 float(a["food_eaten"].iloc[-1]),int(a["kills"].iloc[-1]),
                 round(float(a["fitness"].iloc[-1]),0),
                 dom,round(domc/n,2),
                 round(g.get("emit_signal",0)/n,2),
                 round(g.get("respond_signal",0)/n,2),
                 round(g.get("explore",0)/n,2),
                 round(g.get("seek_food",0)/n + g.get("forage",0)/n,2),
                 int(a["predator_nearby_count"].max())))
rows.sort(key=lambda r:(-r[8],-r[7],-r[3]))  # emit_signal share, dom frac, food
print("brained agents:",len(rows))
print("aid n ev food kills fit | dom domfrac emit resp explore seekfood predmax")
for r in rows[:14]:
    print(r)
