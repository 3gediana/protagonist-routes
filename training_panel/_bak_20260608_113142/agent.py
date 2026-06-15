#!/usr/bin/env python3
"""agent.py — LLM training orchestrator for the DP/PE training panel.

This module bolts an autonomous, LLM-driven training manager onto the existing
`server.py` panel. It does NOT replace any panel feature; it reuses the panel's
endpoints as its "tools" (launch / stop / status / cleanup / metrics).

Design goals (from the user):
  * The brain is a REAL LLM reached over an OpenAI-compatible API. The user can
    register/select multiple providers (name / base_url / api_key / model) in
    the panel. Keys live in a local gitignored config, never in git.
  * Two control modes:
      - "request": every TRAINING-START is proposed as a request card (params +
        reasoning); a human must approve before it runs.
      - "auto": full authority, auto-approve — but every decision's params +
        reasoning are still recorded to history.
  * Long-horizon / overnight: a `sleep` tool lets the agent doze while training
    runs, so it does not burn API credits. A cheap watcher wakes it on a timer
    or on key events (run exited, error, extinction, disk low).
  * Hard guards (enforced in code, not left to the LLM):
      - at most `max_procs` (default 10) training processes; refuse + feed the
        reason back to the model when exceeded;
      - disk-low -> auto cleanup of logs/traces (NEVER *.pt checkpoints) and
        refuse to launch new training;
      - red lines from AUTONOMOUS_PLAYBOOK live in the system prompt.

The module exposes `register_agent(app)` which the server calls to attach the
`/api/agent/*` routes. It imports `server` lazily (inside functions) to avoid a
circular import at module load.
"""
from __future__ import annotations

import json
import os
import re
import threading
import time
import urllib.error
import urllib.request
from collections import deque
from datetime import datetime
from pathlib import Path
from typing import Any, Optional

from fastapi import HTTPException
from pydantic import BaseModel

AGENT_DIR = Path(__file__).resolve().parent
CONFIG_PATH = AGENT_DIR / "agent_config.json"
TIMELINE_PATH = AGENT_DIR / "agent_timeline.jsonl"
DOCS_DIR = AGENT_DIR / "docs"          # strategy/handoff KB, retrieved on demand
KB_PATH = AGENT_DIR / "agent_knowledge.json"   # project knowledge network (persistent)

# --------------------------------------------------------------------------- #
# Strategy digest — distilled from AUTONOMOUS_PLAYBOOK.md + PITFALLS.md.
# Injected verbatim into the system prompt so the LLM inherits the red lines and
# the decision tree. Kept short and high-signal to save tokens.
# --------------------------------------------------------------------------- #
STRATEGY_DIGEST = """\
# 角色
你是 deep_protagonist(DP, 单体生存 RL) 与 protagonist_ecology(PE, 种群 MAP-Elites 进化) 两套训练的自动管家,
运行在用户的 Windows 训练机(单张 RTX5060 8G, 32G 内存)。你通过工具操作训练,平稳时休眠省额度。

# 铁律 / 红线 (违反 = 严重事故)
1. 不改训练核心逻辑、不动 routes/deep_protagonist/ 源码、不改数据流/输入维度。你只调超参数和 TOML 旋钮、管生命周期。
2. 绝不为刷某指标加直接奖励 (Goodhart)。指标长期低 -> 怀疑 bug/算法,给诊断建议,绝不调小参数刷分。
3. 检查点清理:受保护清单(ft_gen/bc_gen/bc_hv2、ppo_d061/065/076_*、r1_cook_s24、任何 *_red/_final/_champ/_keep/_gold/*.bak)永不可删(代码硬拦,你删不掉)。其余 .pt 你可按重要性自行决策删除——用 delete_checkpoint(走回收站,可恢复)。日志/trace 仍用 cleanup_disk。
4. 单卡 8G: 最多同时 10 个训练进程。DP 是大头(50MB 模型/两三天),优先级高于 PE;GPU 被 DP 占满时不与它抢,等空闲或做不占 GPU 的事。
5. 机制级代码改动 (改 C++ + 重新编译) 不由你自动做 —— 你负责发现问题、在聊天里给代码级建议,执行交给人。

# 决策树 (出问题怎么办,预先决策好,别干等)
- 全灭(alive=0): 压力档回退一档(经验 dmg=0.25 是 Goldilocks, 0.5 太狠),重跑 smoke。
- harmony 破: C7 灭绝=压力太狠回退; C6 多样性低=多半没收敛,跑满代再判; C1-C5 行为破=回退+根因诊断。
- 改输入维后旧 checkpoint 不兼容: 正常,起新血脉从 gen0 跑,不接力。
- trace 堆积/磁盘满: 跑完立刻清 trace,只留 .md/.json 小结。
- GPU 被占: 等空闲,不抢。

# 阶段流程
smoke(2-4代) -> short(10-20代) -> long(60-80代,≤8h) -> harmony 7/7 不破 -> 收尾(记录+清 trace)。
每个训练启动都必须写清"参数 + 为什么这么训"(reason 字段),无论哪种模式都要留历史。

# 策略目录 (你自己判断当前该用哪条,用 use_strategy 工具切换;切换后系统会喂给你这条策略的【详细打法】——该用的工具/关注的参数/要避的坑)
- fast_iter 快速迭代: 小步探路+并行多组+预测面板判好坏,留赢家杀输家。摸索新参数/不确定方向时用。
- ab_knob 对照换旋钮: 一次只改一个旋钮做对照,把因果归到单一改动。验证某个旋钮影响时用。
- hist_bench 历史对标: 决策前先 query_gens 对标历史最好代,不超它/不验假设就不开。决定要不要开新跑时用。
- early_stop 早停回退: 发散/全灭/超时立即停+回退档位。run 变坏时用。
- ckpt_hygiene 检查点卫生: 清劣代/冗余 .pt(受保护硬拦),保磁盘。磁盘紧张时用。
- compute_yield 算力让路: DP 优先,GPU 被占就做不吃 GPU 的事(分析/记台账/沉淀)。资源紧张时用。
决策一开始就该选一条最贴合当前处境的策略(use_strategy),需要时再切换;不确定就先用 fast_iter。

# 关键指标读法
- PE 日志行: `Gen N | protagonist avg=.. best=.. | ...各 archetype.. | builds=.. chops=.. hunts=.. | me_cov=X/64 qd=..`
  me_cov=填充的行为格子数(多样性), qd=质量多样性总分。看 avg/best 趋势、me_cov 是否增长、是否 extinct。
- DP jsonl: 每行一局, 看 score、unlocks(里程碑)、prey_kills(真击杀,不是 reward 里的 kills=)、deaths_* 死因。

# 工作风格
- 简洁。先看 get_status 再决策。能一次想清就别频繁调用模型。简单问答(如"在吗""现在几个进程")直接回答,别调工具、别深想。
- 面临【复杂决策】(开训/换参/回退/多因素权衡)时,主动调 deep_think 工具做多轮内部推演再定;每轮唤醒最多调一次,简单事别用,省额度。
- 决定开训前想清参数和理由。开完训就 sleep,把唤醒交给 watcher(到点/run 结束/报错/灭绝/磁盘告警才醒)。
- 拿不准、或要动机制代码/换基线这种大动作,用 log_note 写建议,别擅自做。
- 历史与大块资料【不预载进上下文】:要回顾自己过去的决策/执行,用 recall_timeline;要看训练策略与交接资料(playbook/handoff/知识笔记),用 list_docs 再 read_doc 按需取。需要时才拉,保持上下文精简省额度。

# 你是这个项目的【活知识库 / 状态存查点】
你不只是开训的操作工,更是整个项目状态的持有者和查询入口,要主动维护一张持久的知识网络(跨会话不丢):
- kb_write/kb_query:沉淀并检索知识节点——蓝图目标、红线、踩过的坑、机制结论、各血脉状态;节点间用 links 关联成网。**决策前先 kb_query 查既往的坑和结论**,别重复踩坑。有新发现(尤其是坑/负结果)就 kb_write 记下,并 link 到相关节点。
- record_gen/query_gens:把每代/每血脉的模型表现(checkpoint+指标)记进台账;想知道"哪代最好、趋势如何"就 query_gens。run 出现里程碑或结束时应 record_gen。
- 已预置一批种子知识(蓝图/红线/已知坑/受保护检查点),开局先 kb_query 一遍熟悉项目,再补充你的新理解。
"""

# ------------------------------------------------------------------------- #
# Selectable strategies. The model switches between these with `use_strategy`.
# Only the compact catalog lives in the system prompt; the ACTIVE strategy's
# full playbook (`guide`) is injected into the system prompt while it's active,
# so guidance changes as the agent switches strategies — and is never lost to
# compression. Each guide names the tools to use, params to watch, pitfalls.
# ------------------------------------------------------------------------- #
STRATEGIES: dict[str, dict[str, str]] = {
    "fast_iter": {
        "name": "快速迭代",
        "when": "摸索新参数、方向不确定时。",
        "guide": (
            "打法:① 先用最小代数/步数起 smoke 探路(start_pe_training generations=2-4 / start_dp_training steps 设小);"
            "② 在进程上限(max_procs)内【并行】起多组不同参数(A/B/C 各换一个方向),不要一组跑到底;"
            "③ 每组刚出几代/几集就用 analyze_metrics(project,file,window=最近若干点)判趋势——improving 的留, plateau/declining 的立刻 stop_training 杀掉,把算力让给赢家;"
            "④ 用赢家的参数+结论开下一轮更长的跑,并 record_gen 记台账。"
            "关注:每组 reason 写清在试什么假设;并行别超 max_procs;赢家选出后及时停掉对照组省算力。"
            "坑:不要并行太多把显存/磁盘挤爆;不要凭一两点就下结论(window 太小=too_short 再等等)。"),
    },
    "ab_knob": {
        "name": "对照换旋钮",
        "when": "想验证某一个旋钮(dmg/cold_thr/树密度/sigma/archetype 权重)的影响时。",
        "guide": (
            "打法:① 选定基线(上一代或某 checkpoint);② 只改【一个】旋钮起一个对照 run,其余参数与基线完全一致(start_* 时 reason 写明改了哪个旋钮、从X→Y);"
            "③ 跑到可比阶段后 analyze_metrics 对比对照组与基线的同名指标,把差异归因到这一个改动;④ kb_write 记下这个旋钮的因果结论并 link。"
            "关注:务必一次只动一个变量;seed 尽量与基线一致以可比。"
            "坑:同时改多个旋钮会搅浑因果——禁止;经验值 dmg=0.25 是 Goldilocks,0.5 太狠易全灭。"),
    },
    "hist_bench": {
        "name": "历史对标",
        "when": "决定要不要开一个新跑、值不值得花算力时。",
        "guide": (
            "打法:① 先 query_gens(project,lineage) 看该血脉历史各代表现与最好代;② 明确新跑的目标——要么指标超过历史最好代,要么验证一个写得出来的假设;"
            "③ 两者都不满足就【不开】(用 log_note 说明为何不值得),省算力省额度;④ 要开就把对标基准写进 reason。"
            "关注:对标 avg/best/me_cov/qd(PE)或 score/unlocks(DP)。坑:别重复跑已知结论的实验;别为了跑而跑。"),
    },
    "early_stop": {
        "name": "早停回退",
        "when": "run 出现发散/全灭(alive=0)/超时/指标持续下行时。",
        "guide": (
            "打法:① analyze_metrics 确认是 declining 还是噪声;② 确认变坏就立刻 stop_training(检查点自动保存,安全);"
            "③ 按决策树回退一档(全灭→压力档回退,如 dmg 0.5→0.25;harmony C7 灭绝→回退);④ kb_write 记下这个失败配置(负结果也是知识),再用回退后的参数重起。"
            "关注:超时(long run >8h)也要停。坑:不要干等空耗算力;回退是换参重起,不是接力坏 checkpoint。"),
    },
    "ckpt_hygiene": {
        "name": "检查点卫生",
        "when": "磁盘吃紧(接近 disk_low_gb)或劣代检查点堆积时。",
        "guide": (
            "打法:① list_checkpoints 看清都有哪些 .pt 及其对应代/表现(可配合 query_gens);② 先 cleanup_disk 清日志/trace(不动 .pt);"
            "③ 仍紧张再 delete_checkpoint 删【劣代/被超越/冗余】的 .pt(受保护清单会被硬拦跳过,走回收站可恢复),reason 写清为何不重要;④ 始终保磁盘 > disk_low_gb。"
            "关注:删前务必确认不是受保护或最佳代。坑:绝不删 ft_gen/bc_*/ppo_d06*/r1_cook_s24/*_red/_final/_champ/_keep/_gold/*.bak(代码也会硬拦)。"),
    },
    "compute_yield": {
        "name": "算力让路",
        "when": "GPU 被 DP 大任务占满、资源紧张时。",
        "guide": (
            "打法:① get_status 看 GPU 占用与进程;② DP(大模型/两三天)优先级高于 PE,GPU 满时【不抢】、不新开吃 GPU 的训练;"
            "③ 转去做不吃 GPU 的活:analyze_metrics 分析在跑的 run、record_gen 记台账、kb_write 沉淀结论、读日志诊断、query_gens 复盘;④ 等 GPU 空闲再开训。"
            "关注:单卡 8G,别和 DP 抢显存。坑:并发≤max_procs 仍要看显存余量,不是有空位就开。"),
    },
}


