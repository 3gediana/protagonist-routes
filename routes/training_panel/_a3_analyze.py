import json, agent
cfg = agent._load_config()
agent._execute_tool("focus_project", {"project": "dp"}, cfg)
res = agent._execute_tool("analyze_dp_counterfactual",
                          {"file": "runs/dp_cf_cf_20260609-114144"}, cfg)
print(json.dumps(res, ensure_ascii=False, indent=2))
