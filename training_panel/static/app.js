"use strict";
const $ = (s) => document.querySelector(s);
const $$ = (s) => Array.from(document.querySelectorAll(s));

async function api(path, opts) {
  const r = await fetch(path, opts);
  if (!r.ok) {
    let msg = r.statusText;
    try { msg = (await r.json()).detail || msg; } catch (e) {}
    throw new Error(msg);
  }
  return r.json();
}
function toast(msg, isErr) {
  const t = $("#toast");
  t.textContent = msg;
  t.className = "toast show" + (isErr ? " err" : "");
  setTimeout(() => (t.className = "toast"), 3600);
}
function fmtDur(s) {
  s = Math.round(s || 0);
  const h = Math.floor(s / 3600), m = Math.floor((s % 3600) / 60), ss = s % 60;
  return (h ? h + "h" : "") + (m || h ? m + "m" : "") + ss + "s";
}

// In-page confirmation (replaces native confirm(), which blocks as a
// browser-level dialog outside the DOM). Returns a Promise<boolean>.
function confirmModal(msg, okLabel) {
  const ov = $("#modal");
  $("#modalMsg").textContent = msg;
  const ok = $("#modalOk"), cancel = $("#modalCancel");
  ok.textContent = okLabel || "确认";
  ov.hidden = false;
  return new Promise((resolve) => {
    const done = (v) => {
      ov.hidden = true;
      ok.removeEventListener("click", onOk);
      cancel.removeEventListener("click", onCancel);
      ov.removeEventListener("click", onBackdrop);
      document.removeEventListener("keydown", onKey);
      resolve(v);
    };
    const onOk = () => done(true);
    const onCancel = () => done(false);
    const onBackdrop = (e) => { if (e.target === ov) done(false); };
    const onKey = (e) => {
      if (e.key === "Escape") done(false);
      if (e.key === "Enter") done(true);
    };
    ok.addEventListener("click", onOk);
    cancel.addEventListener("click", onCancel);
    ov.addEventListener("click", onBackdrop);
    document.addEventListener("keydown", onKey);
    ok.focus();
  });
}

/* ----------------------------- tabs ----------------------------- */
$$(".tab").forEach((b) =>
  b.addEventListener("click", () => {
    if (!b.dataset.tab) return; // dropdown toggles (e.g. 工具 ▾) are not panels
    $$(".tab").forEach((x) => x.classList.remove("active"));
    $$(".panel").forEach((x) => x.classList.remove("active"));
    b.classList.add("active");
    $("#tab-" + b.dataset.tab).classList.add("active");
    document.body.classList.toggle("agent-lock", b.dataset.tab === "agent");
    if (b.dataset.tab === "procs") loadProcs();
    if (b.dataset.tab === "logs") loadLogs();
    if (b.dataset.tab === "ckpt") loadCkpts();
    if (b.dataset.tab === "metrics") refreshMetricFiles();
    if (b.dataset.tab === "replay") loadTraces();
  })
);

document.body.classList.toggle("agent-lock",
  !!document.querySelector("#tab-agent.active"));

/* ---------------------------- theme ----------------------------- */
$("#themeBtn").addEventListener("click", () => {
  const cur = document.documentElement.getAttribute("data-theme");
  const next = cur === "light" ? "" : "light";
  document.documentElement.setAttribute("data-theme", next);
  $("#themeBtn").textContent = next === "light" ? "◑" : "◐";
});

/* ------------------------ system status ------------------------- */
async function pollStatus() {
  try {
    const s = await api("/api/system/status");
    if (s.gpu) {
      const g = Array.isArray(s.gpu) ? s.gpu[0] : s.gpu;
      $("#chipGpu").textContent =
        `GPU ${g.util_pct}% · ${Math.round(g.mem_used_mb)}/${Math.round(g.mem_total_mb)}MB`;
    } else $("#chipGpu").textContent = "GPU n/a";
    if (s.disk) $("#chipDisk").textContent = `磁盘 ${s.disk.free_gb}GB 空闲`;
    setChip("#chipDp", "DP", s.dp_count);
    setChip("#chipPe", "PE", s.pe_count);
    $("#procBadge").textContent = (s.dp_count || 0) + (s.pe_count || 0);
  } catch (e) {}
}
function setChip(sel, name, n) {
  const c = $(sel);
  c.textContent = `${name} ×${n || 0}`;
  c.className = "chip" + (n ? " run" : "");
}
setInterval(pollStatus, 4000);
pollStatus();