def _strategy_block(cfg: dict[str, Any]) -> str:
    sid = cfg.get("active_strategy")
    s = STRATEGIES.get(sid or "")
    if not s:
        return ""
    return (f"\n# 当前策略: {s['name']} ({sid})\n适用: {s['when']}\n打法: {s['guide']}\n"
            "(随处境变化可用 use_strategy 切换到更合适的策略。)\n")


DEFAULT_CONFIG: dict[str, Any] = {
    "providers": [],            # [{id,name,base_url,api_key,model}]
    "active_provider": None,
    "mode": "request",          # "request" | "auto"
    "active_strategy": None,    # current strategy id (model switches via use_strategy)
    "max_procs": 10,            # hard cap on concurrent training processes
    "disk_low_gb": 190.0,       # below this -> auto cleanup + refuse launches
    "poll_interval_s": 20,      # watcher poll cadence while sleeping
    "default_sleep_min": 20,    # fallback sleep if model forgets to set one
    "max_sleep_min": 240,       # cap a single sleep
    "gated_tools": ["start_pe_training", "start_dp_training", "stop_training",
                     "delete_checkpoint"],
    "temperature": 0.3,
    "max_tokens": 2000,        # mimo-v2.5 is a reasoning model: needs headroom
                               # for reasoning_content before it emits output
    "objective": "维持 PE/DP 训练健康推进:平稳时休眠,出问题按决策树处理,防止磁盘/资源爆掉。",
    # --- context management ---
    "context_token_budget": 600000,   # keep the request under this (model is 1M)
    "keep_recent_msgs": 14,           # always keep this many newest msgs verbatim
    "pinned_specs": [],               # agent-extracted specs; NEVER compressed
    "rolling_summary": "",            # rolling summary of compressed-out history
    # --- deep thinking ---
    # multi-round guided deliberation BEFORE deciding. 0 = off (use the model's
    # own single-call reasoning only). >0 = spend that many extra API calls, each
    # with a different guiding prompt; the rounds are ephemeral (not kept in the
    # rolling context, only logged to the timeline).
    "deep_think_rounds": 0,
}


def _load_config() -> dict[str, Any]:
    cfg = dict(DEFAULT_CONFIG)
    if CONFIG_PATH.exists():
        try:
            cfg.update(json.loads(CONFIG_PATH.read_text(encoding="utf-8")))
        except Exception:  # noqa: BLE001
            pass
    # ensure all default keys present
    for k, v in DEFAULT_CONFIG.items():
        cfg.setdefault(k, v)
    return cfg


def _save_config(cfg: dict[str, Any]) -> None:
    CONFIG_PATH.write_text(json.dumps(cfg, ensure_ascii=False, indent=2), encoding="utf-8")


def _redact_providers(cfg: dict[str, Any]) -> list[dict[str, Any]]:
    """Provider list with api_key masked for the UI."""
    out = []
    for p in cfg.get("providers", []):
        k = p.get("api_key") or ""
        masked = (k[:6] + "…" + k[-4:]) if len(k) > 12 else ("set" if k else "")
        out.append({"id": p.get("id"), "name": p.get("name"),
                    "base_url": p.get("base_url"), "model": p.get("model"),
                    "api_key_masked": masked, "has_key": bool(k)})
    return out


def _active_provider(cfg: dict[str, Any]) -> Optional[dict[str, Any]]:
    pid = cfg.get("active_provider")
    for p in cfg.get("providers", []):
        if p.get("id") == pid:
            return p
    return None


# --------------------------------------------------------------------------- #
# Agent state (in-memory singleton).
# --------------------------------------------------------------------------- #
class AgentState:
    def __init__(self) -> None:
        self.lock = threading.RLock()
        self.status = "stopped"          # stopped|active|sleeping|waiting_approval|error
        self.messages: list[dict[str, Any]] = []   # chat history (after system)
        self.pending: list[dict[str, Any]] = []     # approval queue
        self.user_inbox: list[str] = []             # queued user chat messages
        self.sleep_until = 0.0
        self.sleep_reason = ""
        self.wake_event = threading.Event()
        self.last_error = ""
        self.thread: Optional[threading.Thread] = None
        self.running = False
        self.stats = {"llm_calls": 0, "actions": 0, "wakes": 0,
                      "prompt_tokens": 0, "completion_tokens": 0}
        self.last_obs_snapshot: dict[str, Any] = {}
        self._watch_baseline: dict[str, Any] = {}
        # live streaming buffer (for the web UI to show thinking + reply as it streams)
        self.live: dict[str, Any] = {"active": False, "phase": "", "reasoning": "",
                                     "content": "", "seq": 0, "deliberation": []}
        self._deliberated_this_wake: bool = False
        self._last_deliberation: list[dict[str, Any]] = []

    def stream_reset(self) -> None:
        with self.lock:
            delib = self.live.get("deliberation", [])
            self.live = {"active": True, "phase": "thinking", "reasoning": "",
                         "content": "", "seq": self.live.get("seq", 0) + 1,
                         "deliberation": delib}

    def stream_update(self, *, reasoning: str = "", content: str = "",
                      phase: Optional[str] = None) -> None:
        with self.lock:
            if reasoning:
                self.live["reasoning"] += reasoning
            if content:
                self.live["content"] += content
            if phase:
                self.live["phase"] = phase

    def stream_done(self) -> None:
        with self.lock:
            self.live["active"] = False
            self.live["phase"] = "done"

    def snapshot(self, cfg: dict[str, Any]) -> dict[str, Any]:
        ap = _active_provider(cfg)
        return {
            "status": self.status,
            "running": self.running,
            "mode": cfg.get("mode"),
            "active_model": (ap or {}).get("model"),
            "active_provider": (ap or {}).get("name"),
            "sleep_until": self.sleep_until,
            "sleep_reason": self.sleep_reason,
            "now": time.time(),
            "pending": self.pending,
            "last_error": self.last_error,
            "stats": self.stats,
            "max_procs": cfg.get("max_procs"),
            "disk_low_gb": cfg.get("disk_low_gb"),
            "objective": cfg.get("objective"),
            "active_strategy": cfg.get("active_strategy"),
            "active_strategy_name": (STRATEGIES.get(cfg.get("active_strategy") or "")
                                     or {}).get("name"),
            "deep_think_rounds": cfg.get("deep_think_rounds", 0),
            "strategies": [{"id": k, "name": v["name"], "when": v["when"]}
                           for k, v in STRATEGIES.items()],
            "context": {
                "budget": cfg.get("context_token_budget"),
                "used_est": sum(_msg_tokens(m) for m in self.messages)
                            + _msg_tokens(_system_prompt(cfg)),
                "msgs": len(self.messages),
                "pinned_specs": len(cfg.get("pinned_specs") or []),
                "has_summary": bool(cfg.get("rolling_summary")),
            },
            "live": self.live,
        }


STATE = AgentState()
_TIMELINE_RING: deque[dict[str, Any]] = deque(maxlen=400)


def _timeline(kind: str, **fields: Any) -> dict[str, Any]:
    ev = {"ts": time.time(), "iso": datetime.now().isoformat(timespec="seconds"),
          "kind": kind, **fields}
    _TIMELINE_RING.append(ev)
    try:
        with open(TIMELINE_PATH, "a", encoding="utf-8") as f:
            f.write(json.dumps(ev, ensure_ascii=False) + "\n")
    except Exception:  # noqa: BLE001
        pass
    return ev


def _rehydrate() -> None:
    """Restore the chat panel AND the agent's dialogue context after a service
    restart. The timeline is the durable record on disk; the in-memory ring and
    STATE.messages are rebuilt from it so reopening the page looks exactly like
    before the restart. Config (mode/strategy/deep-think/providers) is already
    persisted separately in agent_config.json."""
    try:
        lines = TIMELINE_PATH.read_text(encoding="utf-8", errors="ignore").splitlines()
    except Exception:  # noqa: BLE001
        return
    evs: list[dict[str, Any]] = []
    for ln in lines:
        ln = ln.strip()
        if not ln:
            continue
        try:
            evs.append(json.loads(ln))
        except Exception:  # noqa: BLE001
            continue
    # rebuild the display ring (newest maxlen events)
    _TIMELINE_RING.clear()
    for ev in evs[-_TIMELINE_RING.maxlen:]:
        _TIMELINE_RING.append(ev)
    # rebuild a compact dialogue context (user + assistant text only — no tool
    # call/result pairs, so the OpenAI message sequence stays valid). Keep the
    # most recent exchanges; older ones can be pulled back via recall_timeline.
    msgs: list[dict[str, Any]] = []
    for ev in evs:
        k = ev.get("kind")
        if k in ("user_msg", "user_msg_queued"):
            t = (ev.get("text") or "").strip()
            if t:
                msgs.append({"role": "user", "content": "[用户]: " + t})
        elif k == "assistant":
            t = (ev.get("text") or "").strip()
            if t:
                msgs.append({"role": "assistant", "content": t})
    if msgs:
        STATE.messages = msgs[-40:]


