# 指标体系设计草案 — 把"理解世界"翻译成全景指标

> 目标（用户愿景）：一句话定性目标 → agent 自己规划全部策略 → 7×24 不停训 → 每轮自动总结 → 永续迭代。
> 你不会给"把杀敌数拉上去"这种分立指标。**把定性愿景翻译成可观测、多维的代理量，是这套机制要补的核心。**
>
> 现状诊断（为什么跑了几百次没方向）：agent 的自治判定 `_campaign_verdict` 只看
> **PE: avg / qd / me_cov；DP: score / reward / unlocks 计数** —— 3~4 个粗标量。
> 而引擎其实已经把丰富数据写进了 jsonl / CSV，只是**从没喂给 agent 的决策路径**。
> 所以 agent 是在"盲人摸象"地调参；"方向"根本没被测量。

---

## 0. 愿景 → 5 个可测面（DP / PE 通用）

| 面 | 定性含义 | 怎么变成数字（思路） |
|---|---|---|
| **理解世界** | 对环境因果做出**有效**反应，不是空按 | 机制的**成功率/有效性**（attempt→success）、按因死亡的均衡度、夜间避寒率 |
| **逻辑感（链）** | 把机制串成因果链 | 采集→存放→建造 闭合率；砍树→制造→工地 闭合率 |
| **方向感（进度）** | 沿主线单调推进 | 里程碑深度/广度、科技树深度、niche 适应度上行 |
| **不退化（广度）** | 行为多样、不坍缩成单一打法 | 动作熵、单条命用到的机制数(repertoire/life)、niche 占用数、MAP-Elites 覆盖 |
| **真实/拟真** | 预判节律、社会涌现 | 通讯响应率、协作时长、记忆(dnc)使用；(预判 phase-lead 见 Phase 2) |

> **反 Goodhart 关键**：单看 avg/score 会被"刷分坍缩"骗（avg 涨但多样性塌）。新体系把
> **广度/多样性**与**进度**并列为一级轴 —— 进度涨但广度塌 = 坏方向，要回退，不晋级。

---

## 1. DP 指标（**全部可从现有 jsonl 直接推导，零改 C++、零重编**）

现有每集 jsonl 字段：`reward` + 奖励分解 `r_alive/r_food/r_water/r_kills/r_bites/r_death/r_vital/r_collect/r_shelter/r_craft/r_milestone/r_potential`、`deaths/deaths_cold/food/protein/vitamin`、`protein/vitamin`、`score`、`unlocks[10]`、`act[23]`、`explored_m`、`houses_built`、`sites_active`、`bldg_ticks`、`night_total`、`night_shelter`。

| 指标 | 面 | 算法 | 现状 |
|---|---|---|---|
| `milestone_depth` | 方向 | 平均 sum(unlocks)/ep，0–10 | 有 |
| `milestone_breadth` | 方向 | 曾解锁过的里程碑数，0–10 | 有 |
| `per_milestone_rate[10]` | 方向 | 每个里程碑触发率 | 有 |
| `techtree_depth` | 方向 | {COOK,MINE,axe,pickaxe,monument} 中被用到的动作数，0–5 | **新** |
| `actions_alive` | 不退化 | act 里非零动作数(去 NOOP)，0–22 | 有(反向) |
| `action_entropy` | 不退化 | act 分布的香农熵(归一 0–1)；**低=单策略坍缩** | **新** |
| `repertoire_per_life` | 不退化 | 每集用到的不同动作数的均值（区分"总体广但每条命窄"）| **新·关键** |
| `dead_actions[]` | 不退化 | 从不触发的动作清单 | 有 |
| `night_shelter_ratio` | 理解世界 | night_shelter/night_total；夜里会不会避寒 | **新** |
| `cold_death_share` | 理解世界 | deaths_cold/deaths；高=tendfire 空按、护寒无效 | **新** |
| `death_cause_balance` | 理解世界 | {cold,food,protein,vitamin} 死因份额的熵；单一死因=偏科 | **新** |
| `survival_rate` | 理解世界 | 整局不死的集占比 | 有 |
| `build_chain` | 逻辑 | collect_total→deposit(act)→sites_active→houses_built 闭合度 | **新** |
| `reward_decomp` | 反Goodhart | 各 r_* 占总 reward 的份额；看分到底是谁在驱动 | **新·关键** |
| `r_potential_share` | 反Goodhart | 势能塑形份额(policy-invariant，安全杠杆是否在起作用) | **新** |
| `explored_m` | 探索 | 平均探索面积 | 有 |

---

## 2. PE 指标（**全部可从现有 CSV 直接推导，面板当前完全没读**）