/* ------------------------ training control ---------------------- */
async function loadDpCkpts() {
  try {
    const list = await api("/api/dp/checkpoints");
    fillCkptSelect($("#dpCkpt"), list, '<option value="">（不加载，从头）</option>');
    fillCkptSelect($("#nwCkpt"), list, '<option value="">选择…</option>');
    dpPreview();
  } catch (e) { toast("载入 DP 检查点失败: " + e.message, true); }
}
function fillCkptSelect(sel, list, head) {
  sel.innerHTML = head;
  list.forEach((c) => {
    const o = document.createElement("option");
    o.value = c.name; o.textContent = `${c.name} (${c.size_mb}MB)`;
    sel.appendChild(o);
  });
}
async function loadPeConfigs() {
  try {
    const list = await api("/api/pe/configs");
    const sel = $("#peCfg");
    sel.innerHTML = "";
    list.forEach((c) => {
      const o = document.createElement("option");
      o.value = c.name;
      let t = c.name;
      if (c.seed != null) t += ` (seed ${c.seed})`;
      if (c.is_panel_clone) t = "↳ " + t;
      o.textContent = t;
      sel.appendChild(o);
    });
    pePreview();
  } catch (e) { toast("载入 PE 配置失败: " + e.message, true); }
}
function dpPreview() {
  const ck = $("#dpCkpt").value;
  let c = `deep_protagonist_train.exe --policy ${$("#dpPolicy").value}`;
  if (ck) c += ` --load runs\\${ck}`;
  c += ` --n-envs ${$("#dpNenvs").value} --steps ${$("#dpSteps").value}`;
  c += ` --save-path runs\\${$("#dpTag").value}_<ts>.pt --save-every ${$("#dpSaveEvery").value}`;
  c += ` --metrics runs\\${$("#dpTag").value}_<ts>.jsonl --seed ${$("#dpSeed").value}`;
  if ($("#dpExtra").value) c += " " + $("#dpExtra").value;
  const env = [];
  if ($("#dpSpawn").checked) env.push("DP_SPAWN_RANDOM=1");
  if ($("#dpHunt").checked) env.push("DP_HUNT_V2=1");
  if ($("#dpPrey").checked) env.push("DP_PREY_PRIORITY=1");
  $("#dpCmd").textContent = (env.length ? env.join(" ") + "\n" : "") + c;
}
["#dpCkpt","#dpPolicy","#dpSeed","#dpSteps","#dpNenvs","#dpSaveEvery","#dpTag","#dpExtra","#dpSpawn","#dpHunt","#dpPrey"]
  .forEach((s) => $(s).addEventListener("input", dpPreview));

function pePreview() {
  let c = `neural-eco-protagonist-batched.exe --config ${$("#peCfg").value}`;
  const ov = [];
  if ($("#peSeed").value) ov.push("seed=" + $("#peSeed").value);
  if ($("#peResume").value) ov.push("resume=…");
  if ($("#peGen").value) ov.push("gen=" + $("#peGen").value);
  if (ov.length) c = `# 克隆配置 (${ov.join(", ")}):\n` + c.replace($("#peCfg").value, "panel_*.toml");
  $("#peCmd").textContent = "set PATH=D:\\NVIDIA\\CUDA\\v12.8\\bin;%PATH%\n" + c;
}
["#peCfg","#peSeed","#peGen","#peResume"].forEach((s) => $(s).addEventListener("input", pePreview));