# --------------------------------------------------------------------------- #
# On-demand retrieval — history + docs are NOT preloaded into context; the
# model pulls them through these tools only when it needs them.
# --------------------------------------------------------------------------- #
def _recall_timeline(limit: int, kind: Optional[str]) -> dict[str, Any]:
    """Read the agent's own decision history on demand (newest first)."""
    limit = max(1, min(limit, 200))
    events: list[dict[str, Any]] = []
    try:
        lines = TIMELINE_PATH.read_text(encoding="utf-8", errors="ignore").splitlines()
        for ln in lines:
            ln = ln.strip()
            if not ln:
                continue
            try:
                ev = json.loads(ln)
            except Exception:  # noqa: BLE001
                continue
            if kind and ev.get("kind") != kind:
                continue
            events.append({k: v for k, v in ev.items() if k != "ts"})
    except FileNotFoundError:
        pass
    return {"events": events[-limit:][::-1], "returned": min(len(events), limit),
            "total_matched": len(events)}


def _list_docs() -> list[dict[str, Any]]:
    out: list[dict[str, Any]] = []
    if DOCS_DIR.exists():
        for f in sorted(DOCS_DIR.glob("*.md")):
            try:
                txt = f.read_text(encoding="utf-8", errors="ignore")
            except Exception:  # noqa: BLE001
                txt = ""
            first = next((l.strip("# ").strip() for l in txt.splitlines()
                          if l.strip()), "")
            out.append({"name": f.name, "chars": len(txt), "title": first[:120]})
    return out


def _read_doc(name: str, query: Optional[str], max_chars: int) -> dict[str, Any]:
    if not name:
        return {"ok": False, "error": "no doc name"}
    # guard against path traversal — only files directly in DOCS_DIR
    p = DOCS_DIR / Path(name).name
    if not p.exists() or p.suffix.lower() != ".md":
        return {"ok": False, "error": f"doc not found: {name}",
                "available": [d["name"] for d in _list_docs()]}
    text = p.read_text(encoding="utf-8", errors="ignore")
    max_chars = max(500, min(max_chars, 20000))
    if query:
        # return only paragraphs containing the query (case-insensitive) plus
        # neighbours, so the model can search a big doc cheaply.
        paras = re.split(r"\n\s*\n", text)
        q = query.lower()
        hits = [i for i, para in enumerate(paras) if q in para.lower()]
        if hits:
            keep: list[int] = []
            for i in hits:
                for j in (i - 1, i, i + 1):
                    if 0 <= j < len(paras) and j not in keep:
                        keep.append(j)
            chunk = "\n\n".join(paras[i] for i in sorted(keep))
            truncated = len(chunk) > max_chars
            return {"ok": True, "name": p.name, "query": query,
                    "matches": len(hits), "content": chunk[:max_chars],
                    "truncated": truncated}
        return {"ok": True, "name": p.name, "query": query, "matches": 0,
                "content": "", "note": "关键词未命中,可不带 query 再读或换关键词。"}
    truncated = len(text) > max_chars
    return {"ok": True, "name": p.name, "content": text[:max_chars],
            "truncated": truncated, "total_chars": len(text)}


# --------------------------------------------------------------------------- #
# Project knowledge network — persistent nodes (with links) + per-generation
# performance ledger. This is the agent's long-term project memory: queryable,
# survives restarts, lets it stay fully aware of project state across sessions.
# --------------------------------------------------------------------------- #
_KB_LOCK = threading.RLock()


def _load_kb() -> dict[str, Any]:
    kb = {"seq": 0, "nodes": [], "generations": []}
    src = KB_PATH if KB_PATH.exists() else (AGENT_DIR / "agent_knowledge.seed.json")
    if src.exists():
        try:
            data = json.loads(src.read_text(encoding="utf-8"))
            kb.update(data)
        except Exception:  # noqa: BLE001
            pass
    return kb


def _save_kb(kb: dict[str, Any]) -> None:
    KB_PATH.write_text(json.dumps(kb, ensure_ascii=False, indent=2), encoding="utf-8")


def _kb_write(args: dict[str, Any]) -> dict[str, Any]:
    with _KB_LOCK:
        kb = _load_kb()
        now = datetime.now().isoformat(timespec="seconds")
        nid = (args.get("id") or "").strip()
        node = None
        if nid:
            node = next((n for n in kb["nodes"] if n.get("id") == nid), None)
        if node is None:
            kb["seq"] = int(kb.get("seq", 0)) + 1
            nid = nid or f"k{kb['seq']}"
            node = {"id": nid, "created": now}
            kb["nodes"].append(node)
        node["kind"] = args.get("kind", node.get("kind", "other"))
        node["title"] = args.get("title", node.get("title", ""))
        node["body"] = args.get("body", node.get("body", ""))
        if args.get("tags") is not None:
            node["tags"] = args.get("tags")
        if args.get("links") is not None:
            node["links"] = args.get("links")
        node["updated"] = now
        _save_kb(kb)
        _timeline("kb_write", id=node["id"], node_kind=node["kind"], title=node["title"])
        return {"ok": True, "id": node["id"], "node_count": len(kb["nodes"])}


def _kb_query(args: dict[str, Any]) -> dict[str, Any]:
    kb = _load_kb()
    nodes = kb["nodes"]
    nid = (args.get("id") or "").strip()
    if nid:
        node = next((n for n in nodes if n.get("id") == nid), None)
        if not node:
            return {"ok": False, "error": f"node not found: {nid}",
                    "ids": [n["id"] for n in nodes]}
        link_ids = set(node.get("links") or [])
        # neighbours = its links + anything that links back to it (undirected view)
        link_ids |= {n["id"] for n in nodes if nid in (n.get("links") or [])}
        neighbours = [n for n in nodes if n["id"] in link_ids]
        return {"ok": True, "node": node, "neighbours": neighbours}
    q = (args.get("query") or "").lower()
    kind = args.get("kind")
    tag = args.get("tag")
    limit = max(1, min(int(args.get("limit", 12) or 12), 60))
    out = []
    for n in nodes:
        if kind and n.get("kind") != kind:
            continue
        if tag and tag not in (n.get("tags") or []):
            continue
        if q:
            hay = (n.get("title", "") + " " + n.get("body", "") + " "
                   + " ".join(n.get("tags") or [])).lower()
            if q not in hay:
                continue
        out.append({"id": n["id"], "kind": n.get("kind"), "title": n.get("title"),
                    "tags": n.get("tags") or [], "links": n.get("links") or [],
                    "body": (n.get("body", "")[:280])})
    return {"ok": True, "matches": len(out), "nodes": out[:limit],
            "total_nodes": len(nodes)}


def _record_gen(args: dict[str, Any]) -> dict[str, Any]:
    with _KB_LOCK:
        kb = _load_kb()
        rec = {
            "project": args.get("project"), "lineage": args.get("lineage"),
            "gen": args.get("gen"), "checkpoint": args.get("checkpoint"),
            "metrics": args.get("metrics") or {}, "note": args.get("note", ""),
            "ts": datetime.now().isoformat(timespec="seconds"),
        }
        kb["generations"].append(rec)
        _save_kb(kb)
        _timeline("gen_recorded", project=rec["project"], lineage=rec["lineage"],
                  gen=rec["gen"])
        return {"ok": True, "recorded": rec, "total_records": len(kb["generations"])}


def _query_gens(args: dict[str, Any]) -> dict[str, Any]:
    kb = _load_kb()
    recs = kb["generations"]
    proj = args.get("project")
    lin = args.get("lineage")
    limit = max(1, min(int(args.get("limit", 40) or 40), 200))
    sel = [r for r in recs
           if (not proj or r.get("project") == proj)
           and (not lin or r.get("lineage") == lin)]
    sel.sort(key=lambda r: ((r.get("lineage") or ""), (r.get("gen") or 0)))
    return {"ok": True, "count": len(sel), "records": sel[-limit:],
            "lineages": sorted({r.get("lineage") for r in recs if r.get("lineage")})}


# --------------------------------------------------------------------------- #
# Observation — what we feed the model. Compact + deterministic.
# --------------------------------------------------------------------------- #
_PE_GEN_RE = re.compile(
    r"Gen\s+(\d+)\s*\|\s*protagonist\s+avg=([\-\d.]+)\s+best=([\-\d.]+).*?"
    r"me_cov=(\d+)/(\d+)\s+qd=([\-\d.]+)")


def _pe_log_summary(path: Path, tail_lines: int = 4000) -> Optional[dict[str, Any]]:
    try:
        lines = path.read_text(encoding="utf-8", errors="ignore").splitlines()[-tail_lines:]
    except Exception:  # noqa: BLE001
        return None
    last = None
    gens: list[tuple] = []
    for ln in lines:
        m = _PE_GEN_RE.search(ln)
        if m:
            gens.append(m.groups())
    if not gens:
        return None
    g0, gl = gens[0], gens[-1]
    extinct = sum(1 for ln in lines if "extinct" in ln.lower())
    errs = sum(1 for ln in lines if "[error]" in ln.lower() or "[critical]" in ln.lower())
    return {
        "gen": int(gl[0]), "avg": float(gl[1]), "best": float(gl[2]),
        "me_cov": f"{gl[3]}/{gl[4]}", "qd": float(gl[5]),
        "gen_start": int(g0[0]), "avg_start": float(g0[1]),
        "extinct_hits": extinct, "errors": errs,
    }


def _dp_jsonl_summary(path: Path, tail: int = 60) -> Optional[dict[str, Any]]:
    try:
        lines = path.read_text(encoding="utf-8", errors="ignore").splitlines()[-tail:]
    except Exception:  # noqa: BLE001
        return None
    recs = []
    for ln in lines:
        ln = ln.strip()
        if not ln:
            continue
        try:
            recs.append(json.loads(ln))
        except Exception:  # noqa: BLE001
            pass
    if not recs:
        return None
    last = recs[-1]
    kills = sum(int(r.get("prey_kills", 0) or 0) for r in recs)
    score = last.get("score")
    unlocks = last.get("unlocks")
    return {
        "episode": last.get("episode"), "score": score, "unlocks": unlocks,
        "kills_last%d" % len(recs): kills,
        "reward": last.get("reward"), "deaths_cold": last.get("deaths_cold"),
        "deaths_food": last.get("deaths_food"),
    }


def _trend(series: list[float]) -> dict[str, Any]:
    """Early-vs-recent comparison + verdict for a numeric series."""
    xs = [float(v) for v in series if isinstance(v, (int, float))]
    if len(xs) < 4:
        return {"n": len(xs), "verdict": "too_short"}
    half = len(xs) // 2
    early = sum(xs[:half]) / half
    late = sum(xs[half:]) / (len(xs) - half)
    delta = late - early
    rel = delta / (abs(early) + 1e-9)
    if rel > 0.08:
        verdict = "improving"
    elif rel < -0.08:
        verdict = "declining"
    else:
        verdict = "plateau"
    return {"n": len(xs), "first": round(early, 3), "recent": round(late, 3),
            "delta": round(delta, 3), "rel": round(rel, 3),
            "last": round(xs[-1], 3), "max": round(max(xs), 3),
            "min": round(min(xs), 3), "verdict": verdict}


def _viability_hint(trends: list[dict[str, Any]]) -> str:
    """Coarse 'is this param worth continuing' signal from key-metric trends."""
    verds = [t.get("verdict") for t in trends if t]
    if not verds or all(v == "too_short" for v in verds):
        return "样本太少,再观察几代/几集再判断"
    if any(v == "improving" for v in verds) and not any(v == "declining" for v in verds):
        return "指标上行,值得继续"
    if all(v == "plateau" for v in verds):
        return "已进入平台,考虑调旋钮/换参或早停,别空耗算力"
    if any(v == "declining" for v in verds):
        return "出现下行,建议早停并回退/换参(先查 knowledge 既往坑)"
    return "涨跌互现,再观察一窗口确认趋势"