PE 每代写 3 个 CSV（run 目录）：
- `*_behaviors.csv`：`total/survived_protagonists`、`deposits_total/stick/stone`、`completed_worksites`、`worksite_completion_events`、`active_fires`、`chop/craft_attempts+successes`、`throw_attempts/throw_hits`、`hunt_attempts/successes`、`drank_water_events`、`signal_emits`、`signal_response_events`、`signal_co_attendance`、`cooperative_co_presence_seconds`、`dnc_usage_mean_avg`、`dnc_write/read/precedence_peak_avg`
- `*_fitness.csv`：`me_coverage`、`me_qd_score`、`me_max_fitness`、min/avg/best
- `*_evolution.csv`：8 个 niche 的 avg+best（large_herbivore / small_forager / apex_predator / pack_hunter / ambush_predator / scavenger / omnivore / rabbit）

| 指标 | 面 | 算法 | 现状 |
|---|---|---|---|
| `chop_rate / craft_rate / throw_rate / hunt_rate` | 理解世界 | success/attempt；区分真本事 vs **空按** | **新·关键** |
| `chain_closure` | 逻辑 | chop_succ→craft_succ→completed_worksites 闭合 | **新** |
| `deposits_total` | 逻辑 | 采集→存放步 | **新** |
| `active_fires / drank_water_events` | 理解世界 | 生火、喝水机制是否激活 | **新** |
| `niche_occupancy` | 不退化 | best_fitness 超阈值的 niche 数(7 生态位) | **新·关键** |
| `specialist_vs_omnivore` | 不退化 | 专精(apex/pack/ambush)best / 通才 best；<1=专精被 wash out | **新** |
| `me_coverage` | 不退化 | 填满格子数 | 有 |
| `me_qd_score / me_max_fitness` | 方向/不退化 | QD 总分、单格最强 | 有/新 |
| `signal_response_rate` | 真实 | signal_response_events/signal_emits；通讯是否真被用 | **新** |
| `cooperative_seconds` | 真实 | 协作共处秒数 | **新** |
| `dnc_usage` | 真实 | 记忆(可微神经计算机)使用强度；涌现记忆 | **新** |
| `survival_ratio` | 存活 | survived/total | **新** |
| `extinct_hits` | 存活 | 灭绝次数 | 有 |

---

## 3. agent 怎么用这个全景（关键：多轴判定替换 3 标量判定）

**现在**：`_campaign_verdict` 看 3 标量趋势 → improving/plateau/declining → 晋级/回退。
**改成**：把上面指标聚成一个**多轴健康向量**，分两组：

- **进度轴**（方向+理解世界+逻辑）：里程碑深度/广度、科技树深度、机制成功率、链闭合、niche 适应度。
- **广度轴**（不退化+真实）：动作熵、repertoire/life、niche 占用、me_cov、QD。

判定规则（仍然全在代码里、确定性、可单测）：
1. **灭绝 / 任一关键轴下行** → revert（保守不变）。
2. **广度轴坍缩（熵↓ / niche 占用↓ / 覆盖↓）即使进度轴上行** → **revert（反 Goodhart：刷分坍缩不算进步）**。
3. 进度轴上行 **且** 广度轴不退 → promote。
4. 都平 → 末阶段 done，否则 promote（规模吃满）。
5. 样本太少 → extend；读不出 → stop。

同时 `analyze_metrics` 把**整张谱**返回给 LLM（不再只给 3 个数），面板加一块"方向全景"卡片，KB 每轮自动写入谱摘要 → 下轮 kb_query 自动用上。

---

## 4. 范围 / 分期

- **Phase 1（本次实现，零改 C++、零重编）**：上面**所有**带"可推导"的指标，全部从现有 jsonl/CSV 推导；
  接进 `_analyze_metrics` + 多轴 `_campaign_verdict` + 面板全景卡片 + KB 自动摘要 + 单测。
  —— 这一层就能让 agent/你第一次看到"方向"。
- **Phase 2（可选，需引擎支持，归"改底层机制"）**：真正的**预判节律 phase-lead**（趋火早于体温最低点）、
  DP 的**逐步因果触发**（冷→才 tendfire 的时序契合）。这些要 per-step 轨迹或新 C++ 埋点，
  红线相关，等 Phase 1 落地、你确认方向后再单独评估。

> 一句话：Phase 1 不碰游戏物理/奖励结构（红线不动），只是**把已经产生、却被丢弃的丰富行为数据，
> 加工成多维全景喂给 agent**。这正是"完善底层机制"里最该先补、且零风险的一块。