$("#dpStart").addEventListener("click", async () => {
  try {
    const body = {
      checkpoint: $("#dpCkpt").value || null,
      policy: $("#dpPolicy").value,
      seed: $("#dpSeed").value ? +$("#dpSeed").value : 24,
      steps: $("#dpSteps").value ? +$("#dpSteps").value : 8000000,
      n_envs: +$("#dpNenvs").value || 4,
      save_every: +$("#dpSaveEvery").value || 25,
      spawn_random: $("#dpSpawn").checked,
      hunt_v2: $("#dpHunt").checked,
      prey_priority: $("#dpPrey").checked,
      tag: $("#dpTag").value || "panel",
      extra_args: $("#dpExtra").value,
    };
    const r = await api("/api/dp/train", { method: "POST",
      headers: { "Content-Type": "application/json" }, body: JSON.stringify(body) });
    toast(`DP 训练已启动 ${r.run_id} pid=${r.pid}`);
    addLiveRun(r.run_id, "DP " + (body.tag)); openLiveWs(r.run_id);
  } catch (e) { toast("启动失败: " + e.message, true); }
});
$("#peStart").addEventListener("click", async () => {
  try {
    if (!$("#peCfg").value) return toast("请先选择 TOML 配置", true);
    const body = {
      config: $("#peCfg").value,
      seed: $("#peSeed").value ? +$("#peSeed").value : null,
      generations: $("#peGen").value ? +$("#peGen").value : null,
      resume_checkpoint: $("#peResume").value || null,
    };
    const r = await api("/api/pe/train", { method: "POST",
      headers: { "Content-Type": "application/json" }, body: JSON.stringify(body) });
    toast(`PE 训练已启动 ${r.run_id} (${r.config})`);
    addLiveRun(r.run_id, "PE " + r.config); openLiveWs(r.run_id);
    loadPeConfigs();
  } catch (e) { toast("启动失败: " + e.message, true); }
});

/* ------------------------- new world ---------------------------- */
$("#nwProj").addEventListener("change", async () => {
  const p = $("#nwProj").value;
  if (p === "dp") { await loadDpCkpts(); }
  else {
    try {
      const list = await api("/api/pe/checkpoints");
      const sel = $("#nwCkpt");
      sel.innerHTML = '<option value="">选择…</option>';
      list.forEach((r) => {
        const o = document.createElement("option");
        o.value = r.latest_path; o.textContent = `${r.scenario}/${r.run} · ${r.latest}`;
        sel.appendChild(o);
      });
    } catch (e) { toast("载入 PE 检查点失败: " + e.message, true); }
  }
});
$("#nwRun").addEventListener("click", async () => {
  const proj = $("#nwProj").value, ck = $("#nwCkpt").value;
  if (!ck) return toast("请选一个 checkpoint", true);
  const seed = +$("#nwSeed").value, steps = +$("#nwSteps").value;
  try {
    if (proj === "dp") {
      const r = await api("/api/dp/newworld", { method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ checkpoint: ck, seed, steps, frames: 1500 }) });
      if (r.needs_build) {
        $("#nwMsg").textContent = "⚠️ deep_protagonist_dump 还没编译。\n构建命令：\n" + r.build_hint;
        toast("dump 工具未编译，见下方构建提示", true);
      } else {
        $("#nwMsg").textContent = `已启动 ${r.run_id} → ${r.out_dir}\n${r.cmd}`;
        toast(`DP 新世界 rollout 已启动 ${r.run_id}`);
        addLiveRun(r.run_id, "DP newworld"); openLiveWs(r.run_id);
      }
    } else {
      const r = await api("/api/pe/train", { method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ config: $("#peCfg").value, seed,
          resume_checkpoint: ck, generations: 2 }) });
      $("#nwMsg").textContent = `已克隆并启动 ${r.config}\n${r.cmd}`;
      toast(`PE 新世界 (同模型) 已启动 ${r.run_id}`);
      addLiveRun(r.run_id, "PE newworld"); openLiveWs(r.run_id);
    }
  } catch (e) { toast("启动失败: " + e.message, true); }
});

/* --------------------------- live console ----------------------- */
let liveWs = null;
function addLiveRun(runId, label) {
  const sel = $("#liveRun");
  if (![...sel.options].some((o) => o.value === runId)) {
    const o = document.createElement("option");
    o.value = runId; o.textContent = `${label} · ${runId}`;
    sel.appendChild(o);
  }
  sel.value = runId;
}
function openLiveWs(runId) {
  if (liveWs) { try { liveWs.close(); } catch (e) {} }
  $("#liveConsole").textContent = "";
  if (!runId) return;
  const proto = location.protocol === "https:" ? "wss" : "ws";
  liveWs = new WebSocket(`${proto}://${location.host}/ws/logs?run_id=${encodeURIComponent(runId)}`);
  liveWs.onmessage = (ev) => {
    const d = JSON.parse(ev.data);
    if (d.lines && d.lines.length) {
      const el = $("#liveConsole");
      el.textContent += d.lines.join("\n") + "\n";
      el.scrollTop = el.scrollHeight;
    }
  };
}
$("#liveRun").addEventListener("change", (e) => openLiveWs(e.target.value));
$("#liveClear").addEventListener("click", () => ($("#liveConsole").textContent = ""));

