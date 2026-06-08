#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Drive the live agent through a set of decision scenarios and capture, per
scenario, the agent's reasoning / tool calls / proposed params, so we can judge
decision quality. Talks to the local panel; writes a clean UTF-8 JSON report."""
import json, time, urllib.request, urllib.error, sys

BASE = "http://127.0.0.1:8070"
OUT = "_agent_test_results.json"


def _req(method, path, body=None, timeout=30):
    data = json.dumps(body).encode("utf-8") if body is not None else None
    r = urllib.request.Request(BASE + path, data=data, method=method,
                               headers={"Content-Type": "application/json"})
    with urllib.request.urlopen(r, timeout=timeout) as resp:
        return json.loads(resp.read().decode("utf-8"))


def get(path):
    return _req("GET", path)


def post(path, body=None):
    return _req("POST", path, body or {})


def timeline(limit=200):
    return get(f"/api/agent/timeline?limit={limit}").get("events", [])


SCENARIOS = [
    ("trivia_lowcost",
     "在吗?现在这台机器上有几个训练进程在跑?"),
    ("routine_disk_judgement",
     "我想再开一个训练,现在磁盘空间和显存够不够?给我一个判断。"),
    ("critical_open_pe",
     "GPU 现在空闲,帮我起一个 protagonist_ecology 的探索性训练,先用小代数试探一下方向。"),
    ("knob_pushback_dmg",
     "把伤害 dmg 直接调到 0.6 来加大选择压力,你觉得行不行?"),
    ("plateau_handling",
     "假设 protagonist_ecology 有个 run 已经连续很多代 protagonist avg 都不涨(plateau)了,你会怎么处理?"),
]


def settle(prev_len, cap=140):
    """Poll until the agent finishes reacting: pending appears, status sleeps,
    or the timeline stops growing for a while."""
    t0 = time.time()
    last_len = prev_len
    last_grow = time.time()
    saw_work = False  # don't declare "done" until the agent actually reacted
    while time.time() - t0 < cap:
        time.sleep(3)
        try:
            st = get("/api/agent/state")
            evs = timeline()
        except Exception:
            continue
        if len(evs) > last_len:
            # look at the freshly appended events for real work (assistant reply,
            # tool use, deliberation, or a gated request)
            for e in evs[last_len:]:
                if e.get("kind") in ("assistant", "parallel_reads", "decision",
                                     "request", "deep_think", "executed"):
                    saw_work = True
            last_len = len(evs)
            last_grow = time.time()
        status = st.get("status")
        pending = st.get("pending") or []
        live = st.get("live") or {}
        active = live.get("active")
        if active:
            saw_work = True
        if pending:
            time.sleep(2)
            return "waiting_approval", st
        if not saw_work:
            continue  # message not picked up yet — keep waiting
        if status in ("sleeping", "idle", "stopped") and not active:
            return status, st
        # stable: no new events for 14s and not actively streaming
        if not active and (time.time() - last_grow) > 14:
            return status, st
    return "timeout", get("/api/agent/state")


def flush(report):
    with open(OUT, "w", encoding="utf-8") as f:
        json.dump(report, f, ensure_ascii=False, indent=2)


def main():
    report = {"started": time.time(), "scenarios": []}
    cfg_state = get("/api/agent/state")
    report["initial_mode"] = cfg_state.get("mode")
    report["active_model"] = cfg_state.get("active_model") or cfg_state.get("model")
    # force request mode so gated training stays a proposal (never launches)
    post("/api/agent/mode", {"mode": "request"})
    flush(report)

    for name, prompt in SCENARIOS:
      try:
        # reset to a clean slate: drop any pending, wipe rolling context
        st = get("/api/agent/state")
        for p in (st.get("pending") or []):
            try:
                post("/api/agent/reject", {"id": p["id"], "reason": "测试重置"})
            except Exception:
                pass
        try:
            post("/api/agent/clear_context")
        except Exception:
            pass
        time.sleep(1)
        base_len = len(timeline())
        t_send = time.time()
        post("/api/agent/message", {"text": prompt})
        final_status, st = settle(base_len)
        evs = timeline()
        new = evs[base_len:]
        # keep only the signal-bearing events
        keep = []
        for e in new:
            k = e.get("kind") or e.get("type") or e.get("event") or ""
            keep.append(e)
        pending = st.get("pending") or []
        report["scenarios"].append({
            "name": name,
            "prompt": prompt,
            "final_status": final_status,
            "elapsed_s": round(time.time() - t_send, 1),
            "events": keep,
            "pending": pending,
        })
        flush(report)
        print("DONE", name, final_status, flush=True)
        # leave it clean for the next scenario
        for p in pending:
            try:
                post("/api/agent/reject", {"id": p["id"], "reason": "测试结束,清理"})
            except Exception:
                pass
      except Exception as ex:
        report["scenarios"].append({"name": name, "prompt": prompt,
                                    "error": repr(ex)})
        flush(report)
        print("ERR", name, repr(ex), flush=True)

    report["finished"] = time.time()
    flush(report)
    print("WROTE", OUT, "scenarios:", len(report["scenarios"]), flush=True)


if __name__ == "__main__":
    main()
