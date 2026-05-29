"""Quick inspector for one agent_trace.csv file."""
import sys
import pandas as pd

path = sys.argv[1]
df = pd.read_csv(path)
print(f"total rows: {len(df)}")
print(f"unique agent_ids: {df.agent_id.nunique()}")
print(f"species_id values: {sorted(df.species_id.unique())}")
print(f"agent_id range: {df.agent_id.min()} .. {df.agent_id.max()}")
print()
print("rows per agent (sample first 10):")
print(df.groupby("agent_id").size().head(10))
print()
print("agents with non-'na' goals:")
g = df[df.goal_name != "na"].groupby("agent_id").goal_name.unique()
print(f"count: {len(g)}")
print(g.head(15))
print()
print("goal_name distribution across all rows:")
print(df.goal_name.value_counts().head(15))
print()
print("agent_id histogram by species:")
print(df.groupby("species_id").agent_id.nunique())