/* --------------------------- processes -------------------------- */
let procTimer = null;
async function loadProcs() {
  try {
    const r = await api("/api/processes");
    const tb = $("#procTable tbody"); tb.innerHTML = "";
    if (!r.processes.length) {
      tb.innerHTML = '<tr><td colspan="7" class="hint">未发现运行中的训练进程</td></tr>';
    }
    r.processes.forEach((p) => {
      const tr = document.createElement("tr");
      const cfg = p.config || "—";
      const mem = p.mem_mb != null ? p.mem_mb + " MB" : "—";
      const src = p.source === "panel" ? '<span class="badge free">面板</span>'
                                       : '<span class="badge">外部</span>';
      const kind = p.kind === "newworld" ? ' <span class="badge">新世界</span>' : "";
      const stopId = p.run_id ? `data-run="${p.run_id}"` : `data-pid="${p.pid}"`;
      tr.innerHTML = `<td><b>${p.project.toUpperCase()}</b>${kind}</td><td>${src}</td>
        <td>${p.pid}</td><td title="${p.cmdline||''}">${cfg}</td>
        <td>${fmtDur(p.elapsed_s)}</td><td>${mem}</td>
        <td>
          ${p.run_id ? `<button class="ghost mini" data-watch="${p.run_id}">看输出</button>` : ""}
          <button class="warn mini procStop" ${stopId}>优雅停止</button>
          <button class="danger mini procKill" ${stopId}>强杀</button>
        </td>`;
      tb.appendChild(tr);
    });
    $$(".procStop").forEach((b) => b.addEventListener("click", () => stopProc(b, false)));
    $$(".procKill").forEach((b) => b.addEventListener("click", () => stopProc(b, true)));
    $$("[data-watch]").forEach((b) => b.addEventListener("click", () => {
      $$(".tab").forEach((x) => x.classList.remove("active"));
      $$(".panel").forEach((x) => x.classList.remove("active"));
      $('.tab[data-tab="train"]').classList.add("active");
      $("#tab-train").classList.add("active");
      document.body.classList.remove("agent-lock");
      addLiveRun(b.dataset.watch, "run"); openLiveWs(b.dataset.watch);
    }));
  } catch (e) { toast("载入进程失败: " + e.message, true); }
}
async function stopProc(btn, hard) {
  const runId = btn.getAttribute("data-run");
  const pid = btn.getAttribute("data-pid");
  const what = runId || ("pid " + pid);
  if (hard && !(await confirmModal(`强杀 ${what}？可能打断正在写入的 checkpoint，导致 .pt 损坏。确认？`, "强杀"))) return;
  if (!hard && !(await confirmModal(`优雅停止 ${what}？(发送停止信号，等待自身在边界存盘后退出)`, "优雅停止"))) return;
  try {
    const body = runId ? { run_id: runId, hard } : { pid: +pid, hard };
    await api("/api/train/stop", { method: "POST",
      headers: { "Content-Type": "application/json" }, body: JSON.stringify(body) });
    toast(`${what} 已${hard ? "强杀" : "停止"}`);
    setTimeout(loadProcs, 800);
  } catch (e) { toast("停止失败: " + e.message, true); }
}
$("#procRefresh").addEventListener("click", loadProcs);
$("#procAuto").addEventListener("change", (e) => {
  clearInterval(procTimer);
  if (e.target.checked) procTimer = setInterval(loadProcs, 3000);
});
procTimer = setInterval(() => { if ($("#tab-procs").classList.contains("active")) loadProcs(); }, 3000);