def _analyze_metrics(project: str, file: str, window: int = 0) -> dict[str, Any]:
    """Quick viability read of a run's metrics (reuses the panel's parsing).
    PE: parse 'Gen N | avg/best/me_cov/qd' lines. DP: parse jsonl score/reward.
    Returns per-metric trend + a coarse continue/early-stop hint."""
    import server  # lazy
    if project == "pe":
        p = server.PE_ROOT / file
        if not p.exists() and getattr(server, "PE_PANEL_LOGS", None) \
                and server.PE_PANEL_LOGS.exists():
            p = server.PE_PANEL_LOGS / file
        if not p.exists():
            return {"ok": False, "error": f"PE log not found: {file}"}
        lines = p.read_text(encoding="utf-8", errors="ignore").splitlines()
        rows = [m.groups() for ln in lines if (m := _PE_GEN_RE.search(ln))]
        if not rows:
            return {"ok": False, "error": "no PE 'Gen N | avg=...' lines parsed"}
        if window:
            rows = rows[-int(window):]
        gens = [int(r[0]) for r in rows]
        avg = _trend([float(r[1]) for r in rows])
        best = _trend([float(r[2]) for r in rows])
        cov = _trend([float(r[3]) for r in rows])
        qd = _trend([float(r[5]) for r in rows])
        extinct = sum(1 for ln in lines if "extinct" in ln.lower())
        return {"ok": True, "project": "pe", "file": file,
                "gen_range": [gens[0], gens[-1]], "points": len(rows),
                "avg": avg, "best": best, "me_cov": cov, "qd": qd,
                "extinct_hits": extinct,
                "hint": _viability_hint([avg, qd, cov])}
    # DP jsonl
    p = server.DP_RUNS / file
    if not p.exists():
        return {"ok": False, "error": f"DP jsonl not found: {file}"}
    recs = []
    for ln in p.read_text(encoding="utf-8", errors="ignore").splitlines():
        ln = ln.strip()
        if not ln:
            continue
        try:
            recs.append(json.loads(ln))
        except Exception:  # noqa: BLE001
            pass
    if not recs:
        return {"ok": False, "error": "no jsonl rows parsed"}
    if window:
        recs = recs[-int(window):]
    score = _trend([float(r.get("score", 0) or 0) for r in recs])
    reward = _trend([float(r.get("reward", 0) or 0) for r in recs])
    unlocks = _trend([float(sum(1 for x in r.get("unlocks", []) if x)) for r in recs])
    return {"ok": True, "project": "dp", "file": file, "points": len(recs),
            "episode_last": recs[-1].get("episode"),
            "score": score, "reward": reward, "unlocks": unlocks,
            "hint": _viability_hint([score, reward, unlocks])}


def _gather_observation() -> dict[str, Any]:
    import server  # lazy
    obs: dict[str, Any] = {"time": datetime.now().isoformat(timespec="seconds")}
    try:
        obs["system"] = server.system_status()
    except Exception as e:  # noqa: BLE001
        obs["system"] = {"error": str(e)}
    try:
        procs = server.processes()
        obs["processes"] = {
            "total": procs.get("total"), "dp": procs.get("dp_count"),
            "pe": procs.get("pe_count"),
            "list": [{"project": p["project"], "pid": p["pid"],
                      "config": p.get("config"), "uptime_s": p.get("elapsed_s"),
                      "source": p.get("source")}
                     for p in procs.get("processes", [])][:12],
        }
    except Exception as e:  # noqa: BLE001
        obs["processes"] = {"error": str(e)}

    # PE log summaries (external sweep logs + panel logs).
    pe_logs: dict[str, Any] = {}
    try:
        for p in sorted(server.PE_ROOT.glob("onepot_sD_*.log"),
                        key=lambda x: x.stat().st_mtime, reverse=True)[:6]:
            s = _pe_log_summary(p)
            if s:
                pe_logs[p.name] = s
        if server.PE_PANEL_LOGS.exists():
            for p in sorted(server.PE_PANEL_LOGS.glob("*.log"),
                            key=lambda x: x.stat().st_mtime, reverse=True)[:4]:
                s = _pe_log_summary(p)
                if s:
                    pe_logs[p.name] = s
    except Exception as e:  # noqa: BLE001
        pe_logs["error"] = str(e)
    obs["pe_runs"] = pe_logs

    # DP jsonl summaries.
    dp_runs: dict[str, Any] = {}
    try:
        for p in sorted(server.DP_RUNS.glob("ft_*.jsonl"),
                        key=lambda x: x.stat().st_mtime, reverse=True)[:5]:
            s = _dp_jsonl_summary(p)
            if s:
                dp_runs[p.name] = s
    except Exception as e:  # noqa: BLE001
        dp_runs["error"] = str(e)
    obs["dp_runs"] = dp_runs

    STATE.last_obs_snapshot = obs
    return obs


# --------------------------------------------------------------------------- #
# Tools exposed to the LLM (OpenAI function-calling schema).
# --------------------------------------------------------------------------- #
def _fn(name: str, desc: str, props: dict[str, Any],
        required: Optional[list[str]] = None) -> dict[str, Any]:
    params: dict[str, Any] = {"type": "object", "properties": props}
    if required:
        params["required"] = required
    return {"type": "function",
            "function": {"name": name, "description": desc, "parameters": params}}


def _tool_schemas() -> list[dict[str, Any]]:
    return [
        _fn("get_status",
            "读取机器与训练现状:GPU/磁盘/进程数、各 PE run 的最新 gen/me_cov/qd、各 DP run 的 score/unlocks/kills。决策前先调。",
            {}),
        _fn("list_pe_configs", "列出可用的 PE TOML 配置文件名。", {}),
        _fn("list_checkpoints",
            "列出 DP 的 runs/*.pt 与 PE 的 checkpoint_gen* 检查点。",
            {"project": {"type": "string", "enum": ["dp", "pe"]}}),
        _fn("read_log_tail",
            "读取某个 run 日志的末尾若干行用于诊断。",
            {"run_id": {"type": "string", "description": "面板 run_id(优先)"},
             "file": {"type": "string", "description": "或日志文件名(配合 project)"},
             "project": {"type": "string", "enum": ["dp", "pe"]},
             "n": {"type": "integer", "default": 40}}),
        _fn("analyze_metrics",
            "快速判断某个 run 的参数是否值得继续(读它的指标做趋势分析):PE 看 avg/best/me_cov/qd 逐代趋势,DP 看 score/reward/unlocks 逐集趋势,"
            "返回每个指标的 改善/平台/下行 判定 + 一句可继续性建议(hint)。用于快速迭代:小步探路后先 analyze 再决定继续/早停/换参。"
            "file 用 get_status 里看到的 run 文件名(PE 用 .log,DP 用 .jsonl)。window=只看最近 N 个点(0=全部)。",
            {"project": {"type": "string", "enum": ["dp", "pe"]},
             "file": {"type": "string", "description": "PE: onepot_sD_*.log;DP: ft_*.jsonl"},
             "window": {"type": "integer", "default": 0}},
            ["project", "file"]),
        _fn("start_pe_training",
            "启动一局 PE 训练(给定 toml 配置;可覆盖 seed/代数/续跑检查点,面板会克隆新 toml 不改原文件)。必须写 reason。",
            {"config": {"type": "string", "description": "PE TOML 文件名"},
             "seed": {"type": "integer"},
             "generations": {"type": "integer"},
             "resume_checkpoint": {"type": "string"},
             "reason": {"type": "string", "description": "为什么这么训(必填,进历史)"}},
            ["config", "reason"]),
        _fn("start_dp_training",
            "启动一局 DP 训练。必须写 reason。",
            {"checkpoint": {"type": "string", "description": "runs/ 下的基线 .pt(可空=从头)"},
             "seed": {"type": "integer"},
             "steps": {"type": "integer"},
             "n_envs": {"type": "integer"},
             "tag": {"type": "string"},
             "extra_args": {"type": "string"},
             "reason": {"type": "string", "description": "为什么这么训(必填,进历史)"}},
            ["reason"]),
        _fn("stop_training",
            "停止一个训练进程(检查点会自动保存,安全)。给 run_id 或 pid。必须写 reason。",
            {"run_id": {"type": "string"}, "pid": {"type": "integer"},
             "reason": {"type": "string"}},
            ["reason"]),
        _fn("cleanup_disk",
            "清理日志/trace 释放磁盘(只动 .log/.jsonl/trace,移到回收站;不动 *.pt——检查点请用 delete_checkpoint)。",
            {"project": {"type": "string", "enum": ["dp", "pe"]},
             "reason": {"type": "string"}},
            ["reason"]),
        _fn("delete_checkpoint",
            "删除你判定不重要的 DP .pt 检查点(移到回收站,可恢复)。受保护清单会被硬拦跳过。高后果动作,请先 list_checkpoints 看清楚再决定;必填 reason。",
            {"names": {"type": "array", "items": {"type": "string"},
                       "description": "runs/ 下要删的 .pt 文件名列表"},
             "reason": {"type": "string", "description": "为何认为这些不重要可删(必填,进历史)"}},
            ["names", "reason"]),
        _fn("recall_timeline",
            "按需回看你自己过去的决策/执行/休眠/笔记记录(读决策历史),避免重复劳动或遗忘上下文。不传入时不占上下文。",
            {"limit": {"type": "integer", "default": 20, "description": "返回最近多少条"},
             "kind": {"type": "string", "description": "可选过滤:decision/execute/sleep/note/compress/spec_pinned 等"}}),
        _fn("list_docs", "列出可按需调阅的策略/交接/知识文档(playbook、handoff、知识笔记等)。", {}),
        _fn("read_doc",
            "读一份文档的内容(可只取包含关键词的段落以省 token)。先 list_docs 拿文件名。",
            {"name": {"type": "string", "description": "文档文件名(来自 list_docs)"},
             "query": {"type": "string", "description": "可选关键词:仅返回包含它的段落上下文"},
             "max_chars": {"type": "integer", "default": 6000}},
            ["name"]),
        _fn("sleep",
            "进入休眠以省额度。训练平稳时应当休眠;watcher 会在到点或关键事件(run 结束/报错/灭绝/磁盘告警)时唤醒你。",
            {"minutes": {"type": "integer", "description": "最长睡多久"},
             "reason": {"type": "string"}},
            ["minutes", "reason"]),
        _fn("log_note",
            "记一条笔记/建议到历史(用于机制级代码建议、负结果、下一步计划等不需要立刻执行的内容)。",
            {"text": {"type": "string"},
             "level": {"type": "string",
                       "enum": ["info", "suggestion", "warning"], "default": "info"}},
            ["text"]),
        _fn("pin_spec",
            "把一条关键结论/spec/约束钉进【常驻记忆】:它会写进系统提示词每次都带上,且永不被上下文压缩丢弃。"
            "用于你提炼出的、必须长期遵守的事实(如某 config 的最佳 seed、某红线、当前血脉命名)。简短精炼。",
            {"text": {"type": "string", "description": "要长期记住的一句话(精炼)"}},
            ["text"]),
        _fn("kb_write",
            "写入/更新【项目知识网络】的一个节点(持久化,跨会话不丢)。用于沉淀你对项目的理解:蓝图目标、红线、踩过的坑、机制结论、某血脉的状态等。"
            "给 id 则更新该节点,否则新建。用 links 关联到其它节点 id 形成网络。",
            {"id": {"type": "string", "description": "更新已有节点时给其 id;新建留空"},
             "kind": {"type": "string",
                      "enum": ["blueprint", "redline", "pitfall", "mechanism",
                               "finding", "lineage", "todo", "other"],
                      "description": "节点类型"},
             "title": {"type": "string"},
             "body": {"type": "string"},
             "tags": {"type": "array", "items": {"type": "string"}},
             "links": {"type": "array", "items": {"type": "string"},
                       "description": "关联到的其它节点 id"}},
            ["kind", "title", "body"]),
        _fn("kb_query",
            "检索【项目知识网络】。给 id 则取该节点并连带返回它关联的邻居(1跳=知识网络);否则按 query 关键词/kind/tag 过滤。决策前应先查坑与既往结论。",
            {"id": {"type": "string"},
             "query": {"type": "string", "description": "关键词(标题/正文/标签)"},
             "kind": {"type": "string"},
             "tag": {"type": "string"},
             "limit": {"type": "integer", "default": 12}}),
        _fn("record_gen",
            "把某一代/某血脉的模型表现记进【逐代台账】(持久化),便于日后查询哪代最好、趋势如何。",
            {"project": {"type": "string", "enum": ["dp", "pe"]},
             "lineage": {"type": "string", "description": "血脉/配置名(如 onepot_sD、explore_s7)"},
             "gen": {"type": "integer"},
             "checkpoint": {"type": "string", "description": "对应 checkpoint 文件名(可选)"},
             "metrics": {"type": "object",
                         "description": "指标键值,如 {avg,best,me_cov,qd} 或 DP 的 {score,unlocks,prey_kills}"},
             "note": {"type": "string"}},
            ["project", "lineage", "gen"]),
        _fn("query_gens",
            "查询【逐代台账】:某项目/血脉各代的表现记录(按代排序,可看趋势与最佳代)。",
            {"project": {"type": "string", "enum": ["dp", "pe"]},
             "lineage": {"type": "string"},
             "limit": {"type": "integer", "default": 40}}),
        _fn("use_strategy",
            "切换当前【策略】。你自己判断当前处境最该用哪条策略就调它;切换后系统会把这条策略的详细打法(该用的工具/关注参数/要避的坑)注入你的常驻上下文,接下来按它行动。可随处境变化随时再切。返回该策略的详细打法。",
            {"strategy": {"type": "string",
                          "enum": list(STRATEGIES.keys()),
                          "description": "策略 id:fast_iter/ab_knob/hist_bench/early_stop/ckpt_hygiene/compute_yield"},
             "reason": {"type": "string", "description": "为何切到这条策略(进历史)"}},
            ["strategy"]),
        _fn("list_strategies",
            "列出所有可选策略及其适用场景与详细打法(忘了有哪些策略/具体怎么做时查)。", {}),
        _fn("deep_think",
            "面临复杂决策(如启训/换参/多因素权衡)时主动调用,进行多轮引导式内部推演——先看现状→找原因→权衡→定方案。"
            "推演内容流式展示给用户但不占你的长期上下文(用完即丢)。简单问答无需调此工具。每轮唤醒最多调一次。",
            {"rounds": {"type": "integer", "default": 2,
                        "description": "推演轮数(1-上限)"}},
            ["rounds"]),
    ]


