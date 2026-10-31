"use strict";
/* Roadmap goal cards — inline column in Agent tab.
   Model A: joint completion + attack-priority order. Completion is decided by
   the backend from real eval — this UI only renders status.
   User writes goals in natural language; Agent fills metrics.
   Reuses $,$$,api,toast from app.js. */
(function () {
  const STATUS_LABEL = { pending: "待办", active: "主攻中", met: "已达标",
                         blocked: "受阻", near_met: "接近" };

  let cards = [];
  let dirty = false;
  const knownIds = new Set();
  const prevStatus = {};
  let meta = { objective: "", hold_window: 2, stall_after: 6, stall_policy: "skip", active: false };
  let timer = null, dragId = null, lastSig = "";

  const el = (id) => document.getElementById(id);

  function setDirty(v) {
    dirty = v;
    el("rmDirty").hidden = !v;
    el("rmRevert").hidden = !v;
    el("rmApply").disabled = !v;
  }

  /* ---------- fetch / apply ---------- */
  async function load(refresh) {
    if (dirty && !refresh) return;
    let d;
    try { d = await api("/api/agent/roadmap" + (refresh ? "?refresh=1" : "")); }
    catch (e) { return; }
    meta = { objective: d.objective || "", hold_window: d.hold_window, stall_after: d.stall_after,
             stall_policy: d.stall_policy, active: !!d.active };
    if (!dirty) {
      // Structure (cards/order/status/acceptance) unchanged => patch live values
      // in place; never rebuild the DOM, so animations don't replay.
      const structSig = JSON.stringify([
        (d.goals || []).map(g => [g.id, g.status, g.title,
                                  g.project, (g.acceptance || []).map(a => [a.metric, a.op, a.target])]),
        d.objective, d.complete, d.hold_window, d.stall_after, d.stall_policy]);
      if (structSig === lastSig && !refresh) {
        cards = (d.goals || []).map(cloneGoal);
        patchValues();
        paintStatusLine(d);
        return;
      }
      lastSig = structSig;
      cards = (d.goals || []).map(cloneGoal);
      syncMetaInputs();
      render();
      paintStatusLine(d);
    }
  }
  function cloneGoal(g) {
    return {
      id: g.id, title: g.title || "", detail: g.detail || "", project: g.project || "any",
      order: g.order, status: g.status || "pending", progress: +g.progress || 0,
      blocked_reason: g.blocked_reason || "",
      acceptance: (g.acceptance || []).map(a => ({ metric: a.metric, op: a.op, target: a.target })),
      clauses: g.clauses || [],
      trend: g.trend || null,
    };
  }
  function syncMetaInputs() {
    const obj = el("rmObjective");
    if (document.activeElement !== obj) obj.value = meta.objective;
    el("rmHold").value = meta.hold_window;
    el("rmStall").value = meta.stall_after;
    el("rmPolicy").value = meta.stall_policy;
    // rmActive is now hidden; sync its value attr
    const act = el("rmActive");
    if (act) act.value = meta.active ? "1" : "0";
  }
  function paintStatusLine(d) {
    const line = el("rmStatusLine");
    if (!d.goals || !d.goals.length) {
      line.textContent = "未规划。在上面写你的目标，或让 agent 自己用 plan_roadmap 规划。";
      line.className = "rm-status-line"; return;
    }
    const met = d.goals.filter(g => g.status === "met").length;
    const near = d.goals.filter(g => g.status === "near_met").length;
    const blocked = d.goals.filter(g => g.status === "blocked").length;
    const attack = d.goals.find(g => g.status === "active");
    if (d.complete) {
      line.textContent = "✓ 全部达标并稳住（" + met + "/" + d.goals.length + " 卡同时达标）";
      line.className = "rm-status-line done";
    } else {
      let t = met + "/" + d.goals.length + " 达标";
      if (near) t += " · " + near + " 接近";
      if (blocked) t += " · " + blocked + " 受阻";
      if (attack) t += " · 主攻：" + attack.title;
      line.textContent = t;
      line.className = "rm-status-line";
    }
  }

  /* Send objective to agent via chat */
  async function sendObjective() {
    const text = el("rmObjective").value.trim();
    if (!text) { toast("请先写下你的目标", true); return; }
    try {
      await api("/api/agent/roadmap", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ objective: text, reason: "用户更新目标(自然语言)" })
      });
      toast("目标已发送给 Agent，它会自己拆解成具体指标");
      setDirty(false);
      load();
    } catch (e) { toast("发送失败：" + e.message, true); }
  }

  async function apply() {
    /* Save reordering only — user doesn't edit acceptance anymore */
    const body = {
      objective: el("rmObjective").value.trim(),
      hold_window: +el("rmHold").value || 2,
      stall_after: +el("rmStall").value || 6,
      stall_policy: el("rmPolicy").value,
      active: meta.active,
      goals: cards.map(c => ({
        id: c.id, title: c.title, detail: c.detail, project: c.project,
        acceptance: c.acceptance,
      })),
      reason: "网页看板排序/目标编辑",
    };
    try {
      await api("/api/agent/roadmap", { method: "POST", headers: { "Content-Type": "application/json" }, body: JSON.stringify(body) });
      setDirty(false);
      toast("已保存");
      load();
    } catch (e) { toast("保存失败：" + e.message, true); }
  }

  /* ---------- render ---------- */
  function render() {
    const board = el("rmBoard");
    if (!cards.length) {
      board.innerHTML = '<div class="rm-empty">还没有卡片。在上面写下你的目标，Agent 会自己拆解并规划。</div>';
      return;
    }
    board.innerHTML = "";
    let freshCount = 0;
    cards.forEach((c, idx) => {
      const isNew = !knownIds.has(c.id);
      const node = renderCard(c, idx);
      board.appendChild(node);
      if (prevStatus[c.id] === "met" && (c.status === "pending" || c.status === "active")) {
        node.classList.add("regress-flash");
        setTimeout(() => node.classList.remove("regress-flash"), 720);
      }
      prevStatus[c.id] = c.status;
      if (isNew) {
        node.classList.add("enter");
        const delay = 60 * freshCount++;
        requestAnimationFrame(() => requestAnimationFrame(() => {
          node.style.transitionDelay = delay + "ms";
          node.classList.remove("enter");
          setTimeout(() => { node.style.transitionDelay = ""; }, 500 + delay);
        }));
        knownIds.add(c.id);
      }
      const fill = node.querySelector(".rm-prog-fill");
      const w = Math.round(c.progress * 100) + "%";
      if (isNew) {
        requestAnimationFrame(() => requestAnimationFrame(() => { fill.style.width = w; }));
      } else {
        // known card re-rendered: set width instantly so the bar doesn't
        // replay the 0% -> value animation (looked like 3s jumping)
        fill.style.transition = "none";
        fill.style.width = w;
        requestAnimationFrame(() => { fill.style.transition = ""; });
      }
    });
  }

  /* Update progress bars / live values on existing nodes without rebuilding. */
  function patchValues() {
    cards.forEach((c) => {
      const node = el("rmBoard").querySelector('.rm-card[data-id="' + c.id + '"]');
      if (!node) return;
      const fill = node.querySelector(".rm-prog-fill");
      if (fill) fill.style.width = Math.round(c.progress * 100) + "%";
      const pct = node.querySelector(".rm-prog-pct");
      if (pct) pct.textContent = Math.round(c.progress * 100) + "%";
      node.querySelectorAll(".rm-clause").forEach((row, i) => {
        const a = (c.acceptance || [])[i];
        if (!a) return;
        const live = (c.clauses || []).find(x => x.metric === a.metric);
        row.classList.toggle("ok", !!(live && live.ok));
        row.classList.toggle("near", !!(live && live.near && !live.ok));
        const lv = row.querySelector(".rm-live b");
        if (lv && live) lv.textContent =
          (live.value === null || live.value === undefined ? "—" : (+live.value).toFixed(3));
        const tr = row.querySelector(".rm-trend");
        if (tr && live && live.trend) {
          tr.className = "rm-trend " + (live.trend === "up" ? "up" : live.trend === "down" ? "down" : "flat");
          tr.textContent = live.trend === "up" ? "↑" : live.trend === "down" ? "↓" : "→";
        }
      });
      const why = node.querySelector(".rm-blocked-why");
      if (why && c.blocked_reason) {
        const t = "⚠ " + c.blocked_reason;
        if (why.textContent !== t) { why.textContent = t; why.title = c.blocked_reason; }
      }
    });
  }

  function renderCard(c, idx) {
    const div = document.createElement("div");
    div.className = "rm-card";
    div.dataset.status = c.status;
    div.dataset.id = c.id;
    div.draggable = true;

    const top = document.createElement("div");
    top.className = "rm-card-top";
    top.innerHTML =
      '<span class="rm-grip" title="拖动重排">⠿</span>' +
      '<span class="rm-order">' + (idx + 1) + '</span>' +
      '<span class="rm-proj-tag ' + c.project + '">' + c.project + '</span>';
    // Read-only title (Agent-set)
    const title = document.createElement("span");
    title.className = "rm-title-ro";
    title.textContent = c.title || "(Agent 未命名)";
    title.title = c.title;
    top.appendChild(title);
    const badge = document.createElement("span");
    badge.className = "rm-badge " + c.status;
    badge.textContent = STATUS_LABEL[c.status] || c.status;
    top.appendChild(badge);
    div.appendChild(top);

    // progress
    const prog = document.createElement("div"); prog.className = "rm-prog";
    prog.innerHTML = '<div class="rm-prog-fill"></div>';
    div.appendChild(prog);
    const pct = document.createElement("div");
    pct.className = "rm-prog-pct"; pct.textContent = Math.round(c.progress * 100) + "%";
    div.appendChild(pct);

    if (c.status === "blocked" && c.blocked_reason) {
      const w = document.createElement("div"); w.className = "rm-blocked-why";
      w.textContent = "⚠ " + c.blocked_reason; w.title = c.blocked_reason;
      div.appendChild(w);
    }

    // Read-only acceptance clauses (Agent-filled)
    if (c.acceptance && c.acceptance.length) {
      const cl = document.createElement("div"); cl.className = "rm-clauses";
      c.acceptance.forEach((a) => cl.appendChild(renderClauseReadonly(c, a)));
      div.appendChild(cl);
    }

    // drag handlers
    div.addEventListener("dragstart", (e) => { dragId = c.id; div.classList.add("dragging"); e.dataTransfer.effectAllowed = "move"; });
    div.addEventListener("dragend", () => { dragId = null; div.classList.remove("dragging"); $$(".rm-card").forEach(n => n.classList.remove("drop-target")); });
    div.addEventListener("dragover", (e) => { e.preventDefault(); div.classList.add("drop-target"); });
    div.addEventListener("dragleave", () => div.classList.remove("drop-target"));
    div.addEventListener("drop", (e) => {
      e.preventDefault(); div.classList.remove("drop-target");
      if (!dragId || dragId === c.id) return;
      const from = cards.findIndex(x => x.id === dragId), to = cards.findIndex(x => x.id === c.id);
      if (from < 0 || to < 0) return;
      const [moved] = cards.splice(from, 1); cards.splice(to, 0, moved);
      setDirty(true); render();
    });
    return div;
  }

  function renderClauseReadonly(c, a) {
    const row = document.createElement("div"); row.className = "rm-clause";
    const live = (c.clauses || []).find(x => x.metric === a.metric);
    if (live && live.ok) row.classList.add("ok");
    else if (live && live.near) row.classList.add("near");
    row.innerHTML = '<span class="rm-met-dot"></span>';
    // Compact read-only text: "me_cov >= 8"
    const txt = document.createElement("span"); txt.className = "rm-clause-text";
    txt.textContent = (a.metric || "?") + " " + (a.op || ">=") + " " + (a.target != null ? a.target : "?");
    row.appendChild(txt);
    if (live) {
      const lv = document.createElement("span"); lv.className = "rm-live";
      lv.innerHTML = "<b>" + (live.value === null || live.value === undefined ? "—" : (+live.value).toFixed(3)) + "</b>";
      row.appendChild(lv);
      // trend indicator
      if (live.trend) {
        const tr = document.createElement("span");
        tr.className = "rm-trend " + (live.trend === "up" ? "up" : live.trend === "down" ? "down" : "flat");
        tr.textContent = live.trend === "up" ? "↑" : live.trend === "down" ? "↓" : "→";
        row.appendChild(tr);
      }
    }
    return row;
  }

  /* ---------- wiring ---------- */
  function init() {
    el("rmObjSend").addEventListener("click", sendObjective);
    el("rmApply").addEventListener("click", apply);
    el("rmRevert").addEventListener("click", () => { setDirty(false); load(); });
    el("rmRefresh").addEventListener("click", () => load(true));
    el("rmObjective").addEventListener("input", () => setDirty(true));
    // enter key in objective sends
    el("rmObjective").addEventListener("keydown", (e) => {
      if (e.key === "Enter" && !e.shiftKey) { e.preventDefault(); sendObjective(); }
    });

    // The roadmap is always visible in agent tab; start polling immediately
    ensureTimer();
    load();
  }
  function ensureTimer() {
    if (timer) return;
    timer = setInterval(() => {
      if (!el("rmAuto") || !el("rmAuto").checked) return;
      // Always poll since roadmap is inline in agent tab (always visible)
      const agentTab = document.getElementById("tab-agent");
      if (agentTab && agentTab.classList.contains("active")) load();
    }, 2000);
  }

  if (document.readyState === "loading") document.addEventListener("DOMContentLoaded", init);
  else init();
})();