/* ------------------------------ logs ---------------------------- */
async function loadLogs() {
  const proj = $("#logProj").value;
  try {
    const list = await api(`/api/${proj}/logs`);
    const tb = $("#logTable tbody"); tb.innerHTML = "";
    list.forEach((f) => {
      const tr = document.createElement("tr");
      const chk = f.protected
        ? '<span title="受保护">🔒</span>'
        : `<input type="checkbox" class="logChk" value="${f.name}" ${f.kind === "log" ? "checked" : ""}>`;
      const st = f.protected ? '<span class="badge lock">protected</span>'
               : (f.kind === "jsonl" ? '<span class="badge">指标</span>' : '<span class="badge free">日志</span>');
      tr.innerHTML = `<td>${chk}</td><td>${f.name}</td><td>${f.kind}</td>
        <td>${f.size_kb} KB</td><td>${f.mtime}</td><td>${st}</td>`;
      tb.appendChild(tr);
    });
  } catch (e) { toast("载入日志失败: " + e.message, true); }
}
$("#logRefresh").addEventListener("click", loadLogs);
$("#logProj").addEventListener("change", loadLogs);
$("#logAll").addEventListener("change", (e) =>
  $$(".logChk").forEach((c) => (c.checked = e.target.checked)));
function selectedLogs() { return $$(".logChk").filter((c) => c.checked).map((c) => c.value); }
async function clearLogs(hard) {
  const names = selectedLogs();
  if (!names.length) return toast("请先勾选要清理的日志", true);
  if (!(await confirmModal(`清理选中的 ${names.length} 个日志？` + (hard ? " (彻底删除，不可恢复)" : " (移到回收站可恢复)"), hard ? "彻底删除" : "清理"))) return;
  try {
    const r = await api("/api/logs/clear", { method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ project: $("#logProj").value, names, hard }) });
    let m = `已清理 ${r.removed.length} 个 (${r.mode})`;
    if (r.skipped.length) m += `，跳过 ${r.skipped.length}`;
    toast(m); loadLogs();
  } catch (e) { toast("清理失败: " + e.message, true); }
}
$("#logClearSafe").addEventListener("click", () => clearLogs(false));
$("#logClearHard").addEventListener("click", () => clearLogs(true));

/* --------------------------- checkpoints ------------------------ */
async function loadCkpts() {
  try {
    const list = await api("/api/dp/checkpoints");
    const tb = $("#ckptTable tbody"); tb.innerHTML = "";
    list.forEach((c) => {
      const tr = document.createElement("tr");
      const chk = c.protected
        ? '<span title="红线资产">🔒</span>'
        : `<input type="checkbox" class="ckptChk" value="${c.name}">`;
      const st = c.protected ? '<span class="badge lock">protected</span>'
                             : '<span class="badge free">可清理</span>';
      tr.innerHTML = `<td>${chk}</td><td>${c.name}</td><td>${c.size_mb} MB</td><td>${c.mtime}</td><td>${st}</td>`;
      tb.appendChild(tr);
    });
  } catch (e) { toast("载入检查点失败: " + e.message, true); }
  try {
    const pe = await api("/api/pe/checkpoints");
    const tb = $("#peCkptTable tbody"); tb.innerHTML = "";
    pe.forEach((r) => {
      const tr = document.createElement("tr");
      tr.innerHTML = `<td>${r.scenario}</td><td>${r.run}</td><td>${r.count}</td><td>${r.latest}</td>`;
      tb.appendChild(tr);
    });
  } catch (e) {}
}
$("#ckptRefresh").addEventListener("click", loadCkpts);
async function clearCkpts(hard) {
  const names = $$(".ckptChk").filter((c) => c.checked).map((c) => c.value);
  if (!names.length) return toast("请先勾选要清理的检查点", true);
  if (!(await confirmModal(`清理 ${names.length} 个检查点？` + (hard ? " (彻底删除！)" : " (移到回收站可恢复)"), hard ? "彻底删除" : "清理"))) return;
  try {
    const r = await api("/api/dp/checkpoints/clear", { method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ names, hard }) });
    let m = `已清理 ${r.removed.length} 个 (${r.mode})`;
    if (r.skipped.length) m += `，跳过 ${r.skipped.length} 个(受保护/不存在)`;
    toast(m); loadCkpts();
  } catch (e) { toast("清理失败: " + e.message, true); }
}
$("#ckptClearSafe").addEventListener("click", () => clearCkpts(false));
$("#ckptClearHard").addEventListener("click", () => clearCkpts(true));

