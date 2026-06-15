#!/usr/bin/env python3
"""In-place patch for training_panel/agent.py.

Two fixes:
  FIX 1 - stall guard: when the model produces reasoning but neither content nor
          a tool call (plan-without-execute), auto-nudge it to act in the SAME
          wake instead of sleeping (capped per wake). Plus a prompt rule.
  FIX 2 - read_log_tail output cap: clamp lines + per-line length + total bytes
          so per-tick-spammy logs can't blow up the LLM request (400/500).

Safe: every replacement asserts it found its anchor exactly once; aborts and
writes nothing if any anchor is missing. Makes a .bak and runs py_compile.
"""
import sys, os, py_compile, shutil

path = sys.argv[1] if len(sys.argv) > 1 else "agent.py"
src = open(path, encoding="utf-8").read()
orig = src

edits = []  # (label, old, new, expected_count)

# --- Edit A: module constant -------------------------------------------------
edits.append((
    "A:const",
    'KB_PATH = AGENT_DIR / "agent_knowledge.json"   # project knowledge network (persistent)',
    'KB_PATH = AGENT_DIR / "agent_knowledge.json"   # project knowledge network (persistent)\n'
    '_AUTOCONTINUE_MAX = 2   # max auto "execute now" nudges per wake when the model plans but emits no tool call',
    1))

# --- Edit B: AgentState field ------------------------------------------------
edits.append((
    "B:state_field",
    '        self._deliberated_this_wake: bool = False\n'
    '        self._last_deliberation: list[dict[str, Any]] = []\n',
    '        self._deliberated_this_wake: bool = False\n'
    '        self._last_deliberation: list[dict[str, Any]] = []\n'
    '        self._autocontinue: int = 0\n',
    1))

# --- Edit C1: _cap_log_lines helper (inserted before _pe_log_summary) --------
HELPER = '''def _cap_log_lines(lines, *, max_lines: int = 200, max_line_chars: int = 600,
                   max_total_bytes: int = 16000):
    """Cap a log tail so spammy per-tick logs can't blow up the LLM context.

    Keeps only the most recent lines that fit a byte budget and truncates any
    over-long line. Returns {"lines": [...], "truncated": bool, "note": str}.
    """
    lines = list(lines)[-max_lines:]
    capped: list[str] = []
    total = 0
    truncated = False
    for ln in reversed(lines):
        if len(ln) > max_line_chars:
            ln = ln[:max_line_chars] + "\\u2026(truncated)"
        b = len(ln.encode("utf-8", "ignore")) + 1
        if total + b > max_total_bytes and capped:
            truncated = True
            break
        capped.append(ln)
        total += b
    capped.reverse()
    out = {"lines": capped}
    if truncated or len(capped) < len(lines):
        out["truncated"] = True
        out["note"] = ("\\u65e5\\u5fd7\\u8fc7\\u5927\\u5df2\\u622a\\u65ad\\uff0c\\u4ec5\\u4fdd\\u7559\\u672b\\u5c3e\\u7247\\u6bb5\\u3002"
                       "\\u8981\\u770b\\u6307\\u6807\\u8d8b\\u52bf\\u8bf7\\u7528 analyze_metrics / compare_runs"
                       "\\uff08\\u53ea\\u89e3\\u6790 Gen \\u6c47\\u603b\\u884c\\uff0c\\u4f53\\u79ef\\u5c0f\\uff09\\uff0c"
                       "\\u4e0d\\u8981\\u9760 read_log_tail \\u62c9\\u539f\\u59cb\\u65e5\\u5fd7\\u3002")
    return out


'''
edits.append((
    "C1:helper",
    'def _pe_log_summary(path: Path, tail_lines: int = 4000) -> Optional[dict[str, Any]]:',
    HELPER + 'def _pe_log_summary(path: Path, tail_lines: int = 4000) -> Optional[dict[str, Any]]:',
    1))

