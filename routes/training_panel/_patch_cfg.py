import json, pathlib, shutil, time
p = pathlib.Path("agent_config.json")
shutil.copy(p, p.with_suffix(".json.bak_tierc_%d" % int(time.time())))
cfg = json.loads(p.read_text(encoding="utf-8"))
tc = cfg.setdefault("tier_c", {})
defs = {"auto_approve": True, "reward_pct_hard_cap": 3.0,
        "dp_steps_hard_cap": 12000000, "pe_gens_hard_cap": 120}
for k, v in defs.items():
    tc.setdefault(k, v)
p.write_text(json.dumps(cfg, ensure_ascii=False, indent=2), encoding="utf-8")
print("tier_c now:", json.dumps(cfg["tier_c"], ensure_ascii=False))