/* ---------------------------- metrics --------------------------- */
let charts = {};
let autoTimer = null;
async function refreshMetricFiles() {
  const proj = $("#mProj").value;
  try {
    const list = await api(`/api/${proj}/logs`);
    const sel = $("#mFile");
    const jsonl = list.filter((f) => f.kind === "jsonl");
    sel.innerHTML = '<option value="">选择 .jsonl …</option>';
    (jsonl.length ? jsonl : list).forEach((f) => {
      const o = document.createElement("option");
      const short = f.name.length > 48 ? "…" + f.name.slice(-47) : f.name;
      o.value = f.name; o.textContent = `${short} (${f.size_kb}KB)`;
      o.title = f.name;
      sel.appendChild(o);
    });
  } catch (e) { toast("载入指标列表失败: " + e.message, true); }
}
$("#mRefresh").addEventListener("click", refreshMetricFiles);
$("#mProj").addEventListener("change", refreshMetricFiles);
$("#mLoad").addEventListener("click", loadMetrics);
$("#mAuto").addEventListener("change", (e) => {
  if (e.target.checked) autoTimer = setInterval(() => loadMetrics(true), 5000);
  else clearInterval(autoTimer);
});

async function loadMetrics(auto) {
  const isAuto = auto === true;
  if (isAuto && !$("#tab-metrics").classList.contains("active")) return;
  const proj = $("#mProj").value, file = $("#mFile").value;
  if (!file) { if (!isAuto) toast("请先选择 jsonl 文件", true); return; }
  try {
    if (proj === "pe") { await loadPeMetrics(file); return; }
    const m = await api(`/api/metrics?project=${proj}&file=${encodeURIComponent(file)}`);
    if (!m.n) { $("#mKpis").innerHTML = "<p class='hint'>该文件没有可解析的指标行</p>"; return; }
    $("#mKpis").innerHTML = `
      ${kpi("episodes", m.n)}
      ${kpi("最新 reward", m.reward_smooth.at(-1).toFixed(1))}
      ${kpi("存活率", (m.survival_rate * 100).toFixed(0) + "%")}
      ${kpi("解锁里程碑", m.unlocks.at(-1) + "/" + m.milestone_names.length)}
      ${kpi("沉默动作", m.dead_actions.length)}`;
    drawReward(m); drawMilestone(m); drawAction(m); drawDeath(m);
  } catch (e) { toast("加载指标失败: " + e.message, true); }
}
async function loadPeMetrics(file) {
  const m = await api(`/api/pe/curve?file=${encodeURIComponent(file)}`);
  if (!m.n) { $("#mKpis").innerHTML = "<p class='hint'>该文件没有可解析的 Gen 指标行</p>"; return; }
  $("#mKpis").innerHTML = `
    ${kpi("世代", m.gen.at(-1))}
    ${kpi("最新 avg", m.avg.at(-1).toFixed(1))}
    ${kpi("最新 best", m.best.at(-1).toFixed(1))}
    ${kpi("行为覆盖", m.me_cov.at(-1) + "/" + m.me_total)}
    ${kpi("QD 总分", m.qd.at(-1).toFixed(0))}`;
  const labels = m.gen;
  mk("chReward", { type: "line", data: { labels, datasets: [
    { label: "avg", data: m.avg, borderColor: C_BRONZE, pointRadius: 0, borderWidth: 2 },
    { label: "best", data: m.best, borderColor: C_GREEN, pointRadius: 0, borderWidth: 2 },
  ]}, options: { responsive: true, maintainAspectRatio: false, plugins: { title: { display: true, text: "PE 适应度曲线 (按世代)", color: titleColor }, legend: { labels: { color: tickColor } } }, scales: baseScale } });
  mk("chMilestone", { type: "line", data: { labels, datasets: [
    { label: "qd", data: m.qd, borderColor: C_AMBER, pointRadius: 0, borderWidth: 2 },
  ]}, options: { responsive: true, maintainAspectRatio: false, plugins: { title: { display: true, text: "QD 总分", color: titleColor }, legend: { display: false } }, scales: baseScale } });
  mk("chAction", { type: "line", data: { labels, datasets: [
    { label: "me_cov", data: m.me_cov, borderColor: C_SLATE, pointRadius: 0, borderWidth: 2 },
  ]}, options: { responsive: true, maintainAspectRatio: false, plugins: { title: { display: true, text: `行为覆盖 me_cov (/${m.me_total})`, color: titleColor }, legend: { display: false } }, scales: baseScale } });
  const cov = m.me_cov.at(-1), total = Math.max(m.me_total, 1);
  mk("chDeath", { type: "doughnut", data: { labels: ["已覆盖", "未覆盖"], datasets: [
    { data: [cov, total - cov], backgroundColor: [C_GREEN, "rgba(140,154,171,.25)"] },
  ]}, options: { responsive: true, maintainAspectRatio: false, plugins: { title: { display: true, text: "行为格子覆盖占比", color: titleColor }, legend: { labels: { color: tickColor } } } } });
}
function kpi(k, v) { return `<div class="kpi"><div class="v">${v}</div><div class="k">${k}</div></div>`; }
function mk(id, cfg) {
  const old = charts[id];
  if (old && old.config.type === cfg.type) {
    old.data = cfg.data;
    old.options = cfg.options;
    old.update("none");   // in-place refresh: no destroy, no replayed animation
    return;
  }
  if (old) old.destroy();
  charts[id] = new Chart($("#" + id), cfg);
}
// premium-black chart palette (shared look with the agent training-detail modal):
// bronze accent + muted earth tones, no blue AI flavor.
const gridColor = "rgba(140,154,171,.14)";
const tickColor = "#8b9aab", titleColor = "#e8e6e1";
const C_BRONZE = "#c8a86a", C_BRONZE_FAINT = "rgba(200,168,106,.35)";
const C_GREEN = "#6fae8f", C_SLATE = "#7ea6c8";
const C_AMBER = "#d9a441", C_RED = "#c8736a";
const baseScale = { x: { grid: { color: gridColor }, ticks: { color: tickColor } },
                    y: { grid: { color: gridColor }, ticks: { color: tickColor } } };