READ_TOOLS = {"get_status", "list_pe_configs", "list_checkpoints", "read_log_tail",
              "analyze_metrics",
              "sleep", "log_note", "cleanup_disk", "pin_spec",
              "recall_timeline", "list_docs", "read_doc",
              "kb_write", "kb_query", "record_gen", "query_gens",
              "use_strategy", "list_strategies", "deep_think"}


# --------------------------------------------------------------------------- #
# Guards.
# --------------------------------------------------------------------------- #
def _guard_launch(cfg: dict[str, Any]) -> Optional[str]:
    """Return a refusal reason string if a new training MUST NOT start, else None."""
    import server
    try:
        procs = server.processes()
        if procs.get("total", 0) >= cfg.get("max_procs", 10):
            return (f"拒绝启动:已有 {procs['total']} 个训练进程,达到上限 "
                    f"{cfg['max_procs']}。请先停掉一些或等其结束。")
    except Exception:  # noqa: BLE001
        pass
    try:
        st = server.system_status()
        disk = st.get("disk") or {}
        free = disk.get("free_gb")
        if free is not None and free < cfg.get("disk_low_gb", 25.0):
            return (f"拒绝启动:磁盘只剩 {free}G,低于阈值 {cfg['disk_low_gb']}G。"
                    f"请先 cleanup_disk 释放空间。")
    except Exception:  # noqa: BLE001
        pass
    return None


# --------------------------------------------------------------------------- #
# Tool execution. Returns a JSON-serializable dict (the tool result).
# --------------------------------------------------------------------------- #
def _execute_tool(name: str, args: dict[str, Any], cfg: dict[str, Any]) -> dict[str, Any]:
    import server
    try:
        if name == "get_status":
            return _gather_observation()
        if name == "list_pe_configs":
            return {"configs": server.pe_configs()}
        if name == "list_checkpoints":
            proj = args.get("project", "pe")
            if proj == "dp":
                return {"checkpoints": server.dp_checkpoints()}
            return {"checkpoints": server.pe_checkpoints()}
        if name == "read_log_tail":
            n = int(args.get("n", 40))
            rid = args.get("run_id")
            if rid and rid in server.RUNS:
                return {"lines": server.RUNS[rid].lines[-n:]}
            proj = args.get("project", "pe")
            fname = args.get("file", "")
            base = server.PE_ROOT if proj == "pe" else server.DP_RUNS
            p = base / fname
            if not p.exists():
                return {"error": f"log not found: {fname}"}
            return {"lines": p.read_text(encoding="utf-8", errors="ignore").splitlines()[-n:]}
        if name == "analyze_metrics":
            return _analyze_metrics(args.get("project", "pe"), args.get("file", ""),
                                    int(args.get("window", 0) or 0))
        if name == "start_pe_training":
            refusal = _guard_launch(cfg)
            if refusal:
                return {"ok": False, "refused": True, "reason": refusal}
            req = server.PETrainReq(config=args["config"], seed=args.get("seed"),
                                    resume_checkpoint=args.get("resume_checkpoint"),
                                    generations=args.get("generations"))
            res = server.pe_train(req)
            STATE.stats["actions"] += 1
            return {"ok": True, **res}
        if name == "start_dp_training":
            refusal = _guard_launch(cfg)
            if refusal:
                return {"ok": False, "refused": True, "reason": refusal}
            req = server.DPTrainReq(
                checkpoint=args.get("checkpoint"), seed=args.get("seed", 24),
                steps=args.get("steps", 8_000_000), n_envs=args.get("n_envs", 4),
                tag=args.get("tag", "agent"), extra_args=args.get("extra_args", ""))
            res = server.dp_train(req)
            STATE.stats["actions"] += 1
            return {"ok": True, **res}
        if name == "stop_training":
            req = server.StopReq(run_id=args.get("run_id"), pid=args.get("pid"))
            res = server.train_stop(req)
            STATE.stats["actions"] += 1
            return {"ok": True, **res}
        if name == "cleanup_disk":
            proj = args.get("project", "pe")
            before = (server.system_status().get("disk") or {}).get("free_gb")
            # Reuse the panel's safe log clear over discovered logs (jsonl excluded
            # by default protection; *.pt never touched). Also clear traces.
            logs = server.dp_logs() if proj == "dp" else server.pe_logs()
            names = [l["name"] for l in logs
                     if l.get("name", "").endswith(".log") and not l.get("in_use")]
            cleared = {}
            if names:
                creq = server.ClearLogsReq(project=proj, names=names, hard=False)
                cleared = server.logs_clear(creq)
            after = (server.system_status().get("disk") or {}).get("free_gb")
            STATE.stats["actions"] += 1
            return {"ok": True, "freed_gb": (after - before) if (after and before) else None,
                    "free_gb_now": after, "cleared": cleared}
        if name == "delete_checkpoint":
            names = args.get("names") or []
            if isinstance(names, str):
                names = [names]
            names = [n for n in names if n]
            if not names:
                return {"ok": False, "error": "no checkpoint names given"}
            res = server.dp_clear_checkpoints(server.DeleteReq(names=names, hard=False))
            STATE.stats["actions"] += 1
            _timeline("checkpoint_deleted", removed=res.get("removed", []),
                      skipped=res.get("skipped", []), reason=args.get("reason", ""))
            return {"ok": True, **res}
        if name == "recall_timeline":
            return _recall_timeline(int(args.get("limit", 20) or 20), args.get("kind"))
        if name == "list_docs":
            return {"docs": _list_docs()}
        if name == "read_doc":
            return _read_doc(args.get("name", ""), args.get("query"),
                             int(args.get("max_chars", 6000) or 6000))
        if name == "kb_write":
            return _kb_write(args)
        if name == "kb_query":
            return _kb_query(args)
        if name == "record_gen":
            return _record_gen(args)
        if name == "query_gens":
            return _query_gens(args)
        if name == "use_strategy":
            sid = args.get("strategy", "")
            if sid not in STRATEGIES:
                return {"ok": False, "error": f"unknown strategy: {sid}",
                        "available": list(STRATEGIES.keys())}
            cfg["active_strategy"] = sid
            _save_config(cfg)
            s = STRATEGIES[sid]
            _timeline("strategy", strategy=sid, name=s["name"],
                      reason=args.get("reason", ""))
            return {"ok": True, "active_strategy": sid, "name": s["name"],
                    "when": s["when"], "guide": s["guide"]}
        if name == "list_strategies":
            return {"active": cfg.get("active_strategy"),
                    "strategies": [{"id": k, "name": v["name"], "when": v["when"],
                                    "guide": v["guide"]} for k, v in STRATEGIES.items()]}
        if name == "deep_think":
            cap = int(cfg.get("deep_think_rounds", 0) or 0)
            if cap <= 0:
                return {"ok": False, "error": "深度思考已禁用(上限=0)"}
            if STATE._deliberated_this_wake:
                return {"ok": False, "error": "本轮唤醒已执行过深度思考,无需重复"}
            rounds = max(1, min(int(args.get("rounds", 2)), cap))
            base_msgs = [_system_prompt(cfg)] + _safe_tail(STATE.messages)
            try:
                results = _deliberate(cfg, base_msgs, rounds)
            except Exception as e:  # noqa: BLE001
                _timeline("error", text=f"deep_think 失败: {e}")
                return {"ok": False, "error": str(e)}
            STATE._deliberated_this_wake = True
            STATE._last_deliberation = results
            blob = "\n".join(f"[第{e['round']}步·{e['label']}] {e['text']}"
                             for e in results if e.get("text"))
            return {"ok": True, "conclusion": blob[:4000]}
        if name == "sleep":
            minutes = max(1, min(int(args.get("minutes", cfg["default_sleep_min"])),
                                 cfg["max_sleep_min"]))
            STATE.sleep_until = time.time() + minutes * 60
            STATE.sleep_reason = args.get("reason", "")
            STATE.status = "sleeping"
            STATE.wake_event.clear()
            _timeline("sleep", minutes=minutes, reason=args.get("reason", ""))
            return {"ok": True, "sleeping_minutes": minutes}
        if name == "log_note":
            _timeline("note", level=args.get("level", "info"), text=args.get("text", ""))
            return {"ok": True}
        if name == "pin_spec":
            text = (args.get("text") or "").strip()
            if not text:
                return {"ok": False, "error": "empty spec"}
            specs = cfg.setdefault("pinned_specs", [])
            if text not in specs:
                specs.append(text)
            # cap so the always-present block stays bounded
            if len(specs) > 40:
                del specs[: len(specs) - 40]
            _save_config(cfg)
            _timeline("spec_pinned", text=text, count=len(specs))
            return {"ok": True, "pinned_count": len(specs)}
    except HTTPException as e:
        return {"ok": False, "error": f"{e.status_code}: {e.detail}"}
    except Exception as e:  # noqa: BLE001
        return {"ok": False, "error": str(e)}
    return {"ok": False, "error": f"unknown tool {name}"}