# --- Edit C2: read_log_tail uses the cap -------------------------------------
edits.append((
    "C2:read_log_tail",
    '        if name == "read_log_tail":\n'
    '            n = int(args.get("n", 40))\n'
    '            rid = args.get("run_id")\n'
    '            if rid and rid in server.RUNS:\n'
    '                return {"lines": server.RUNS[rid].lines[-n:]}\n'
    '            proj = args.get("project", "pe")\n'
    '            fname = args.get("file", "")\n'
    '            if proj == "dp":\n'
    '                p = server._dp_resolve(fname)\n'
    '            else:\n'
    '                p = server.PE_PANEL_LOGS / fname\n'
    '                if not p.exists():\n'
    '                    p = server.PE_ROOT / fname\n'
    '            if not p.exists():\n'
    '                return {"error": f"log not found: {fname}"}\n'
    '            return {"lines": p.read_text(encoding="utf-8", errors="ignore").splitlines()[-n:]}\n',
    '        if name == "read_log_tail":\n'
    '            n = max(1, min(int(args.get("n", 40)), 200))\n'
    '            rid = args.get("run_id")\n'
    '            if rid and rid in server.RUNS:\n'
    '                return _cap_log_lines(server.RUNS[rid].lines[-n:])\n'
    '            proj = args.get("project", "pe")\n'
    '            fname = args.get("file", "")\n'
    '            if proj == "dp":\n'
    '                p = server._dp_resolve(fname)\n'
    '            else:\n'
    '                p = server.PE_PANEL_LOGS / fname\n'
    '                if not p.exists():\n'
    '                    p = server.PE_ROOT / fname\n'
    '            if not p.exists():\n'
    '                return {"error": f"log not found: {fname}"}\n'
    '            return _cap_log_lines(p.read_text(encoding="utf-8", errors="ignore").splitlines()[-n:])\n',
    1))

# --- Edit D: stall guard in the no-tool-call branch --------------------------
edits.append((
    "D:stall_guard",
    '    if not tool_calls:\n'
    '        # No action proposed -> sleep to conserve credits. When the box is idle\n',
    '    if not tool_calls:\n'
    '        # STALL GUARD: model produced reasoning but neither spoke (empty\n'
    '        # content) nor acted (no tool call) -> it planned without executing.\n'
    '        # Auto-nudge it to turn the plan into tool calls in the SAME wake\n'
    '        # instead of sleeping, capped per wake so a truly idle turn still\n'
    '        # falls through to sleep.\n'
    '        if (not content) and reasoning and STATE._autocontinue < _AUTOCONTINUE_MAX:\n'
    '            STATE._autocontinue += 1\n'
    '            STATE.messages.append({"role": "user", "content": (\n'
    '                "[\\u7ee7\\u7eed] \\u4f60\\u4e0a\\u4e00\\u6b65\\u53ea\\u5728\\u601d\\u8003\\u91cc\\u89c4\\u5212\\u4e86\\uff0c\\u4f46\\u6ca1\\u6709\\u53d1\\u51fa\\u4efb\\u4f55\\u5de5\\u5177\\u8c03\\u7528\\u3002"\n'
    '                "\\u5982\\u679c\\u8ba1\\u5212\\u9700\\u8981\\u6267\\u884c\\uff0c\\u73b0\\u5728\\u5c31\\u76f4\\u63a5\\u53d1\\u51fa\\u5bf9\\u5e94\\u7684\\u5de5\\u5177\\u8c03\\u7528"\n'
    '                "\\uff08\\u5982 use_strategy / make_config_variant / start_pe_training / "\n'
    '                "compare_runs \\u7b49\\uff09\\uff0c\\u628a\\u5206\\u6790\\u2192\\u51b3\\u7b56\\u2192\\u6267\\u884c\\u5728\\u8fd9\\u4e00\\u6b21\\u5524\\u9192\\u5185\\u4e00\\u53e3\\u6c14\\u505a\\u5b8c\\uff1b"\n'
    '                "\\u82e5\\u786e\\u5b9e\\u65e0\\u4e8b\\u53ef\\u505a\\uff0c\\u518d\\u663e\\u5f0f\\u8c03\\u7528 sleep \\u5de5\\u5177\\u3002")})\n'
    '            _timeline("autocontinue", n=STATE._autocontinue,\n'
    '                      reason="\\u89c4\\u5212\\u672a\\u843d\\u5730\\u4e3a\\u5de5\\u5177\\u8c03\\u7528\\uff0c\\u81ea\\u52a8\\u50ac\\u4fc3\\u6267\\u884c")\n'
    '            if STATE.running:\n'
    '                STATE.status = "active"\n'
    '            return\n'
    '        # No action proposed -> sleep to conserve credits. When the box is idle\n',
    1))