function drawReward(m) {
  const labels = m.reward.map((_, i) => i);
  mk("chReward", { type: "line", data: { labels, datasets: [
    { label: "reward", data: m.reward, borderColor: C_BRONZE_FAINT, pointRadius: 0, borderWidth: 1 },
    { label: "reward (smooth)", data: m.reward_smooth, borderColor: C_BRONZE, pointRadius: 0, borderWidth: 2 },
    { label: "wander ref", data: labels.map(() => m.wander_ref), borderColor: C_RED, borderDash: [5,5], pointRadius: 0, borderWidth: 1 },
  ]}, options: { responsive: true, maintainAspectRatio: false, plugins: { title: { display: true, text: "Reward 曲线", color: titleColor }, legend: { labels: { color: tickColor } } }, scales: baseScale } });
}
function drawMilestone(m) {
  mk("chMilestone", { type: "bar", data: { labels: m.milestone_names, datasets: [
    { label: "触发率", data: m.milestone_rate.map((x) => +(x*100).toFixed(1)), backgroundColor: C_BRONZE },
  ]}, options: { responsive: true, maintainAspectRatio: false, plugins: { title: { display: true, text: "里程碑触发率 %", color: titleColor }, legend: { display: false } }, scales: baseScale } });
}
function drawAction(m) {
  const colors = m.action_names.map((n) => m.dead_actions.includes(n) ? C_RED : C_GREEN);
  mk("chAction", { type: "bar", data: { labels: m.action_names, datasets: [
    { label: "平均次数/局", data: m.action_mean.map((x) => +x.toFixed(2)), backgroundColor: colors },
  ]}, options: { responsive: true, maintainAspectRatio: false, plugins: { title: { display: true, text: "动作分布 (红=沉默)", color: titleColor }, legend: { display: false } }, scales: baseScale } });
}
function drawDeath(m) {
  const labels = Object.keys(m.deaths), data = Object.values(m.deaths);
  mk("chDeath", { type: "doughnut", data: { labels, datasets: [
    { data, backgroundColor: [C_SLATE, C_AMBER, C_RED, C_GREEN] },
  ]}, options: { responsive: true, maintainAspectRatio: false, plugins: { title: { display: true, text: "死因分布", color: titleColor }, legend: { labels: { color: tickColor } } } } });
}

