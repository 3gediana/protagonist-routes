# 交接提示词 — DP/PE 训练编排 Agent + 网页控制台

> 复制下面整段作为新会话的起始提示词即可。新会话需要 **SSH key**（secret 名 `DEVIN_SSH_KEY_SANS`）才能连机器；MiMo 模型 key 已存在远端面板配置里，无需再给。

---

## 你接手的是什么

一个**已经做好并在用户 Windows 训练机上运行**的「LLM 自主训练管家 + 网页控制台」，管理两套训练：
- **DP** `deep_protagonist`（单体生存 RL）
- **PE** `protagonist_ecology`（种群 MAP-Elites 进化）

面板 = FastAPI 后端（`server.py`）+ 原生 HTML/JS 前端（`index.html` / `static/*`）+ LLM agent 模块（`agent.py`）。
agent 用 OpenAI 兼容接口（当前 MiMo `mimo-v2.5`）做大脑，通过工具调用操作训练，事件驱动循环，平稳时休眠省额度。

## 连接方式（机器在用户那，GPU/训练都在那边）

- chisel 隧道已常驻：`2222→Windows:22`(SSH)、`8070→面板web`。本会话机器上有 `~/chisel` 在跑。
- SSH：`ssh -o IdentitiesOnly=yes -o StrictHostKeyChecking=no -i ~/.ssh/devin_key -p 2222 sans@localhost`
  - 远端默认 shell 是 **PowerShell**。**铁律：别用 `ssh ... powershell -Command "..."` 带引号**（SSH 转义会炸）。要跑脚本就**写 .ps1 → scp 上去 → `powershell -File`**。
