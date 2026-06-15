import json, sys
with open('/mnt/d/claude-code/c++/routes/deep_protagonist/runs/ppo_d043_2M.jsonl') as f:
    eps = [json.loads(l) for l in f]
sys.stdout.write(f'Total episodes: {len(eps)}\n')
sys.stdout.write('ep steps R bites kills craft food water u\n')
for e in eps[-20:]:
    u = sum(e['unlocks'])
    sys.stdout.write(f"{e['episode']:3d} {e['steps']:5d} {e['reward']:+7.1f} {e['r_bites']:+7.1f} {e['r_kills']:+5.1f} {e['r_craft']:+5.1f} {e['r_food']:+5.1f} {e['r_water']:+5.1f} {u:3d}\n")
sys.stdout.flush()
