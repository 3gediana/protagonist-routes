import json, agent
cfg = agent._load_config()
try:
    agent.STATE.focus_project = "dp"
except Exception:
    pass
res = agent._execute_tool("dp_counterfactual_probe", {
    "checkpoint": "checkpoints/ft_hyp_entropy_bg.pt",
    "episodes": 4,
    "groups": ["spatial", "water", "threat"],
    "reason": "A3 end-to-end smoke: verify agent can trigger DP perception ablation",
}, cfg)
print("TRIGGER_RESULT:")
print(json.dumps(res, ensure_ascii=False, indent=2))