# --------------------------------------------------------------------------- #
# OpenAI-compatible chat call (stdlib urllib; no extra deps).
# --------------------------------------------------------------------------- #
def _chat(cfg: dict[str, Any], messages: list[dict[str, Any]]) -> dict[str, Any]:
    prov = _active_provider(cfg)
    if not prov:
        raise RuntimeError("no active model provider configured")
    url = prov["base_url"].rstrip("/") + "/chat/completions"
    body = {
        "model": prov["model"],
        "messages": messages,
        "tools": _tool_schemas(),
        "tool_choice": "auto",
        "temperature": cfg.get("temperature", 0.3),
        "max_tokens": cfg.get("max_tokens", 900),
    }
    body["stream"] = True
    body["stream_options"] = {"include_usage": True}
    data = json.dumps(body).encode("utf-8")
    req = urllib.request.Request(url, data=data, method="POST", headers={
        "Content-Type": "application/json",
        "Authorization": "Bearer " + prov.get("api_key", ""),
    })
    content_parts: list[str] = []
    reasoning_parts: list[str] = []
    tool_map: dict[int, dict[str, Any]] = {}
    usage: dict[str, Any] = {}
    STATE.stream_reset()
    try:
        with urllib.request.urlopen(req, timeout=180) as resp:
            for raw in resp:                      # SSE: one "data: {...}" per line
                line = raw.decode("utf-8", "replace").strip()
                if not line or line.startswith(":"):
                    continue
                if line.startswith("data:"):
                    line = line[5:].strip()
                if line == "[DONE]":
                    break
                try:
                    o = json.loads(line)
                except Exception:  # noqa: BLE001
                    continue
                if o.get("usage"):
                    usage = o["usage"]
                ch = o.get("choices") or []
                if not ch:
                    continue
                d = ch[0].get("delta") or {}
                rc = d.get("reasoning_content")
                if rc:
                    reasoning_parts.append(rc)
                    STATE.stream_update(reasoning=rc, phase="thinking")
                c = d.get("content")
                if c:
                    content_parts.append(c)
                    STATE.stream_update(content=c, phase="replying")
                for tcd in (d.get("tool_calls") or []):
                    idx = tcd.get("index", 0)
                    slot = tool_map.setdefault(idx, {"id": "", "type": "function",
                        "function": {"name": "", "arguments": ""}})
                    if tcd.get("id"):
                        slot["id"] = tcd["id"]
                    fnd = tcd.get("function") or {}
                    if fnd.get("name"):
                        slot["function"]["name"] = fnd["name"]
                    if fnd.get("arguments"):
                        slot["function"]["arguments"] += fnd["arguments"]
                    STATE.stream_update(phase="tool:" + slot["function"]["name"])
    finally:
        STATE.stream_done()
    STATE.stats["llm_calls"] += 1
    STATE.stats["prompt_tokens"] += int(usage.get("prompt_tokens", 0) or 0)
    STATE.stats["completion_tokens"] += int(usage.get("completion_tokens", 0) or 0)
    msg: dict[str, Any] = {"role": "assistant", "content": "".join(content_parts)}
    tcs = [tool_map[k] for k in sorted(tool_map)]
    if tcs:
        msg["tool_calls"] = tcs
    reasoning = "".join(reasoning_parts)
    if reasoning:
        msg["reasoning_content"] = reasoning
    return msg


def _system_prompt(cfg: dict[str, Any]) -> dict[str, Any]:
    """The foundation — ALWAYS present, NEVER compressed: role + red lines +
    decision tree + objective/settings + agent-pinned specs + rolling summary
    of any compressed-out history."""
    mode = cfg.get("mode")
    mode_note = ("【请求模式】启动训练/停止等受控动作会先变成请求卡,需人工批准后才执行。"
                 if mode == "request" else
                 "【完全决策权】你的动作会自动批准并立即执行,但参数+理由都会进历史。")
    extra = (f"\n# 当前设置\n模式: {mode_note}\n并发进程上限: {cfg['max_procs']}。"
             f"磁盘低于 {cfg['disk_low_gb']}G 会强制先清理。\n"
             f"用户目标: {cfg.get('objective','')}\n")
    specs = cfg.get("pinned_specs") or []
    if specs:
        extra += ("\n# 常驻记忆 / 你提取的关键 spec (始终有效,务必遵守)\n"
                  + "\n".join(f"- {s}" for s in specs) + "\n"
                  + "(用 pin_spec 工具补充关键结论;它们永不会被压缩。)\n")
    summ = cfg.get("rolling_summary") or ""
    if summ:
        extra += f"\n# 早期历史摘要 (较早的对话已压缩成下文,细节以此为准)\n{summ}\n"
    extra += _strategy_block(cfg)
    return {"role": "system", "content": STRATEGY_DIGEST + extra}


def _estimate_tokens(text: Optional[str]) -> int:
    """Rough token estimate that works for mixed CN/EN. CJK ≈ 1 tok/char,
    ASCII ≈ 1 tok/3.5 chars. Deliberately conservative (slightly over-counts)."""
    if not text:
        return 0
    cjk = sum(1 for ch in text if "\u4e00" <= ch <= "\u9fff")
    return int(cjk + (len(text) - cjk) / 3.5) + 1


def _msg_tokens(m: dict[str, Any]) -> int:
    t = _estimate_tokens(m.get("content"))
    for tc in (m.get("tool_calls") or []):
        fn = tc.get("function", {}) or {}
        t += _estimate_tokens(str(fn.get("arguments", ""))) + _estimate_tokens(fn.get("name")) + 8
    return t + 4  # per-message role/formatting overhead


def _safe_tail(messages: list[dict[str, Any]]) -> list[dict[str, Any]]:
    """A conversation must not start with a 'tool' message (it needs the
    assistant tool_call that precedes it)."""
    cut = list(messages)
    while cut and cut[0].get("role") == "tool":
        cut = cut[1:]
    return cut


def _chat_plain(cfg: dict[str, Any], messages: list[dict[str, Any]],
                max_tokens: int = 1200) -> str:
    """A no-tools completion (used for history compression)."""
    prov = _active_provider(cfg)
    if not prov:
        return ""
    url = prov["base_url"].rstrip("/") + "/chat/completions"
    body = {"model": prov["model"], "messages": messages,
            "temperature": 0.2, "max_tokens": max_tokens}
    req = urllib.request.Request(url, data=json.dumps(body).encode("utf-8"),
        method="POST", headers={"Content-Type": "application/json",
        "Authorization": "Bearer " + prov.get("api_key", "")})
    with urllib.request.urlopen(req, timeout=120) as resp:
        payload = json.loads(resp.read().decode("utf-8"))
    usage = payload.get("usage") or {}
    STATE.stats["llm_calls"] += 1
    STATE.stats["prompt_tokens"] += int(usage.get("prompt_tokens", 0) or 0)
    STATE.stats["completion_tokens"] += int(usage.get("completion_tokens", 0) or 0)
    return payload["choices"][0]["message"].get("content") or ""


def _chat_stream_plain(cfg: dict[str, Any], messages: list[dict[str, Any]],
                       max_tokens: int = 1500, phase: str = "thinking") -> str:
    """No-tools STREAMING completion. Streams reasoning+content into STATE.live
    (so the web UI shows the thinking live) and returns the assembled text.
    Used by the multi-round deep-think loop."""
    prov = _active_provider(cfg)
    if not prov:
        return ""
    url = prov["base_url"].rstrip("/") + "/chat/completions"
    body = {"model": prov["model"], "messages": messages, "temperature": 0.4,
            "max_tokens": max_tokens, "stream": True,
            "stream_options": {"include_usage": True}}
    req = urllib.request.Request(url, data=json.dumps(body).encode("utf-8"),
        method="POST", headers={"Content-Type": "application/json",
        "Authorization": "Bearer " + prov.get("api_key", "")})
    parts: list[str] = []
    usage: dict[str, Any] = {}
    STATE.stream_update(phase=phase)
    try:
        with urllib.request.urlopen(req, timeout=180) as resp:
            for raw in resp:
                line = raw.decode("utf-8", "replace").strip()
                if not line or line.startswith(":"):
                    continue
                if line.startswith("data:"):
                    line = line[5:].strip()
                if line == "[DONE]":
                    break
                try:
                    o = json.loads(line)
                except Exception:  # noqa: BLE001
                    continue
                if o.get("usage"):
                    usage = o["usage"]
                ch = o.get("choices") or []
                if not ch:
                    continue
                d = ch[0].get("delta") or {}
                rc = d.get("reasoning_content")
                if rc:
                    STATE.stream_update(reasoning=rc, phase=phase)
                c = d.get("content")
                if c:
                    parts.append(c)
                    STATE.stream_update(content=c, phase=phase)
    except Exception as e:  # noqa: BLE001
        return "(深度思考此轮失败: %s)" % e
    STATE.stats["llm_calls"] += 1
    STATE.stats["prompt_tokens"] += int(usage.get("prompt_tokens", 0) or 0)
    STATE.stats["completion_tokens"] += int(usage.get("completion_tokens", 0) or 0)
    return "".join(parts).strip()


# Guided deliberation steps. Each round gets a DIFFERENT prompt so the model is
# walked through real reasoning instead of one shallow pass. The last step is
# always the conclusion.
THINK_STEPS: list[tuple[str, str]] = [
    ("看清现状", "只做第一步:看清现状。把与当前决策相关的事实列清楚——机器资源(GPU/磁盘/进程数)、各 run 的指标趋势、历史台账(query_gens 印象)、当前红线约束。先不要下结论、不要给动作。"),
    ("找原因与可能性", "第二步:基于上面的现状,分析可能的原因、风险与机会,提出 2-4 个候选方向(含大致参数/旋钮方向)。先不要定。"),
    ("权衡取舍", "第三步:逐个权衡候选方向的收益/风险/对红线与资源的影响,回忆 knowledge 里的已知坑排除会踩坑的,缩小到最优的 1-2 个。"),
    ("定方案", "最后一步:给出你倾向执行的具体动作与参数,以及一句话理由。这是内部结论,先用文字说清,不要在这一步调用工具。"),
]
_THINK_DEEPEN = ("再深入", "在前面思考的基础上再深入一层:有没有被忽略的风险、更优的参数、或可借鉴的历史教训?")