# --- Edit E: reset counter on wake -------------------------------------------
edits.append((
    "E:reset_on_wake",
    '                STATE._recorded_since_wake = False\n'
    '                STATE._deliberated_this_wake = False\n'
    '                STATE._last_deliberation = []\n',
    '                STATE._recorded_since_wake = False\n'
    '                STATE._deliberated_this_wake = False\n'
    '                STATE._last_deliberation = []\n'
    '                STATE._autocontinue = 0\n',
    1))

# --- Edit F: reset counter once the model actually acts ----------------------
edits.append((
    "F:reset_on_act",
    '    gated = set(cfg.get("gated_tools", []))\n'
    '    n = len(tool_calls)\n',
    '    STATE._autocontinue = 0  # model acted -> reset stall counter\n'
    '    gated = set(cfg.get("gated_tools", []))\n'
    '    n = len(tool_calls)\n',
    1))

# --- Edit G: prompt rule (one-shot execution) --------------------------------
edits.append((
    "G:prompt_rule",
    '- \u51b3\u5b9a\u5f00\u8bad\u524d\u60f3\u6e05\u53c2\u6570\u548c\u7406\u7531\u3002\u5f00\u5b8c\u8bad\u5c31 sleep,\u628a\u5524\u9192\u4ea4\u7ed9 watcher',
    '- \u3010\u89c4\u5212\u5fc5\u987b\u5f53\u56de\u5408\u843d\u5730\u4e3a\u5de5\u5177\u8c03\u7528\u3011\u60f3\u6e05\u8981\u505a\u4ec0\u4e48\u540e,'
    '\u5c31\u5728\u540c\u4e00\u6b21\u5524\u9192\u91cc\u76f4\u63a5\u53d1\u51fa\u5bf9\u5e94\u5de5\u5177\u8c03\u7528'
    '(use_strategy\u2192inspect_config\u2192make_config_variant\u2192start_pe_training \u4e00\u6c14\u5450\u6210),'
    '\u522b\u53ea\u5728\u601d\u8003\u91cc\u89c4\u5212\u5b8c\u5374\u4e0d\u53d1\u4efb\u4f55\u5de5\u5177\u8c03\u7528\u2014\u2014'
    '\u90a3\u6837\u8fd9\u4e00\u8f6e\u7b49\u4e8e\u7a7a\u8f6c\u3002\u786e\u5b9e\u65e0\u4e8b\u53ef\u505a\u65f6\u624d\u663e\u5f0f\u8c03\u7528 sleep\u3002\n'
    '- \u51b3\u5b9a\u5f00\u8bad\u524d\u60f3\u6e05\u53c2\u6570\u548c\u7406\u7531\u3002\u5f00\u5b8c\u8bad\u5c31 sleep,\u628a\u5524\u9192\u4ea4\u7ed9 watcher',
    1))

# Apply
for label, old, new, exp in edits:
    cnt = src.count(old)
    if cnt != exp:
        print(f"ABORT [{label}]: anchor count={cnt}, expected {exp}. No changes written.")
        sys.exit(3)
    src = src.replace(old, new, 1)

if src == orig:
    print("ABORT: no changes produced.")
    sys.exit(4)

bak = path + ".bak"
shutil.copyfile(path, bak)
open(path, "w", encoding="utf-8").write(src)
try:
    py_compile.compile(path, doraise=True)
except py_compile.PyCompileError as e:
    shutil.copyfile(bak, path)  # rollback
    print("ABORT: py_compile failed, rolled back:\n", e)
    sys.exit(5)
print("PATCH OK: applied", len(edits), "edits; backup at", bak)
print("autocontinue_max present:", "_AUTOCONTINUE_MAX = 2" in src)
print("cap helper present:", "_cap_log_lines(" in src)