/* ---------------------------- replay ---------------------------- */
let curTraces = { pe: [], dp: [] };
async function loadTraces() {
  try {
    const t = await api("/api/traces");
    curTraces = t;
    renderTraces();
    $("#viewerFrame").src = `/viewer/${$("#rProj").value}/index.html`;
  } catch (e) { toast("载入 trace 失败: " + e.message, true); }
}
function renderTraces() {
  const proj = $("#rProj").value;
  const list = curTraces[proj] || [];
  const tb = $("#traceTable tbody"); tb.innerHTML = "";
  if (!list.length) {
    tb.innerHTML = `<tr><td colspan="4" class="hint">没有可用 trace</td></tr>`;
  }
  list.forEach((tr0) => {
    const tr = document.createElement("tr");
    if (proj === "pe") {
      const lbl = `${tr0.scenario}/${tr0.run} · ${tr0.generation}/${tr0.episode}`;
      tr.innerHTML = `<td><input type="checkbox" class="trChk" value="${tr0.run_root || (tr0.scenario + '/' + tr0.run)}"></td>
        <td title="${tr0.id}">${lbl}</td>
        <td>${tr0.frames_mb} MB${tr0.has_world ? "" : " ⚠️无world"}</td>
        <td><button class="primary mini" data-load="${tr0.id}">载入回放</button></td>`;
    } else {
      tr.innerHTML = `<td><input type="checkbox" class="trChk" value="${tr0.id}"></td>
        <td>${tr0.id}</td><td>${tr0.frames_mb} MB</td>
        <td><button class="primary mini" data-load="${tr0.id}">载入回放</button></td>`;
    }
    tb.appendChild(tr);
  });
  $$("[data-load]").forEach((b) => b.addEventListener("click", () => loadReplay(b.dataset.load)));
  $("#replayInfo").innerHTML = `共 ${list.length} 个 trace` +
    (proj === "pe" && curTraces.pe_total > list.length ? `（仅显示前 ${list.length}/${curTraces.pe_total}）` : "");
}
async function loadReplay(traceId) {
  const proj = $("#rProj").value;
  const down = Math.max(1, +$("#rDown").value || 1);
  try {
    toast("载入中…");
    const r = await api("/api/replay/load", { method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ project: proj, trace_id: traceId, downsample: down }) });
    $("#replayInfo").innerHTML =
      `已载入 <code>${traceId}</code> → ${r.loaded_frames}/${r.total_frames} 帧` +
      (r.downsample_step > 1 ? ` (降采样 1/${r.downsample_step})` : "") +
      (r.world_copied === false ? " · ⚠️缺 world" : "");
    $("#viewerFrame").src = `/viewer/${proj}/index.html?t=` + Date.now();
    toast(`回放已载入 ${r.loaded_frames} 帧`);
  } catch (e) { toast("载入回放失败: " + e.message, true); }
}
$("#trAll").addEventListener("change", (e) =>
  $$(".trChk").forEach((c) => (c.checked = e.target.checked)));
$("#rClear").addEventListener("click", async () => {
  const ids = $$(".trChk").filter((c) => c.checked).map((c) => c.value);
  if (!ids.length) return toast("请先勾选要清理的 trace", true);
  if (!(await confirmModal(`清理 ${ids.length} 个 trace？(移到回收站可恢复)`, "清理"))) return;
  try {
    const r = await api("/api/traces/clear", { method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ project: $("#rProj").value, ids, hard: false }) });
    const freed = r.removed.reduce((a, x) => a + (x.freed_mb || 0), 0);
    toast(`已清理 ${r.removed.length} 个 trace，释放 ${freed.toFixed(1)} MB`);
    loadTraces();
  } catch (e) { toast("清理失败: " + e.message, true); }
});
$("#rProj").addEventListener("change", renderTraces);
$("#rRefresh").addEventListener("click", loadTraces);

/* ----------------------------- init ----------------------------- */
loadDpCkpts(); loadPeConfigs(); dpPreview(); pePreview();
