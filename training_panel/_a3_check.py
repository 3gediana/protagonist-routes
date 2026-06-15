import sys, traceback
try:
    import server, agent
    print("IMPORT_OK")
    print("has_trigger:", hasattr(server, "dp_counterfactual_probe"))
    print("has_analyzer:", hasattr(agent, "_analyze_dp_counterfactual"))
    # confirm tool registered in schema + dispatch maps
    names = []
    try:
        for t in agent.tools_schema() if hasattr(agent, "tools_schema") else []:
            pass
    except Exception:
        pass
    print("trigger_in_meta:", "dp_counterfactual_probe" in getattr(agent, "TOOL_META", {}))
    print("analyzer_in_meta:", "analyze_dp_counterfactual" in getattr(agent, "TOOL_META", {}))
    print("trigger_in_readtools:", "analyze_dp_counterfactual" in getattr(agent, "READ_TOOLS", set()))
except Exception:
    traceback.print_exc()
    sys.exit(2)