- scp：`scp -o IdentitiesOnly=yes -o StrictHostKeyChecking=no -i ~/.ssh/devin_key -P 2222 <本地> sans@localhost:'D:/.../file'`（注意 scp 用大写 `-P`）。
- 面板代码根：`D:\claude-code\c++\routes\training_panel\`
- 本地工作副本（本会话）：`/home/ubuntu/training_panel/`（与远端保持一致，改完 scp 过去）。

## 项目路径 / 铁律（红线，违反=事故）

- DP 项根 `D:\claude-code\c++\routes\deep_protagonist`；PE 项根 `D:\claude-code\c++\routes\protagonist_ecology`；PE 二进制 `D:\claude-code\c++\build_ninja_cuda\bin\neural-eco-protagonist-batched.exe`。
- **不改训练核心 C++ 逻辑 / 不动数据流 / 不改输入维度**。agent 只调 TOML 超参数和旋钮、管生命周期。机制级代码改动由人来做。
- **绝不为刷指标加直接奖励**（Goodhart）。指标长期低→怀疑 bug，给诊断建议，不调小参数刷分。
- **磁盘保持 ≥190GB 空闲**（`disk_low_gb=190`）。低于则硬拦截拒绝开训 + 自动清日志/trace。
- **最多 10 个并发训练进程**（`max_procs=10`）。
- **受保护检查点永不可删**（代码硬拦）：`ft_gen/bc_gen/bc_hv2`、`ppo_d061/065/076_*`、`r1_cook_s24`、任何 `*_red/_final/_champ/_keep/_gold/*.bak`。其余 .pt agent 可决策删（走回收站 `_trash`，可恢复）。

## agent 能力（都已实现并部署）

- **两种模式**：`request`（开训/删检查点等门控操作先生成请求卡，人点同意才执行）/ `auto`（自动执行，但每步参数+理由都进时间线历史）。
- **休眠**：事件+定时驱动。下完动作就 sleep；只在到点/关键事件（run 结束/进程减少/灭绝/磁盘告警）/用户消息/手动唤醒时才调 API。空闲无训练时长睡（默认上限 240min）。睡眠期间每 20s 只做轻量轮询，不调 API。
- **资源感知 + 硬拦截**：每次喂 obs 快照（GPU/磁盘/进程数），≤10 进程、≥190G 在代码层硬拦，不靠模型自觉。
- **可切换策略**（模型用 `use_strategy` 工具自主选，激活后系统提示注入该策略【详细打法】）：`fast_iter`(快速迭代=小步smoke+并行多组+analyze_metrics早判好坏，留赢家杀输家) / `ab_knob`(对照换单旋钮) / `hist_bench`(历史对标query_gens) / `early_stop`(早停回退) / `ckpt_hygiene`(检查点卫生) / `compute_yield`(算力让路DP优先)。
- **agent 自主深度思考**（`deep_think` 工具）：面临复杂决策时模型**自己决定**调用，做多轮引导式内部推演（看现状→找原因→权衡→定方案），每轮不同上下文。推演内容流式展示+时间线留痕，但**不进滚动上下文**（用完即丢）。滑块=**上限**（0=禁用，N=最多N轮）；**同一次唤醒最多深思一次**。所有推演轮+正文合并进**同一个气泡**（历史折叠/当前展开）。
- **活知识库 / 状态存查点**：`kb_write/kb_query`（知识网络节点+links）、`record_gen/query_gens`（逐代台账）、`pin_spec`（常驻记忆，永不压缩）、`recall_timeline`/`list_docs`/`read_doc`（历史与资料**按需检索**，不预载）。已预置种子知识（蓝图/红线/已知坑/受保护清单）。
- **上下文管理**：系统提示（角色/红线/策略目录/目标/pin的spec/滚动摘要）**永不压缩、始终在场**；对话历史预算 600k token，超了把最旧段滚动摘要、保留最近N条，切割点不拆 tool_call/tool 结果对。
- **手动清空上下文**：面板「清空上下文」按钮 → `POST /api/agent/clear_context`。丢弃滚动对话历史+rolling_summary，**保留**系统提示/pin spec/知识库/台账。时间线留 `context_cleared` 痕迹，聊天流里显示分隔线。
- **持久化**：配置（模式/策略/深思上限/provider/阈值/pin/摘要）存 `agent_config.json`；知识 `agent_knowledge.json`；时间线 `agent_timeline.jsonl`。**重启服务后** `register_agent()` 调 `_rehydrate()` 从时间线重建显示用的 ring + 紧凑对话上下文（仅 user/assistant 文本，不带 tool 对，避免 OpenAI 配对报错），所以重开网页和离开时一致。

## 当前状态（截至交接）

- 4 个文件已 scp 到远端并**重启过面板**（无 `--reload`，改代码必须重启）：`agent.py` / `index.html` / `static/agent.js` / `static/style.css`。
- 面板在 `http://127.0.0.1:8070`（经隧道 `http://localhost:8070`）运行中。已验证：`/api/agent/state`（mode=request, strategy=fast_iter, deep_think_rounds=2, 1个provider=mimo激活）、`/api/agent/models`、`/api/agent/clear_context`(200)、`/api/agent/timeline`(rehydrate 出 63 条历史)。
- MiMo 测试 key 余额很低（~10块且本会话用过），**别空跑烧额度**：调试用 `/tmp/agent_it/stub_server.py` 离线跑（monkeypatch server），不必每次真调 LLM。
- 注意：终端里中文显示乱码是 **PTY 编码问题**，不是模型/网页问题；网页输出是正常 UTF-8。

## 重启 / 部署面板（PowerShell）

```powershell
# 停旧：
$p = (Get-NetTCPConnection -LocalPort 8070 -State Listen -EA SilentlyContinue).OwningProcess
if ($p) { Stop-Process -Id $p -Force }
# 起新（detach 到后台，日志写 panel.log）：
& 'D:\claude-code\c++\routes\training_panel\detach_panel.ps1'
```

## 可能的后续工作（用户没强制，按需）

1. 用户上一会话「网页面板 = DP/PE web3d viewer」收尾：DP active `frames.json` 曾是旧帧，已切到最新 `ft_gen` 导出；如需再核实两个 viewer 渲染。
2. 真机用 MiMo 跑一轮端到端演示（请求模式开训→批准→监控→休眠）；注意余额。
3. 把 `training_panel` 提交进仓库 + 更新 blueprint（用户本次未要求）。
4. 录制 UI 演示（单气泡深思、清空上下文、重启后恢复）。

## 离线自测

```
cd /tmp/agent_it && python3 test_rehydrate.py   # 验证 _rehydrate 重建 ring+对话、final assistant 带 deliberation
# run_it4.py 是旧测试（_deliberate 返回类型已从 str 改为 dict，需同步更新断言再用）
```
