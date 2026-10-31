import agent
def cfg(mode, auto=True):
    c = dict(agent.DEFAULT_CONFIG); c["mode"]=mode
    c["tier_c"]=dict(agent.DEFAULT_CONFIG["tier_c"]); c["tier_c"]["auto_approve"]=auto
    return c
A=cfg("auto"); R=cfg("request"); AOFF=cfg("auto",auto=False)
print("=== _needs_approval ===")
print("auto/curriculum :", agent._needs_approval("pe_curriculum_edit",{"config":"x","edits":{}},A))
print("auto/reward sflip:", agent._needs_approval("pe_reward_tune",{"config":"onepot.toml","edits":{"reward.food_weight":-5}},A))
print("auto/new dp blood:", agent._needs_approval("start_dp_training",{"steps":8000000},A))
print("REQ /curriculum  :", agent._needs_approval("pe_curriculum_edit",{"config":"x","edits":{}},R))
print("auto_OFF/curric  :", agent._needs_approval("pe_curriculum_edit",{"config":"x","edits":{}},AOFF))
print("=== _hard_cap_violation (training length) ===")
print("dp steps 20M     :", agent._hard_cap_violation("start_dp_training",{"steps":20000000},A))
print("dp steps 5M      :", agent._hard_cap_violation("start_dp_training",{"steps":5000000},A))
print("pe gens 200      :", agent._hard_cap_violation("start_pe_training",{"generations":200},A))
print("pe gens 50       :", agent._hard_cap_violation("start_pe_training",{"generations":50},A))
