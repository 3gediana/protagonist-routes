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

    // pending approvals
    const pc = A("#agPendingCard"), pd = A("#agPending");
    const pend = s.pending || [];
    if (pend.length) {
      pc.hidden = false;
      pd.innerHTML = pend.map((p) => `
        <div class="approve-card" data-id="${esc(p.id)}">
          <div class="ac-head"><b>${esc(p.tool)}</b>
            <span class="ac-time">${fmtClock(p.proposed_at)}</span></div>
          <div class="ac-reason">理由：${esc(p.reason || "(未填)")}</div>
          <pre class="ac-args">${esc(JSON.stringify(p.args, null, 1))}</pre>
          <div class="ac-actions">
            <input class="ac-rej" placeholder="否决理由(可选)" />
            <button class="primary mini ac-ok">同意执行</button>
            <button class="danger mini ac-no">否决</button>
          </div>
        </div>`).join("");
      pd.querySelectorAll(".approve-card").forEach((card) => {
        const id = card.dataset.id;
        card.querySelector(".ac-ok").addEventListener("click", async () => {
          try { const r = await apost("/api/agent/approve", { id });
            tnote("已批准并执行"); refreshState(); refreshTimeline();
          } catch (e) { tnote("批准失败: " + e.message, true); }
        });
        card.querySelector(".ac-no").addEventListener("click", async () => {
          const reason = card.querySelector(".ac-rej").value || "";
          try { await apost("/api/agent/reject", { id, reason });
            tnote("已否决"); refreshState(); refreshTimeline();
          } catch (e) { tnote("否决失败: " + e.message, true); }
        });
      });
    } else { pc.hidden = true; pd.innerHTML = ""; }
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
  function chatBubble(ev) {
    if (ev.kind === "context_cleared")
      return `<div class="chat-divider">— 已清空上下文（丢弃 ${ev.dropped || 0} 条；系统提示/常驻 spec/知识库保留）—</div>`;
    const isUser = ev.kind === "user_msg" || ev.kind === "user_msg_queued";
    const who = isUser ? "你" : "Agent";
    const cls = isUser ? "ab-user" : "ab-assistant";
    // history assistant bubbles fold the deep-think rounds + reasoning by default
    const delib = !isUser ? delibBlock(ev.deliberation, false) : "";
    const think = (!isUser && (ev.reasoning || "").trim())
      ? `<details class="cb-think"><summary>思考过程</summary>
         <div class="cb-think-body">${esc(ev.reasoning)}</div></details>` : "";
    return `<div class="chat-bubble ${cls}">${metaRow(who, ev.ts)}
      ${delib}${think}<div class="cb-text">${esc(ev.text || "")}</div></div>`;
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
    // deep_think events go to the decision-history sidebar only; in the chat
    // they are merged into the assistant bubble via ev.deliberation.
    const chat = evs.filter((e) =>
      e.kind === "user_msg" || e.kind === "context_cleared" ||
      (e.kind === "assistant" &&
       ((e.text || "").trim() || (e.deliberation && e.deliberation.length))));
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

  /* ----------------------------- wiring ---------------------------- */
  function wire() {
    if (started) return; started = true;

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
    A("#agHistRefresh").addEventListener("click", refreshTimeline);

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
    refreshModels(); poll();
    if (timer) clearInterval(timer);
    timer = setInterval(poll, 1200);
  }
  function deactivate() { if (timer) { clearInterval(timer); timer = null; } }

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
  }
})();