def _build_think_sequence(rounds: int) -> list[tuple[str, str]]:
    rounds = max(1, min(int(rounds), 6))
    concl = THINK_STEPS[-1]
    explore = THINK_STEPS[:-1]
    if rounds == 1:
        return [concl]
    need = rounds - 1
    chosen = explore[:need]
    while len(chosen) < need:
        chosen.append(_THINK_DEEPEN)
    return chosen + [concl]


def _deliberate(cfg: dict[str, Any], base_msgs: list[dict[str, Any]],
                rounds: int) -> list[dict[str, Any]]:
    """Run multi-round guided thinking. Each round has its own context (prior
    thoughts + a step-specific prompt). Rounds are EPHEMERAL: they are streamed
    to the UI and logged to the timeline, but NOT added to STATE.messages, so
    they do not linger in the rolling context (just like normal reasoning).
    Returns list of {"round": i, "total": N, "label": str, "text": str}."""
    seq = _build_think_sequence(rounds)
    results: list[dict[str, Any]] = []
    prior_texts: list[str] = []
    for i, (label, prompt) in enumerate(seq, 1):
        msgs = list(base_msgs)
        for t in prior_texts:
            msgs.append({"role": "assistant", "content": t})
        msgs.append({"role": "user",
                     "content": f"【内部深度思考 第{i}/{len(seq)}步·{label}】(这是你的内部推演,不是给用户的回复,也不要调用工具)\n{prompt}"})
        STATE.stream_reset()
        txt = _chat_stream_plain(cfg, msgs, max_tokens=int(cfg.get("max_tokens", 2000)),
                                 phase=f"think:{i}/{len(seq)}·{label}")
        STATE.stream_done()
        txt = (txt or "").strip()
        prior_texts.append(txt)
        entry = {"round": i, "total": len(seq), "label": label, "text": txt}
        results.append(entry)
        with STATE.lock:
            STATE.live.setdefault("deliberation", []).append(entry)
        _timeline("deep_think", **entry)
    return results


def _serialize_block(msgs: list[dict[str, Any]]) -> str:
    out = []
    for m in msgs:
        role = m.get("role", "?")
        content = (m.get("content") or "")
        if len(content) > 900:
            content = content[:900] + "…(截断)"
        line = f"[{role}] {content}".strip()
        for tc in (m.get("tool_calls") or []):
            fn = tc.get("function", {}) or {}
            line += f"\n  ↳调用 {fn.get('name')}({str(fn.get('arguments',''))[:300]})"
        out.append(line)
    return "\n".join(out)


def _maybe_compress(cfg: dict[str, Any]) -> None:
    """Keep the request under context_token_budget. The system prompt (role,
    red lines, decision tree, objective, pinned specs, rolling summary) is
    ALWAYS kept and never compressed; only the oldest *dialogue* messages get
    folded into the rolling summary."""
    budget = int(cfg.get("context_token_budget", 600000))
    headroom = int(cfg.get("max_tokens", 2000)) + 3000
    sys_tokens = _msg_tokens(_system_prompt(cfg))
    avail = budget - sys_tokens - headroom
    msgs = STATE.messages
    total = sum(_msg_tokens(m) for m in msgs)
    if total <= avail or not _active_provider(cfg):
        return

    keep = max(2, int(cfg.get("keep_recent_msgs", 14)))
    # Compress in chunks until we're back under budget (cap iterations to be safe).
    for _ in range(6):
        if len(STATE.messages) <= keep:
            break
        if sum(_msg_tokens(m) for m in STATE.messages) <= avail:
            break
        split = len(STATE.messages) - keep
        # never cut between an assistant tool_call and its tool replies
        while split < len(STATE.messages) and STATE.messages[split].get("role") == "tool":
            split += 1
        old = STATE.messages[:split]
        recent = _safe_tail(STATE.messages[split:])
        if not old:
            break
        prev = cfg.get("rolling_summary", "")
        sys = {"role": "system", "content":
               "你是历史压缩器。把下面这段训练管家的对话/动作历史压成要点(中文,"
               "≤1200字):保留所有已执行动作及其参数、结果/指标、踩过的坑、未决问题、"
               "需长期记住的结论。不要遗漏关键数字。"}
        user = {"role": "user", "content":
                (f"已有摘要:\n{prev}\n\n" if prev else "")
                + "新增需要并入摘要的历史:\n" + _serialize_block(old)}
        try:
            summary = _chat_plain(cfg, [sys, user], max_tokens=1500)
        except Exception as e:  # noqa: BLE001
            _timeline("error", text=f"compress failed: {e}")
            break
        if not summary:
            break
        cfg["rolling_summary"] = summary.strip()
        STATE.messages = recent
        _save_config(cfg)
        _timeline("compress", dropped=len(old), kept=len(recent),
                  summary_chars=len(cfg["rolling_summary"]))
        sys_tokens = _msg_tokens(_system_prompt(cfg))
        avail = budget - sys_tokens - headroom


# --------------------------------------------------------------------------- #
# The agent loop (background thread). One LLM "step" per active iteration.
# --------------------------------------------------------------------------- #
def _do_step(cfg: dict[str, Any]) -> None:
    # Drain queued user messages into the conversation.
    if STATE.user_inbox:
        joined = "\n".join(STATE.user_inbox)
        STATE.user_inbox = []
        STATE.messages.append({"role": "user", "content": "[用户]: " + joined})
        _timeline("user_msg", text=joined)

    # Always give the model a fresh, compact observation as context.
    obs = _gather_observation()
    STATE.messages.append({"role": "user",
                           "content": "[现状快照]\n" + json.dumps(obs, ensure_ascii=False)})

    # Context budget: fold oldest dialogue into the rolling summary if needed.
    # System prompt + pinned specs are always present and never compressed.
    _maybe_compress(cfg)
    base_msgs = [_system_prompt(cfg)] + _safe_tail(STATE.messages)

    # Deep thinking is now agent-initiated: the model calls `deep_think` tool
    # when it decides a decision is complex enough. The slider sets the max cap.
    msgs = base_msgs
    try:
        reply = _chat(cfg, msgs)
    except urllib.error.HTTPError as e:
        detail = e.read().decode("utf-8", "ignore")[:300]
        STATE.last_error = f"LLM HTTP {e.code}: {detail}"
        _timeline("error", text=STATE.last_error)
        STATE.status = "sleeping"
        STATE.sleep_until = time.time() + 120
        STATE.sleep_reason = "LLM 调用失败,稍后重试"
        STATE.wake_event.clear()
        return
    except Exception as e:  # noqa: BLE001
        STATE.last_error = f"LLM error: {e}"
        _timeline("error", text=STATE.last_error)
        STATE.status = "sleeping"
        STATE.sleep_until = time.time() + 120
        STATE.sleep_reason = "LLM 调用异常,稍后重试"
        STATE.wake_event.clear()
        return

    # Record assistant message (content + any tool calls).
    assistant_msg: dict[str, Any] = {"role": "assistant",
                                      "content": reply.get("content") or ""}
    tool_calls = reply.get("tool_calls") or []
    if tool_calls:
        assistant_msg["tool_calls"] = tool_calls
    STATE.messages.append(assistant_msg)
    # Deep-think rounds belong to the FINAL reply so they render in one bubble.
    # Intermediate tool-calling turns don't carry deliberation; we keep it
    # pending until the model emits its final (no-tool-call) reply.
    final = not tool_calls
    delib = STATE._last_deliberation if final else []
    content = reply.get("content") or ""
    reasoning = reply.get("reasoning_content") or ""
    if content or reasoning or delib:
        _timeline("assistant", text=content, reasoning=reasoning,
                  deliberation=delib if delib else None)
    if final:
        STATE._last_deliberation = []

    if not tool_calls:
        # No action proposed -> sleep to conserve credits. When the box is idle
        # (no training running) there is nothing to monitor, so sleep long
        # instead of waking every default_sleep_min; a user message / manual
        # wake / critical event brings it back instantly.
        try:
            import server
            n_train = int(server.processes().get("total", 0) or 0)
        except Exception:  # noqa: BLE001
            n_train = 0
        idle = n_train == 0
        mins = int(cfg.get("max_sleep_min", 240) if idle
                   else cfg["default_sleep_min"])
        STATE.status = "sleeping"
        STATE.sleep_until = time.time() + mins * 60
        STATE.sleep_reason = ("空闲无训练,长睡省额度" if idle
                              else "模型未提动作,默认休眠")
        STATE.wake_event.clear()
        _timeline("sleep", minutes=mins,
                  reason="auto idle (no training)" if idle else "auto (no tool call)")
        return

    gated = set(cfg.get("gated_tools", []))
    for tc in tool_calls:
        fn = tc.get("function", {})
        name = fn.get("name", "")
        try:
            args = json.loads(fn.get("arguments") or "{}")
        except Exception:  # noqa: BLE001
            args = {}
        tcid = tc.get("id", "")

        # REQUEST mode: gated tool -> queue for human approval, pause.
        if cfg.get("mode") == "request" and name in gated:
            pend = {"id": f"act-{int(time.time()*1000)}-{len(STATE.pending)}",
                    "tool": name, "args": args, "reason": args.get("reason", ""),
                    "tool_call_id": tcid, "proposed_at": time.time()}
            STATE.pending.append(pend)
            STATE.messages.append({"role": "tool", "tool_call_id": tcid,
                                   "content": "QUEUED:已生成请求卡,等待人工批准;你已暂停。"})
            _timeline("request", tool=name, args=args, reason=args.get("reason", ""),
                      action_id=pend["id"])
            STATE.status = "waiting_approval"
            return  # stop stepping until approval

        # Otherwise execute now (auto mode, or non-gated tool).
        if name not in READ_TOOLS:
            _timeline("decision", tool=name, args=args, reason=args.get("reason", ""),
                      mode=cfg.get("mode"))
        result = _execute_tool(name, args, cfg)
        STATE.messages.append({"role": "tool", "tool_call_id": tcid,
                               "content": json.dumps(result, ensure_ascii=False)})
        if name not in READ_TOOLS:
            _timeline("executed", tool=name, result=result)
        if name == "sleep":
            return  # sleep already set status

    # If we executed actions (not sleep), loop again next iteration immediately.
    if STATE.status not in ("sleeping", "waiting_approval"):
        STATE.status = "active"


def _critical_wake(cfg: dict[str, Any]) -> Optional[str]:
    """Cheap, no-LLM check for events that should wake a sleeping agent."""
    import server
    reason = None
    try:
        st = server.system_status()
        disk = st.get("disk") or {}
        if disk.get("free_gb") is not None and disk["free_gb"] < cfg["disk_low_gb"]:
            return f"磁盘告警:剩 {disk['free_gb']}G"
        procs = server.processes()
        base = STATE._watch_baseline
        cur_total = procs.get("total", 0)
        if "proc_total" in base and cur_total < base["proc_total"]:
            reason = f"训练进程减少({base['proc_total']}→{cur_total}),可能 run 结束"
        base["proc_total"] = cur_total
        # extinction / error scan on the freshest PE log
        for p in sorted(server.PE_ROOT.glob("onepot_sD_*.log"),
                        key=lambda x: x.stat().st_mtime, reverse=True)[:3]:
            try:
                tail = p.read_text(encoding="utf-8", errors="ignore").splitlines()[-60:]
            except Exception:  # noqa: BLE001
                continue
            if any("extinct" in ln.lower() for ln in tail):
                return f"检测到灭绝事件:{p.name}"
    except Exception:  # noqa: BLE001
        pass
    return reason


