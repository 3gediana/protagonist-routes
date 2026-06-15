# -*- coding: utf-8 -*-
"""Lightweight verification of the two agent.py fixes.

Runs in an ISOLATED process (its own fresh AgentState singleton); it does NOT
touch the live panel. It stubs _chat (so NO LLM/mimo cost) and _timeline (so the
real timeline file is not polluted), and stubs the heavy observation gather.

FIX 1 (stall guard): a turn with reasoning but no content and no tool_calls must
  auto-nudge (append a continue message) and stay 'active' instead of sleeping,
  capped at _AUTOCONTINUE_MAX per wake; once capped it must sleep.
FIX 2 (read_log_tail cap): a huge real log fed through _cap_log_lines must come
  back within the byte budget, with a truncated flag/note.
"""
import sys, glob, os, json

import agent  # from training_panel dir, panel venv

PASS = []
FAIL = []
def check(label, cond, extra=""):
    (PASS if cond else FAIL).append(label + ((" :: " + extra) if extra else ""))
    print(("PASS " if cond else "FAIL ") + label + ((" :: " + extra) if extra else ""))

# ---------- FIX 2: read_log_tail cap ----------------------------------------
# find the biggest log on the box
cands = []
for root in [getattr(agent, "AGENT_DIR", None)]:
    pass
search_dirs = []
try:
    import server
    for attr in ("PE_PANEL_LOGS", "PE_ROOT"):
        d = getattr(server, attr, None)
        if d:
            search_dirs.append(str(d))
except Exception as e:
    print("note: server import for log search:", e)
# also scan protagonist_ecology logs/runs
search_dirs += [r"D:\claude-code\c++\routes\protagonist_ecology\logs",
                r"D:\claude-code\c++\routes\protagonist_ecology\runs"]
for d in search_dirs:
    if d and os.path.isdir(d):
        for ext in ("*.log", "*.txt"):
            cands += glob.glob(os.path.join(d, "**", ext), recursive=True)
cands = [c for c in cands if os.path.isfile(c)]
big = max(cands, key=os.path.getsize) if cands else None
if big:
    raw = open(big, encoding="utf-8", errors="ignore").read().splitlines()
    rawbytes = sum(len(l.encode("utf-8", "ignore")) + 1 for l in raw)
    out = agent._cap_log_lines(raw)
    outbytes = sum(len(l.encode("utf-8", "ignore")) + 1 for l in out["lines"])
    print(f"  biggest log: {big}")
    print(f"  raw: {len(raw)} lines / {rawbytes} bytes  ->  capped: {len(out['lines'])} lines / {outbytes} bytes")
    check("FIX2 output within 16KB budget", outbytes <= 16001, f"{outbytes} bytes")
    check("FIX2 truncated flag+note present when source big",
          (rawbytes <= 16000) or (out.get("truncated") and out.get("note")),
          f"raw={rawbytes}")
    # per-line clamp
    check("FIX2 no line exceeds clamp", all(len(l) <= 600 + 20 for l in out["lines"]))
else:
    check("FIX2 found a log to test", False, "no logs found")

# ---------- FIX 1: stall guard ----------------------------------------------
events = []
agent._timeline = lambda kind, **kw: events.append((kind, kw))
agent._chat = lambda cfg, msgs: {"content": "", "reasoning_content":
                                 "我先分析指标，再决定建哪个变体……（只想没动手）",
                                 "tool_calls": []}
# keep it light + offline
agent._gather_observation = lambda: {"_test": True}
agent._microcompact_obs = lambda keep_last=1: None
try:
    agent._maybe_compress = lambda cfg: None
except Exception:
    pass

cfg = agent._load_config()
S = agent.STATE
S.running = True
S.status = "active"
S._autocontinue = 0
S.messages = []
S.user_inbox = []

print("AUTOCONTINUE_MAX =", getattr(agent, "_AUTOCONTINUE_MAX", "MISSING"))

# turn 1
agent._do_step(cfg)
n1, st1 = S._autocontinue, S.status
msg1 = S.messages[-1] if S.messages else {}
check("FIX1 turn1 auto-nudged (counter=1)", n1 == 1, f"counter={n1}")
check("FIX1 turn1 stayed active (not sleeping)", st1 == "active", f"status={st1}")
check("FIX1 turn1 appended a continue nudge msg",
      msg1.get("role") == "user" and "继续" in (msg1.get("content") or ""))
check("FIX1 turn1 timeline has 'autocontinue'", any(e[0] == "autocontinue" for e in events))

# turn 2
agent._do_step(cfg)
n2, st2 = S._autocontinue, S.status
check("FIX1 turn2 nudged again (counter=2)", n2 == 2, f"counter={n2}")
check("FIX1 turn2 still active", st2 == "active", f"status={st2}")

# turn 3 -> cap reached, must sleep instead of nudging
events.clear()
agent._do_step(cfg)
n3, st3 = S._autocontinue, S.status
check("FIX1 turn3 cap held (counter stays 2, no 3rd nudge)", n3 == 2, f"counter={n3}")
check("FIX1 turn3 fell through to sleep", st3 == "sleeping", f"status={st3}")
check("FIX1 turn3 no new autocontinue event", not any(e[0] == "autocontinue" for e in events))

# ---------- summary ----------------------------------------------------------
print("\n===== SUMMARY =====")
print(f"PASS={len(PASS)}  FAIL={len(FAIL)}")
if FAIL:
    print("FAILURES:")
    for f in FAIL:
        print("  -", f)
    sys.exit(1)
print("ALL CHECKS PASSED")
