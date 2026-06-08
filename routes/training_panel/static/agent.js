"use strict";
/* Agent tab — chat, mode toggle, approval cards, decision timeline, model
 * registry. Self-attaches; reuses global api()/toast() from app.js. */
(function () {
  const A = (s) => document.querySelector(s);
  const AA = (s) => Array.from(document.querySelectorAll(s));
  const esc = (s) => String(s == null ? "" : s)
    .replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;");
  const apost = (p, body) => api(p, {
    method: "POST", headers: { "Content-Type": "application/json" },
    body: JSON.stringify(body || {}),
  });
  const tnote = (m, e) => (typeof toast === "function") ? toast(m, e) : 0;

  let timer = null;
  let started = false;
  let lastLive = null;       // latest STATE.live snapshot
  let lastChatSig = "";      // signature to avoid rebuilding committed chat every poll
  let lastPendingIds = new Set(); // request action_ids still awaiting human confirm
  let stratLoaded = false;   // strategy <select> populated once
  let dtDirty = false;       // user is dragging deep-think slider

  function fmtClock(ts) {
    if (!ts) return "";
    const d = new Date(ts * 1000);
    return d.toLocaleTimeString("zh-CN", { hour12: false });
  }

  /* ----------------------------- state ----------------------------- */
  async function refreshState() {
    let s;
    try { s = await api("/api/agent/state"); }
    catch (e) { A("#agStatus").textContent = "状态读取失败: " + e.message; return; }

    // badge + status line
    const badge = A("#agentBadge");
    const map = { active: "运行", sleeping: "休眠", waiting_approval: "待批",
                  stopped: "停止", error: "错误" };
    if (badge) badge.textContent = map[s.status] || s.status;

    // segmented status line: each piece is a labelled chip, laid out with flex
    const seg = (k, v, cls) =>
      `<span class="st-seg"><span class="st-k">${k}</span>` +
      `<span class="st-v ${cls || ""}">${v}</span></span>`;
    const dotCls = { active: "on", sleeping: "sleep", waiting_approval: "wait" }[s.status] || "";
    const st = s.stats || {};
    const cx = s.context || {};
    const dt = s.deep_think_rounds || 0;
    const stratName = s.active_strategy_name || (s.active_strategy || "");
    const k = (n) => Math.round((n || 0) / 1000) + "k";
    const parts = [];
    parts.push(`<span class="st-seg"><span class="st-dot ${dotCls}"></span>` +
      `<span class="st-v">${esc(map[s.status] || s.status)}</span></span>`);
    parts.push(seg("模式", s.mode === "auto" ? "全自动" : "请求"));
    parts.push(seg("模型", esc(s.active_model || "—")));
    if (s.status === "sleeping" && s.sleep_until) {
      const left = Math.max(0, Math.round((s.sleep_until - s.now) / 60));
      parts.push(seg("休眠", `剩~${left}分`, "st-dt"));
    }
    parts.push(seg("调用", `${st.llm_calls || 0}·${(st.prompt_tokens||0)+(st.completion_tokens||0)}tok`));
    parts.push(seg("动作", st.actions || 0));
    if (cx.budget) {
      let ctx = `${k(cx.used_est)}/${k(cx.budget)}`;
      if (cx.pinned_specs) ctx += ` 钉${cx.pinned_specs}`;
      if (cx.has_summary) ctx += " 压缩";
      parts.push(seg("上下文", ctx));
    }
    parts.push(seg("上限", `${s.max_procs}进程·盘<${s.disk_low_gb}G`));
    parts.push(seg("策略", esc(stratName || "未定"), "st-strat"));
    parts.push(seg("深思", dt ? dt + "轮" : "禁", "st-dt"));
    if (s.last_error) parts.push(seg("错误", esc(s.last_error), "st-err"));
    A("#agStatus").innerHTML = parts.join("");

    // stash live for the streaming bubble
    lastLive = s.live || null;

    // populate strategy selector once, then keep its value synced
    const strSel = A("#agStrategy");
    if (strSel && !stratLoaded && (s.strategies || []).length) {
      strSel.innerHTML = `<option value="">（由 agent 自选）</option>` +
        s.strategies.map((x) => `<option value="${esc(x.id)}">${esc(x.name)} · ${esc(x.when)}</option>`).join("");
      stratLoaded = true;
    }
    if (strSel && document.activeElement !== strSel)
      strSel.value = s.active_strategy || "";

    // deep-think slider (don't fight the user while dragging)
    const dtR = A("#agDeepThink"), dtV = A("#agDeepThinkVal");
    if (dtR && !dtDirty) { dtR.value = String(dt); if (dtV) dtV.textContent = dt ? dt + " 轮" : "禁"; }

    // mode segmented active
    AA("#agModeSeg .seg-btn").forEach((b) =>
      b.classList.toggle("active", b.dataset.mode === s.mode));

    // pending approvals are now rendered INLINE in the chat (see approvalCard);
    // keep the legacy standalone card hidden, and just track which request
    // action_ids are still actionable so the chat can show live cards.
    const pc = A("#agPendingCard"), pd = A("#agPending");
    if (pc) pc.hidden = true;
    if (pd) pd.innerHTML = "";
    const newIds = new Set((s.pending || []).map((p) => p.id));
    const changed = newIds.size !== lastPendingIds.size ||
      [...newIds].some((id) => !lastPendingIds.has(id)) ||
      [...lastPendingIds].some((id) => !newIds.has(id));
    lastPendingIds = newIds;
    if (changed) lastChatSig = "";  // force chat rebuild so cards appear/clear
  }

  /* --------------------------- timeline/chat ------------------------ */
  function metaRow(who, ts) {
    return `<div class="cb-meta"><span class="cb-who">${esc(who)}</span>` +
           `<span class="cb-time">${fmtClock(ts)}</span></div>`;
  }
  // render deep-think rounds as collapsed sections (history: folded by default)
  function delibBlock(rounds, openAll) {
    if (!rounds || !rounds.length) return "";
    const op = openAll ? " open" : "";
    return rounds.map((d) =>
      `<details class="cb-think cb-delib"${op}><summary>深度推演 ${d.round}/${d.total}·${esc(d.label || "")}</summary>
       <div class="cb-think-body">${esc(d.text || "")}</div></details>`).join("");
  }
  // brief one-line arg preview for a tool chip (config/file/seed/reason…)
  function argBrief(a) {
    if (!a || typeof a !== "object") return "";
    const keys = ["config", "file", "files", "project", "checkpoint", "resume_checkpoint",
                  "seed", "generations", "steps", "strategy", "names", "query", "rounds"];
    const bits = [];
    for (const k of keys) {
      if (a[k] == null || a[k] === "") continue;
      let v = Array.isArray(a[k]) ? a[k].join(",") : String(a[k]);
      if (v.length > 28) v = v.slice(0, 27) + "…";
      bits.push(`${k}=${v}`);
      if (bits.length >= 3) break;
    }
    return bits.length ? ` <span class="tc-arg">${esc(bits.join(" "))}</span>` : "";
  }
  // chips for the tools a model invoked in one assistant turn (intent; incl. reads)
  function toolCallChips(tcs) {
    if (!tcs || !tcs.length) return "";
    return `<div class="cb-tools">` + tcs.map((t) =>
      `<details class="tool-chip"><summary><span class="tc-ic">⚙</span>${esc(t.name)}${argBrief(t.args)}</summary>
       <pre class="tc-args">${esc(JSON.stringify(t.args || {}, null, 1))}</pre></details>`).join("") + `</div>`;
  }
  // inline, actionable approval card rendered directly in the chat stream when a
  // gated decision is awaiting human confirmation. Shows params / reason / goal
  // with 确认 / 驳回(+理由). Buttons are handled via delegation on #agChat.
  function approvalCard(ev) {
    const a = ev.args || {};
    const PK = [["config","配置"],["seed","seed"],["generations","代数"],
                ["steps","步数"],["resume_checkpoint","续跑"],["project","项目"],
                ["strategy","策略"]];
    const params = PK.filter(([k]) => a[k] != null && a[k] !== "")
      .map(([k, lab]) => `<span class="ac-kv"><i>${esc(lab)}</i>${esc(String(a[k]))}</span>`)
      .join("");
    const purpose = a.purpose || "";
    return `<div class="approve-card ac-inline" data-id="${esc(ev.action_id)}">
      <div class="ac-head"><span class="ac-badge">待确认</span><b>${esc(ev.tool)}</b>
        <span class="ac-time">${fmtClock(ev.ts)}</span></div>
      ${params ? `<div class="ac-params">${params}</div>` : ""}
      <div class="ac-reason"><i>理由</i>${esc(ev.reason || a.reason || "(未填)")}</div>
      ${purpose ? `<div class="ac-purpose"><i>目标</i>${esc(purpose)}</div>` : ""}
      <div class="ac-actions">
        <input class="ac-rej" placeholder="驳回理由（可选，会回灌给 agent 重想）" />
        <button class="primary mini ac-ok">确认执行</button>
        <button class="danger mini ac-no">驳回</button>
      </div>
    </div>`;
  }
  // inline chip for a tool RESULT / control event (executed / reads / request…)
  function toolResultChip(ev) {
    const tm = `<span class="tc-time">${fmtClock(ev.ts)}</span>`;
    if (ev.kind === "executed") {
      const r = ev.result || {};
      const ok = r.ok !== false && !r.refused;
      return `<div class="tool-result ${ok ? "tr-ok" : "tr-err"}">
        <details><summary><span class="tc-ic">${ok ? "✓" : "✕"}</span>${esc(ev.tool)} → ${esc(shortRes(r))}${tm}</summary>
        <pre class="tc-args">${esc(JSON.stringify(r, null, 1))}</pre></details></div>`;
    }
    if (ev.kind === "parallel_reads")
      return `<div class="tool-result tr-read"><span class="tc-ic">⇶</span>并行只读 ${(ev.tools || []).length} 个：${esc((ev.tools || []).join(", "))}${tm}</div>`;
    if (ev.kind === "request") {
      if (lastPendingIds.has(ev.action_id)) return approvalCard(ev);
      return `<div class="tool-result tr-wait"><span class="tc-ic">⏳</span>请求 ${esc(ev.tool)} — ${esc(ev.reason || "")}${tm}</div>`;
    }
    if (ev.kind === "rejected")
      return `<div class="tool-result tr-err"><span class="tc-ic">✕</span>否决 ${esc(ev.tool)} — ${esc(ev.reason || "")}${tm}</div>`;
    if (ev.kind === "interrupted")
      return `<div class="tool-result tr-int"><span class="tc-ic">⛔</span>已打断（${esc(ev.phase || "")}）— ${esc(ev.reason || "")}${tm}</div>`;
    return "";
  }
  function chatBubble(ev) {
    if (ev.kind === "context_cleared")
      return `<div class="chat-divider">— 已清空上下文（丢弃 ${ev.dropped || 0} 条；系统提示/常驻 spec/知识库保留）—</div>`;
    if (ev.kind === "user_msg" || ev.kind === "user_msg_queued")
      return `<div class="chat-bubble ab-user">${metaRow("你", ev.ts)}
        <div class="cb-text">${esc(ev.text || "")}</div></div>`;
    if (ev.kind === "assistant") {
      // history assistant bubbles fold deep-think + reasoning by default
      const delib = delibBlock(ev.deliberation, false);
      const think = (ev.reasoning || "").trim()
        ? `<details class="cb-think"><summary>💭 思考过程</summary>
           <div class="cb-think-body">${esc(ev.reasoning)}</div></details>` : "";
      const tools = toolCallChips(ev.tool_calls);
      const text = (ev.text || "").trim()
        ? `<div class="cb-text">${esc(ev.text)}</div>` : "";
      return `<div class="chat-bubble ab-assistant">${metaRow("Agent", ev.ts)}
        ${delib}${think}${text}${tools}</div>`;
    }
    // tool result / control events render as inline chips (no bubble chrome)
    return toolResultChip(ev);
  }
  // live streaming bubble (reasoning + reply as they arrive)
  function liveBubble(live) {
    const phaseMap = { thinking: "思考中…", replying: "回复中…", done: "" };
    let phase = live.phase || "";
    if (phase.indexOf("tool:") === 0) phase = "调用工具 " + phase.slice(5) + "…";
    else if (phase.indexOf("think:") === 0) phase = "深度推演 " + phase.slice(6) + "…";
    else phase = phaseMap[phase] || phase;
    // completed deep-think rounds stay in the SAME live bubble (expanded)
    const delib = delibBlock(live.deliberation, true);
    const think = live.reasoning
      ? `<details class="cb-think" open><summary>思考过程</summary>
         <div class="cb-think-body">${esc(live.reasoning)}</div></details>` : "";
    const body = live.content
      ? `<div class="cb-text cb-cursor">${esc(live.content)}</div>`
      : (live.reasoning ? "" : `<div class="cb-phase cb-cursor"></div>`);
    return `<div class="chat-bubble ab-live" id="agLiveBubble">
      ${metaRow("Agent", Date.now() / 1000)}
      ${phase ? `<div class="cb-phase">${esc(phase)}</div>` : ""}
      ${delib}${think}${body}</div>`;
  }

  function histLine(ev) {
    let icon = "·", txt = ev.kind;
    if (ev.kind === "decision") { icon = "◆"; txt = `决策 ${ev.tool} ${ev.approved ? "(已批准)" : ""} — ${ev.reason || ""}`; }
    else if (ev.kind === "executed") { icon = "›"; txt = `执行 ${ev.tool}: ${shortRes(ev.result)}`; }
    else if (ev.kind === "request") { icon = "?"; txt = `请求 ${ev.tool} — ${ev.reason || ""}`; }
    else if (ev.kind === "rejected") { icon = "✕"; txt = `否决 ${ev.tool} — ${ev.reason || ""}`; }
    else if (ev.kind === "sleep") { icon = "z"; txt = `休眠 ${ev.minutes}分 — ${ev.reason || ""}`; }
    else if (ev.kind === "wake") { icon = "•"; txt = `唤醒 — ${ev.reason || ""}`; }
    else if (ev.kind === "note") { icon = "–"; txt = `[${ev.level || "info"}] ${ev.text || ""}`; }
    else if (ev.kind === "spec_pinned") { icon = "*"; txt = `钉常驻记忆(共${ev.count||"?"}): ${ev.text || ""}`; }
    else if (ev.kind === "compress") { icon = "≡"; txt = `上下文压缩: 折叠${ev.dropped}条→摘要(留${ev.kept}条)`; }
    else if (ev.kind === "error") { icon = "!"; txt = ev.text || ""; }
    else if (ev.kind === "auto_cleanup") { icon = "~"; txt = `自动清理(${ev.trigger||""})`; }
    else if (ev.kind === "checkpoint_deleted") { icon = "✕"; txt = `删检查点 删除[${(ev.removed||[]).join(", ")||"无"}]${(ev.skipped&&ev.skipped.length)?` 跳过${ev.skipped.length}个受保护`:""} — ${ev.reason || ""}`; }
    else if (ev.kind === "kb_write") { icon = "+"; txt = `记知识[${ev.node_kind||""}] ${ev.title || ev.id || ""}`; }
    else if (ev.kind === "gen_recorded") { icon = "#"; txt = `记台账 ${ev.project||""}/${ev.lineage||""} gen${ev.gen}`; }
    else if (ev.kind === "strategy") { icon = "▶"; txt = `切策略→${ev.name||ev.strategy}${ev.reason?` — ${ev.reason}`:""}`; }
    else if (ev.kind === "deep_think") { icon = "…"; txt = `深度思考 ${ev.round}/${ev.total}·${ev.label||""}`; }
    else if (ev.kind === "mode") { icon = "⇄"; txt = `切换模式→${ev.mode}`; }
    else if (ev.kind === "context_cleared") { icon = "⊘"; txt = `手动清空上下文（丢弃 ${ev.dropped||0} 条）`; }
    else if (ev.kind === "agent_start") { icon = "▶"; txt = `启动值守: ${ev.objective || ""}`; }
    else if (ev.kind === "agent_stop") { icon = "■"; txt = "停止值守"; }
    else if (ev.kind === "model_added") { icon = "+"; txt = `加模型 ${ev.name}/${ev.model}`; }
    else return "";
    return `<div class="tl-line"><span class="tl-ic">${icon}</span>
      <span class="tl-tm">${fmtClock(ev.ts)}</span>
      <span class="tl-tx">${esc(txt)}</span></div>`;
  }
  function shortRes(r) {
    if (!r) return "";
    if (r.refused) return "被拒: " + (r.reason || "");
    if (r.ok === false) return "失败: " + (r.error || "");
    if (r.run_id) return "run " + r.run_id + (r.pid ? " pid " + r.pid : "");
    if (r.freed_gb != null) return "释放 " + r.freed_gb + "G";
    if (r.sleeping_minutes) return "睡 " + r.sleeping_minutes + "分";
    return "ok";
  }

  async function refreshTimeline() {
    let r;
    try { r = await api("/api/agent/timeline?limit=200"); }
    catch (e) { return; }
    const evs = r.events || [];
    // The chat now mirrors the whole working trace: user msgs, assistant turns
    // (text + collapsible thinking + the tools it invoked), and the tool
    // results / control events (executed, parallel reads, requests, aborts).
    // deep_think stays merged into the assistant bubble via ev.deliberation.
    const TOOL_KINDS = { executed: 1, parallel_reads: 1, request: 1,
                         rejected: 1, interrupted: 1 };
    const chat = evs.filter((e) => {
      if (e.kind === "user_msg" || e.kind === "user_msg_queued" ||
          e.kind === "context_cleared") return true;
      if (e.kind === "assistant")
        return (e.text || "").trim() || (e.reasoning || "").trim() ||
               (e.deliberation && e.deliberation.length) ||
               (e.tool_calls && e.tool_calls.length);
      return !!TOOL_KINDS[e.kind];
    });
    const chatBox = A("#agChat");
    const atBottom = chatBox.scrollHeight - chatBox.scrollTop - chatBox.clientHeight < 80;
    const sig = chat.length + ":" + (chat.length ? chat[chat.length - 1].ts : 0);
    if (sig !== lastChatSig) {
      lastChatSig = sig;
      chatBox.querySelectorAll(".chat-bubble:not(#agLiveBubble)").forEach((n) => n.remove());
      const committed = A("#agChatCommitted") || (() => {
        const d = document.createElement("div"); d.id = "agChatCommitted";
        d.style.display = "contents"; chatBox.prepend(d); return d;
      })();
      committed.innerHTML = chat.length ? chat.map(chatBubble).join("")
        : `<div class="hint" style="padding:10px">还没有对话。启动值守或直接发消息。</div>`;
    }
    // live streaming bubble (always reflect latest STATE.live)
    let liveEl = A("#agLiveBubble");
    if (lastLive && lastLive.active) {
      const html = liveBubble(lastLive);
      if (liveEl) liveEl.outerHTML = html;
      else chatBox.insertAdjacentHTML("beforeend", html);
    } else if (liveEl) { liveEl.remove(); }
    if (atBottom) chatBox.scrollTop = chatBox.scrollHeight;

    const tl = A("#agTimeline");
    const lines = evs.slice().reverse().map(histLine).filter(Boolean).join("");
    tl.innerHTML = lines || `<div class="hint" style="padding:10px">暂无决策记录。</div>`;
  }

  /* ----------------------------- models ---------------------------- */
  async function refreshModels() {
    let r;
    try { r = await api("/api/agent/models"); }
    catch (e) { return; }
    const provs = r.providers || [];
    const sel = A("#agModel");
    sel.innerHTML = provs.length
      ? provs.map((p) => `<option value="${esc(p.id)}" ${p.id === r.active_provider ? "selected" : ""}>${esc(p.name)} (${esc(p.model)})</option>`).join("")
      : `<option value="">未注册</option>`;
    const tb = A("#agModelTable tbody");
    tb.innerHTML = provs.length ? provs.map((p) => `
      <tr>
        <td>${esc(p.name)}${p.id === r.active_provider ? " ·在用" : ""}</td>
        <td>${esc(p.model)}</td>
        <td class="mono small">${esc(p.base_url)}</td>
        <td class="mono small">${esc(p.api_key_masked)}</td>
        <td>
          <button class="ghost mini" data-act="${esc(p.id)}">激活</button>
          <button class="ghost mini" data-test="${esc(p.id)}">测试</button>
          <button class="danger mini" data-del="${esc(p.id)}">删</button>
        </td>
      </tr>`).join("") : `<tr><td colspan="5" class="hint">还没注册任何模型</td></tr>`;
    tb.querySelectorAll("[data-act]").forEach((b) => b.addEventListener("click", async () => {
      try { await apost("/api/agent/models/activate", { id: b.dataset.act });
        tnote("已激活"); refreshModels(); refreshState();
      } catch (e) { tnote("激活失败: " + e.message, true); }
    }));
    tb.querySelectorAll("[data-test]").forEach((b) => b.addEventListener("click", async () => {
      tnote("测试中…");
      try { const x = await apost("/api/agent/models/test", { id: b.dataset.test });
        tnote(x.ok ? ("回复: " + (x.reply || "(空)")) : ("失败: " + x.error), !x.ok);
      } catch (e) { tnote("测试失败: " + e.message, true); }
    }));
    tb.querySelectorAll("[data-del]").forEach((b) => b.addEventListener("click", async () => {
      try { await api("/api/agent/models/" + b.dataset.del, { method: "DELETE" });
        tnote("已删除"); refreshModels(); refreshState();
      } catch (e) { tnote("删除失败: " + e.message, true); }
    }));
  }

  /* ----------------------- live training cards --------------------- */
  let tcTimer = null;
  let cardCache = {};        // run_id|pid -> merged proc+meta (for modal)
  const baseName = (p) => String(p || "").split(/[\\/]/).pop();
  const trunc = (s, n) => { s = String(s == null ? "" : s); return s.length > n ? s.slice(0, n - 1) + "…" : s; };

  async function refreshTrainCards() {
    const grid = A("#agTrainGrid");
    if (!grid) return;
    let procs, runs = [];
    try { procs = await api("/api/processes"); }
    catch (e) { return; }
    try { runs = await api("/api/runs"); } catch (e) {}
    const runMap = {};
    (runs || []).forEach((r) => { runMap[r.run_id] = r; });
    const list = (procs.processes || []);
    cardCache = {};
    if (!list.length) {
      grid.innerHTML = `<div class="hint" style="padding:8px">还没有正在跑的训练。去「工具 ▸ 训练控制」启动，或让 agent 启动。</div>`;
      return;
    }
    grid.innerHTML = list.map((p) => {
      const run = p.run_id ? runMap[p.run_id] : null;
      const meta = (run && run.meta) || {};
      const key = p.run_id || ("pid" + p.pid);
      cardCache[key] = { proc: p, meta };
      const proj = (p.project || "").toUpperCase();
      const title = trunc(p.config || p.cmdline || proj, 34);
      const srcBadge = p.source === "panel"
        ? `<span class="tc-badge tc-panel">面板</span>`
        : `<span class="tc-badge tc-ext">外部</span>`;
      const kindBadge = p.kind === "newworld" ? `<span class="tc-badge">新世界</span>` : "";
      const pp = [];
      if (meta.seed != null) pp.push("seed=" + meta.seed);
      if (meta.generations != null) pp.push("gen=" + meta.generations);
      if (meta.steps != null) pp.push("steps=" + meta.steps);
      if (meta.resume_checkpoint) pp.push("warm");
      const purpose = meta.purpose || meta.reason || "";
      return `<div class="train-card" data-key="${esc(key)}">
        <div class="tcard-top">
          <span class="tcard-proj tcard-${esc(p.project)}">${esc(proj)}</span>
          ${srcBadge}${kindBadge}
          <span class="tcard-dot"></span>
        </div>
        <div class="tcard-title" title="${esc(p.config || p.cmdline || "")}">${esc(title)}</div>
        ${pp.length ? `<div class="tcard-params">${esc(pp.join(" · "))}</div>` : ""}
        ${purpose ? `<div class="tcard-purpose">${esc(trunc(purpose, 60))}</div>` : `<div class="tcard-purpose tcard-noaim">（无目的说明）</div>`}
        <div class="tcard-foot">
          <span>PID ${p.pid}</span>
          <span>${fmtClock ? "" : ""}${esc(fmtDurSafe(p.elapsed_s))}</span>
          ${p.mem_mb != null ? `<span>${Math.round(p.mem_mb)}MB</span>` : ""}
        </div>
      </div>`;
    }).join("");
    grid.querySelectorAll(".train-card").forEach((el) =>
      el.addEventListener("click", () => openTrainModal(el.dataset.key)));
  }
  function fmtDurSafe(s) {
    return (typeof fmtDur === "function") ? fmtDur(s) : Math.round(s || 0) + "s";
  }

  /* --------------------- training detail modal --------------------- */
  let tmCurveTimer = null, tm3dTimer = null, tmKey = null;
  const tmCharts = {};
  const themeAxis = () => {
    const grid = "rgba(140,154,171,.14)", tick = "#8b9aab";
    return { x: { grid: { color: grid }, ticks: { color: tick } },
             y: { grid: { color: grid }, ticks: { color: tick } } };
  };
  function tmMk(id, cfg) {
    if (tmCharts[id]) tmCharts[id].destroy();
    const el = A("#" + id); if (!el || typeof Chart === "undefined") return;
    tmCharts[id] = new Chart(el, cfg);
  }
  function openTrainModal(key) {
    const c = cardCache[key];
    if (!c) return;
    tmKey = key;
    const p = c.proc, meta = c.meta || {};
    const proj = (p.project || "").toUpperCase();
    A("#tmTitle").textContent = `${proj} · ${p.config || p.cmdline || ""}`;
    A("#tmHeadMeta").innerHTML =
      `<span>PID ${p.pid}</span><span>${esc(p.source === "panel" ? "面板启动" : "外部进程")}</span>` +
      `<span>${esc(fmtDurSafe(p.elapsed_s))}</span>`;
    // params / reason / purpose
    renderTmParams(p, meta);
    // open
    A("#trainModal").hidden = false;
    document.body.classList.add("tm-open");
    // curves: load now + poll
    loadTmCurves();
    if (tmCurveTimer) clearInterval(tmCurveTimer);
    tmCurveTimer = setInterval(loadTmCurves, 5000);
    // 3D: load latest world now + optional poll
    tmLoad3d();
    if (tm3dTimer) clearInterval(tm3dTimer);
    if (A("#tm3dAuto").checked) tm3dTimer = setInterval(tmLoad3d, 15000);
  }
  function closeTrainModal() {
    A("#trainModal").hidden = true;
    document.body.classList.remove("tm-open");
    tmKey = null;
    if (tmCurveTimer) { clearInterval(tmCurveTimer); tmCurveTimer = null; }
    if (tm3dTimer) { clearInterval(tm3dTimer); tm3dTimer = null; }
    Object.keys(tmCharts).forEach((k) => { try { tmCharts[k].destroy(); } catch (e) {} delete tmCharts[k]; });
    A("#tm3dFrame").src = "about:blank";
  }
  function renderTmParams(p, meta) {
    const rows = [];
    const add = (k, v) => { if (v != null && v !== "") rows.push([k, v]); };
    add("项目", (p.project || "").toUpperCase());
    add("配置/检查点", p.config);
    add("seed", meta.seed);
    add("generations", meta.generations);
    add("steps", meta.steps);
    add("n_envs", meta.n_envs);
    add("policy", meta.policy);
    add("resume_checkpoint", meta.resume_checkpoint);
    add("PID", p.pid);
    add("来源", p.source === "panel" ? "面板启动" : "外部进程");
    const kv = rows.map(([k, v]) =>
      `<div class="tmp-kv"><span class="tmp-k">${esc(k)}</span><span class="tmp-v mono">${esc(String(v))}</span></div>`).join("");
    const reason = meta.reason
      ? `<div class="tmp-block"><div class="tmp-bh">理由 (为何这么训)</div><div class="tmp-bt">${esc(meta.reason)}</div></div>` : "";
    const purpose = meta.purpose
      ? `<div class="tmp-block"><div class="tmp-bh">目的 (要达成/验证什么)</div><div class="tmp-bt">${esc(meta.purpose)}</div></div>` : "";
    const cmd = p.cmdline
      ? `<div class="tmp-block"><div class="tmp-bh">命令行</div><pre class="tmp-cmd">${esc(p.cmdline)}</pre></div>` : "";
    const note = (!meta.reason && !meta.purpose)
      ? `<div class="hint">该训练不是由 agent 启动（或启动时未写理由/目的），故无理由/目的记录。</div>` : "";
    A("#tmParams").innerHTML = `<div class="tmp-kvs">${kv}</div>${reason}${purpose}${note}${cmd}`;
  }
  async function loadTmCurves() {
    const c = cardCache[tmKey]; if (!c) return;
    const p = c.proc, meta = c.meta || {};
    const info = A("#tmCurveInfo");
    try {
      if (p.project === "pe") {
        const file = baseName(p.log);
        if (!file) { info.textContent = "无日志文件"; return; }
        const m = await api(`/api/pe/curve?file=${encodeURIComponent(file)}`);
        if (!m.n) { info.textContent = "暂无 Gen 行（训练刚开始？）"; return; }
        info.textContent = `${m.n} 代 · 最新 gen ${m.gen[m.gen.length-1]} · avg ${m.avg[m.avg.length-1]}`;
        tmMk("tmChartA", { type: "line", data: { labels: m.gen, datasets: [
          { label: "avg", data: m.avg, borderColor: "#c8a86a", pointRadius: 0, borderWidth: 2 },
          { label: "best", data: m.best, borderColor: "#6fae8f", pointRadius: 0, borderWidth: 1 },
        ]}, options: lineOpts("protagonist avg / best") });
        const total = m.me_total || 96;
        tmMk("tmChartB", { type: "line", data: { labels: m.gen, datasets: [
          { label: `me_cov /${total}`, data: m.me_cov, borderColor: "#7ea6c8", pointRadius: 0, borderWidth: 2, yAxisID: "y" },
          { label: "qd", data: m.qd, borderColor: "#b48ec8", pointRadius: 0, borderWidth: 1, yAxisID: "y1" },
        ]}, options: dualOpts("多样性 me_cov / 质量多样性 qd") });
      } else {
        const file = (meta.metrics) || (baseName(p.config).replace(/\.[^.]+$/, "") + ".jsonl");
        const m = await api(`/api/metrics?project=dp&file=${encodeURIComponent(file)}`);
        if (!m.n) { info.textContent = "暂无指标行"; return; }
        info.textContent = `${m.n} 局 · 最新 reward ${m.reward_smooth.at(-1).toFixed(1)} · 解锁 ${m.unlocks.at(-1)}`;
        const labels = m.reward.map((_, i) => i);
        tmMk("tmChartA", { type: "line", data: { labels, datasets: [
          { label: "reward", data: m.reward, borderColor: "rgba(200,168,106,.35)", pointRadius: 0, borderWidth: 1 },
          { label: "reward(smooth)", data: m.reward_smooth, borderColor: "#c8a86a", pointRadius: 0, borderWidth: 2 },
        ]}, options: lineOpts("reward 曲线") });
        tmMk("tmChartB", { type: "line", data: { labels, datasets: [
          { label: "unlocks", data: m.unlocks, borderColor: "#6fae8f", pointRadius: 0, borderWidth: 2 },
        ]}, options: lineOpts("里程碑解锁数") });
      }
    } catch (e) { info.textContent = "曲线加载失败: " + e.message; }
  }
  function lineOpts(title) {
    return { responsive: true, maintainAspectRatio: false, animation: false,
      plugins: { title: { display: true, text: title, color: "#e8e6e1" },
                 legend: { labels: { color: "#8b9aab" } } }, scales: themeAxis() };
  }
  function dualOpts(title) {
    const a = themeAxis();
    return { responsive: true, maintainAspectRatio: false, animation: false,
      plugins: { title: { display: true, text: title, color: "#e8e6e1" },
                 legend: { labels: { color: "#8b9aab" } } },
      scales: { x: a.x, y: { position: "left", grid: a.y.grid, ticks: a.y.ticks },
                y1: { position: "right", grid: { drawOnChartArea: false }, ticks: { color: "#b48ec8" } } } };
  }
  async function tmLoad3d() {
    const c = cardCache[tmKey]; if (!c) return;
    const p = c.proc;
    const info = A("#tm3dInfo");
    try {
      if (p.project === "pe") {
        const lt = await api("/api/pe/live_trace");
        if (!lt.trace_id) { info.textContent = "暂无 PE trace（训练还没产出可视化帧）"; return; }
        await apost("/api/replay/load", { project: "pe", trace_id: lt.trace_id, downsample: 1 });
        A("#tm3dFrame").src = `/viewer/pe/index.html?t=` + Date.now();
        info.textContent = "最新世界 " + (lt.mtime || "");
      } else {
        const t = await api("/api/traces");
        const dp = (t.dp || []);
        if (!dp.length) { info.textContent = "暂无 DP dump（训练中需 dump 才能渲染；用「新世界」生成）"; return; }
        const newest = dp[dp.length - 1];
        await apost("/api/replay/load", { project: "dp", trace_id: newest.id, downsample: 1 });
        A("#tm3dFrame").src = `/viewer/dp/index.html?t=` + Date.now();
        info.textContent = "dump " + newest.id;
      }
    } catch (e) { info.textContent = "3D 载入失败: " + e.message; }
  }

  /* ----------------------------- wiring ---------------------------- */
  function wire() {
    if (started) return; started = true;

    // nav: tools dropdown (legacy pages tucked here)
    const tt = A("#toolsToggle"), tm = A("#toolsMenu");
    if (tt && tm) {
      tt.addEventListener("click", (e) => {
        e.stopPropagation();
        const open = tm.hidden;
        tm.hidden = !open;
        tt.setAttribute("aria-expanded", String(open));
      });
      tm.querySelectorAll("[data-tab]").forEach((b) =>
        b.addEventListener("click", () => { tm.hidden = true; tt.setAttribute("aria-expanded", "false"); }));
      document.addEventListener("click", (e) => {
        if (!tm.hidden && !tm.contains(e.target) && e.target !== tt) tm.hidden = true;
      });
    }

    // training cards refresh
    const tcR = A("#tcRefresh"), tcA = A("#tcAuto");
    if (tcR) tcR.addEventListener("click", refreshTrainCards);
    if (tcA) tcA.addEventListener("change", () => {
      if (tcA.checked) { if (!tcTimer) tcTimer = setInterval(refreshTrainCards, 4000); }
      else if (tcTimer) { clearInterval(tcTimer); tcTimer = null; }
    });

    // training modal close + 3d controls
    A("#tmClose").addEventListener("click", closeTrainModal);
    A("#trainModal").addEventListener("click", (e) => {
      if (e.target === A("#trainModal")) closeTrainModal();
    });
    document.addEventListener("keydown", (e) => {
      if (e.key === "Escape" && !A("#trainModal").hidden) closeTrainModal();
    });
    A("#tm3dReload").addEventListener("click", tmLoad3d);
    A("#tm3dAuto").addEventListener("change", (e) => {
      if (tm3dTimer) { clearInterval(tm3dTimer); tm3dTimer = null; }
      if (e.target.checked && !A("#trainModal").hidden) tm3dTimer = setInterval(tmLoad3d, 15000);
    });

    AA("#agModeSeg .seg-btn").forEach((b) => b.addEventListener("click", async () => {
      try { await apost("/api/agent/mode", { mode: b.dataset.mode });
        refreshState();
      } catch (e) { tnote("切换失败: " + e.message, true); }
    }));

    // collapsible control bar
    const barT = A("#agBarToggle"), barB = A("#agBarBody");
    if (barT && barB) barT.addEventListener("click", () => {
      const open = barB.hidden;
      barB.hidden = !open;
      barT.setAttribute("aria-expanded", String(open));
      barT.textContent = open ? "控制 ▴" : "控制 ▾";
    });

    // deep-think slider
    const dtR = A("#agDeepThink"), dtV = A("#agDeepThinkVal");
    if (dtR) {
      dtR.addEventListener("input", () => {
        dtDirty = true;
        const v = parseInt(dtR.value, 10) || 0;
        if (dtV) dtV.textContent = v ? v + " 轮" : "关";
      });
      dtR.addEventListener("change", async () => {
        const v = parseInt(dtR.value, 10) || 0;
        try { await apost("/api/agent/config", { deep_think_rounds: v });
          tnote(v ? `深思上限 ${v} 轮(agent 自决是否用)` : "深思已禁用"); }
        catch (e) { tnote("设置失败: " + e.message, true); }
        finally { dtDirty = false; refreshState(); }
      });
    }

    // strategy selector (manual override; agent can still switch via use_strategy)
    const strSel = A("#agStrategy");
    if (strSel) strSel.addEventListener("change", async () => {
      try { await apost("/api/agent/config", { active_strategy: strSel.value });
        tnote(strSel.value ? "已固定策略" : "策略交回 agent 自选"); refreshState(); }
      catch (e) { tnote("设置失败: " + e.message, true); }
    });
    A("#agModel").addEventListener("change", async (e) => {
      if (!e.target.value) return;
      try { await apost("/api/agent/models/activate", { id: e.target.value });
        tnote("已激活"); refreshState();
      } catch (err) { tnote("激活失败: " + err.message, true); }
    });
    A("#agModelsBtn").addEventListener("click", () => {
      const c = A("#agModelsCard"); c.hidden = !c.hidden;
      if (!c.hidden) refreshModels();
    });
    A("#agModelsClose").addEventListener("click", () => A("#agModelsCard").hidden = true);
    A("#mpAdd").addEventListener("click", async () => {
      const body = { name: A("#mpName").value.trim(), model: A("#mpModel").value.trim(),
        base_url: A("#mpUrl").value.trim(), api_key: A("#mpKey").value.trim() };
      if (!body.name || !body.model || !body.base_url || !body.api_key)
        return tnote("名称/模型/base_url/key 都要填", true);
      try { await apost("/api/agent/models", body);
        tnote("已保存"); A("#mpKey").value = ""; refreshModels(); refreshState();
      } catch (e) { tnote("保存失败: " + e.message, true); }
    });

    A("#agStart").addEventListener("click", async () => {
      try { await apost("/api/agent/start", {}); tnote("已启动值守"); refreshState(); }
      catch (e) { tnote("启动失败: " + e.message, true); }
    });
    A("#agStop").addEventListener("click", async () => {
      try { await apost("/api/agent/stop", {}); tnote("已停止"); refreshState(); }
      catch (e) { tnote("停止失败: " + e.message, true); }
    });
    A("#agWake").addEventListener("click", async () => {
      try { await apost("/api/agent/wake", {}); tnote("已唤醒"); }
      catch (e) { tnote(e.message, true); }
    });
    A("#agChatClear").addEventListener("click", () => A("#agChat").innerHTML = "");
    A("#agCtxClear").addEventListener("click", async () => {
      if (!confirm("清空上下文：丢弃滚动对话历史与摘要。\n系统提示词 / 常驻 spec / 知识库 / 台账 全部保留，agent 仍认识这个项目。\n确定？")) return;
      try { const r = await apost("/api/agent/clear_context", {});
        tnote(`已清空上下文（丢弃 ${r.dropped} 条）`);
        lastChatSig = ""; refreshState(); refreshTimeline();
      } catch (e) { tnote("清空失败: " + e.message, true); }
    });
    A("#agHistRefresh").addEventListener("click", (e) => { e.preventDefault(); e.stopPropagation(); refreshTimeline(); });

    // inline approval cards (rendered inside the chat) — handle via delegation
    A("#agChat").addEventListener("click", async (e) => {
      const card = e.target.closest(".approve-card.ac-inline");
      if (!card) return;
      const id = card.dataset.id;
      const ok = e.target.closest(".ac-ok"), no = e.target.closest(".ac-no");
      if (!ok && !no) return;
      const btns = card.querySelectorAll("button");
      btns.forEach((b) => (b.disabled = true));
      try {
        if (ok) { await apost("/api/agent/approve", { id }); tnote("已确认并执行"); }
        else {
          const reason = (card.querySelector(".ac-rej").value || "").trim();
          await apost("/api/agent/reject", { id, reason }); tnote("已驳回");
        }
        lastChatSig = ""; refreshState(); refreshTimeline();
      } catch (err) {
        tnote((ok ? "确认" : "驳回") + "失败: " + err.message, true);
        btns.forEach((b) => (b.disabled = false));
      }
    });

    const send = async () => {
      const t = A("#agInput").value.trim();
      if (!t) return;
      A("#agInput").value = "";
      try { await apost("/api/agent/message", { text: t });
        setTimeout(() => { refreshTimeline(); refreshState(); }, 300);
      } catch (e) { tnote("发送失败: " + e.message, true); }
    };
    A("#agSend").addEventListener("click", send);
    A("#agInput").addEventListener("keydown", (e) => {
      if (e.key === "Enter" && !e.shiftKey) { e.preventDefault(); send(); }
    });
  }

  async function poll() { await refreshState(); await refreshTimeline(); }

  function activate() {
    wire();
    refreshModels(); poll(); refreshTrainCards();
    if (timer) clearInterval(timer);
    timer = setInterval(poll, 1200);
    if (!tcTimer && A("#tcAuto") && A("#tcAuto").checked)
      tcTimer = setInterval(refreshTrainCards, 4000);
  }
  function deactivate() {
    if (timer) { clearInterval(timer); timer = null; }
    if (tcTimer) { clearInterval(tcTimer); tcTimer = null; }
  }

  // attach to the Agent tab button (in addition to app.js's own handler)
  document.addEventListener("DOMContentLoaded", hookTab);
  if (document.readyState !== "loading") hookTab();
  function hookTab() {
    const btn = document.querySelector('[data-tab="agent"]');
    if (!btn) return;
    btn.addEventListener("click", activate);
    AA(".tab").forEach((b) => {
      if (b.dataset.tab !== "agent") b.addEventListener("click", deactivate);
    });
    // Agent is the default landing tab -> activate immediately on load.
    if (A("#tab-agent") && A("#tab-agent").classList.contains("active")) activate();
  }
})();