def _runner() -> None:
    while STATE.running:
        cfg = _load_config()
        st = STATE.status
        if st == "waiting_approval":
            time.sleep(0.5)
            continue
        if st == "sleeping":
            # auto-disk-protection even while asleep
            crit = _critical_wake(cfg)
            now = time.time()
            if crit or now >= STATE.sleep_until or STATE.wake_event.is_set() or STATE.user_inbox:
                STATE.stats["wakes"] += 1
                why = crit or ("用户消息" if STATE.user_inbox else
                               ("手动唤醒" if STATE.wake_event.is_set() else "到点"))
                STATE.wake_event.clear()
                # disk emergency: auto-cleanup before handing back to the model
                if crit and "磁盘" in crit:
                    res = _execute_tool("cleanup_disk", {"project": "pe",
                                        "reason": "磁盘告警自动清理"}, cfg)
                    _timeline("auto_cleanup", trigger=crit, result=res)
                _timeline("wake", reason=why)
                STATE.messages.append({"role": "user",
                                       "content": f"[唤醒] 原因: {why}"})
                STATE.status = "active"
                STATE._deliberated_this_wake = False
                STATE._last_deliberation = []
                with STATE.lock:
                    STATE.live["deliberation"] = []
            else:
                time.sleep(min(cfg["poll_interval_s"], 5))
            continue
        if st in ("active", "error"):
            try:
                _do_step(cfg)
            except Exception as e:  # noqa: BLE001
                STATE.last_error = f"step crash: {e}"
                _timeline("error", text=STATE.last_error)
                STATE.status = "sleeping"
                STATE.sleep_until = time.time() + 60
            time.sleep(1.0)
            continue
        time.sleep(0.5)


def _ensure_thread() -> None:
    if STATE.thread and STATE.thread.is_alive():
        return
    STATE.thread = threading.Thread(target=_runner, daemon=True)
    STATE.thread.start()


# --------------------------------------------------------------------------- #
# Request models + routes.
# --------------------------------------------------------------------------- #
class ProviderReq(BaseModel):
    id: Optional[str] = None
    name: str
    base_url: str
    api_key: str
    model: str


class ActivateReq(BaseModel):
    id: str


class ModeReq(BaseModel):
    mode: str


class ConfigReq(BaseModel):
    max_procs: Optional[int] = None
    disk_low_gb: Optional[float] = None
    default_sleep_min: Optional[int] = None
    objective: Optional[str] = None
    gated_tools: Optional[list[str]] = None
    deep_think_rounds: Optional[int] = None
    active_strategy: Optional[str] = None


class StartReq(BaseModel):
    objective: Optional[str] = None


class MessageReq(BaseModel):
    text: str


class ApprovalReq(BaseModel):
    id: str
    reason: Optional[str] = ""


def register_agent(app: Any) -> None:
    # Restore chat + dialogue context from the on-disk timeline so reopening the
    # page after a service restart looks exactly like before.
    _rehydrate()

    @app.get("/api/agent/state")
    def agent_state() -> dict[str, Any]:
        cfg = _load_config()
        return STATE.snapshot(cfg)

    @app.get("/api/agent/timeline")
    def agent_timeline(limit: int = 120) -> dict[str, Any]:
        return {"events": list(_TIMELINE_RING)[-limit:]}

    @app.get("/api/agent/models")
    def agent_models() -> dict[str, Any]:
        cfg = _load_config()
        return {"providers": _redact_providers(cfg),
                "active_provider": cfg.get("active_provider")}

    @app.post("/api/agent/models")
    def agent_add_model(req: ProviderReq) -> dict[str, Any]:
        cfg = _load_config()
        pid = req.id or re.sub(r"[^a-zA-Z0-9]+", "-", req.name.lower()).strip("-") or \
            f"m{int(time.time())}"
        prov = {"id": pid, "name": req.name, "base_url": req.base_url,
                "api_key": req.api_key, "model": req.model}
        provs = [p for p in cfg.get("providers", []) if p.get("id") != pid]
        provs.append(prov)
        cfg["providers"] = provs
        if not cfg.get("active_provider"):
            cfg["active_provider"] = pid
        _save_config(cfg)
        _timeline("model_added", id=pid, name=req.name, model=req.model)
        return {"ok": True, "id": pid, "providers": _redact_providers(cfg)}

    @app.delete("/api/agent/models/{pid}")
    def agent_del_model(pid: str) -> dict[str, Any]:
        cfg = _load_config()
        cfg["providers"] = [p for p in cfg.get("providers", []) if p.get("id") != pid]
        if cfg.get("active_provider") == pid:
            cfg["active_provider"] = (cfg["providers"][0]["id"]
                                      if cfg["providers"] else None)
        _save_config(cfg)
        return {"ok": True, "providers": _redact_providers(cfg)}

    @app.post("/api/agent/models/activate")
    def agent_activate_model(req: ActivateReq) -> dict[str, Any]:
        cfg = _load_config()
        if not any(p.get("id") == req.id for p in cfg.get("providers", [])):
            raise HTTPException(404, "provider not found")
        cfg["active_provider"] = req.id
        _save_config(cfg)
        return {"ok": True, "active_provider": req.id}

    @app.post("/api/agent/models/test")
    def agent_test_model(req: ActivateReq) -> dict[str, Any]:
        cfg = _load_config()
        # temporarily target the requested provider
        save = cfg.get("active_provider")
        cfg["active_provider"] = req.id
        try:
            msg = _chat(cfg, [{"role": "user", "content": "ping(只回 pong)"}])
            return {"ok": True, "reply": (msg.get("content") or "")[:200]}
        except urllib.error.HTTPError as e:
            return {"ok": False, "error": f"HTTP {e.code}: "
                    + e.read().decode('utf-8', 'ignore')[:300]}
        except Exception as e:  # noqa: BLE001
            return {"ok": False, "error": str(e)}
        finally:
            cfg["active_provider"] = save

    @app.post("/api/agent/mode")
    def agent_set_mode(req: ModeReq) -> dict[str, Any]:
        if req.mode not in ("request", "auto"):
            raise HTTPException(400, "mode must be 'request' or 'auto'")
        cfg = _load_config()
        cfg["mode"] = req.mode
        _save_config(cfg)
        _timeline("mode", mode=req.mode)
        return {"ok": True, "mode": req.mode}

    @app.post("/api/agent/config")
    def agent_set_config(req: ConfigReq) -> dict[str, Any]:
        cfg = _load_config()
        for k in ("max_procs", "disk_low_gb", "default_sleep_min", "objective",
                  "gated_tools", "deep_think_rounds"):
            v = getattr(req, k)
            if v is not None:
                cfg[k] = v
        if req.active_strategy is not None:
            sid = req.active_strategy or None
            if sid and sid not in STRATEGIES:
                raise HTTPException(400, f"unknown strategy: {sid}")
            cfg["active_strategy"] = sid
        _save_config(cfg)
        return {"ok": True, "config": {k: cfg[k] for k in
                ("max_procs", "disk_low_gb", "default_sleep_min", "objective",
                 "gated_tools", "deep_think_rounds", "active_strategy")}}

    @app.post("/api/agent/start")
    def agent_start(req: StartReq) -> dict[str, Any]:
        cfg = _load_config()
        if not _active_provider(cfg):
            raise HTTPException(400, "请先注册并激活一个模型")
        if req.objective:
            cfg["objective"] = req.objective
            _save_config(cfg)
        with STATE.lock:
            if not STATE.messages:
                STATE.messages.append({"role": "user", "content":
                    f"开始值守。目标:{cfg.get('objective','')}。"
                    f"先 get_status 看现状,再决定要不要开训或休眠。"})
            STATE.running = True
            STATE.status = "active"
            STATE.last_error = ""
            STATE._deliberated_this_wake = False
            STATE._last_deliberation = []
            with STATE.lock:
                STATE.live["deliberation"] = []
        _ensure_thread()
        _timeline("agent_start", objective=cfg.get("objective"))
        return {"ok": True, "status": STATE.status}

    @app.post("/api/agent/stop")
    def agent_stop() -> dict[str, Any]:
        STATE.running = False
        STATE.status = "stopped"
        _timeline("agent_stop")
        return {"ok": True}

    @app.post("/api/agent/wake")
    def agent_wake() -> dict[str, Any]:
        STATE.wake_event.set()
        return {"ok": True}

    @app.post("/api/agent/message")
    def agent_message(req: MessageReq) -> dict[str, Any]:
        STATE.user_inbox.append(req.text)
        STATE.wake_event.set()
        _timeline("user_msg_queued", text=req.text)
        # if agent never started, start it so the user can chat immediately
        cfg = _load_config()
        if not STATE.running and _active_provider(cfg):
            STATE.running = True
            STATE.status = "active"
            _ensure_thread()
        return {"ok": True}

    @app.post("/api/agent/approve")
    def agent_approve(req: ApprovalReq) -> dict[str, Any]:
        cfg = _load_config()
        pend = next((p for p in STATE.pending if p["id"] == req.id), None)
        if not pend:
            raise HTTPException(404, "no such pending action")
        STATE.pending = [p for p in STATE.pending if p["id"] != req.id]
        _timeline("decision", tool=pend["tool"], args=pend["args"],
                  reason=pend.get("reason", ""), approved=True)
        result = _execute_tool(pend["tool"], pend["args"], cfg)
        _timeline("executed", tool=pend["tool"], result=result, approved=True)
        STATE.messages.append({"role": "user", "content":
            f"[人工已批准 {pend['tool']}] 执行结果: "
            + json.dumps(result, ensure_ascii=False)})
        STATE.status = "active"
        return {"ok": True, "result": result}

    @app.post("/api/agent/reject")
    def agent_reject(req: ApprovalReq) -> dict[str, Any]:
        pend = next((p for p in STATE.pending if p["id"] == req.id), None)
        if not pend:
            raise HTTPException(404, "no such pending action")
        STATE.pending = [p for p in STATE.pending if p["id"] != req.id]
        _timeline("rejected", tool=pend["tool"], args=pend["args"],
                  reason=req.reason or "")
        STATE.messages.append({"role": "user", "content":
            f"[人工否决 {pend['tool']}] 理由: {req.reason or '(未填)'}。"
            f"请换个方案或休眠。"})
        STATE.status = "active"
        return {"ok": True}

    @app.post("/api/agent/clear_context")
    def agent_clear_context() -> dict[str, Any]:
        """Manually wipe the rolling dialogue context. The system prompt
        (role / red lines / strategy catalog / objective), pinned specs, the
        knowledge base and the gen ledger are ALL kept — only the chat history
        and rolling summary are dropped. The agent still knows the project."""
        cfg = _load_config()
        n = len(STATE.messages)
        with STATE.lock:
            STATE.messages = []
            STATE.user_inbox = []
            STATE.live["deliberation"] = []
        STATE._last_deliberation = []
        STATE._deliberated_this_wake = False
        if cfg.get("rolling_summary"):
            cfg["rolling_summary"] = ""
            _save_config(cfg)
        _timeline("context_cleared", dropped=n)
        return {"ok": True, "dropped": n}
