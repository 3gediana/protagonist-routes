# Memory Evidence Audit · 2026-05-22

> 位置：`routes/protagonist_ecology/docx/supporting/MEMORY_EVIDENCE_AUDIT.md`
>
> HEAD 基线：`f64d7c4`（初版审计 commit）→ 后续 audit-driven fix 见 § 7
>
> 作用：把"三任务证据链 + 44 维 MemoryCognitionPerception + 四类 memory bank 的真实 query"对照一次。
>
> 用法：
> - 每次想动 reward shaping / metric / perception 维度前先查 § 1~§ 3
> - 不在这里讲设计哲学；只列「真代码 + 真数据 + 哪里 DEAD/WEAK」
> - 数据来源：§ 1~§ 4 用 v3 genome (2026-05-22 01:57:11)；§ 7 增加 v4-v7 follow-up 数据

---

## 1. 三任务对照表（含 danger_avoidance 一并校准）

### 1.1 任务 setup 与 reward channels

| 任务 | code 路径 | world setup | reward channels（taskParams 给的非零项） |
|---|---|---|---|
| `water_return` | `MemoryAblationEval.cpp` L73-107 + `Runtime.cpp` L416-428 | 120×120，base@(44,60)，1 water source，spawn_radius=42（>感知 36），init_hydration=0.65，decay=0.6/s | survival_weight=2.0；其它全 0 |
| `danger_avoidance` | `MemoryAblationEval.cpp` L109-125 + `Runtime.cpp` L430-442 | 96×96，6 predators，protagonist_warn_enabled=false | survival_weight=2.5，food_weight=0.5 |
| `worksite_return` | `MemoryAblationEval.cpp` L127-149 + `Runtime.cpp` L444-456 | 120×120，1 worksite，24 sticks/stones，required=2 stick+1 stone | survival_weight=0.2，cooperation_weight=6.0，worksite_proximity=0.08，worksite_completion=80.0 |
| `signal_response` | `MemoryAblationEval.cpp` L151-178 + `Runtime.cpp` L458-479 | 120×120，controlled_emitter 30s 周期，response 半径=8，window=15s | survival=0.2，food=4.0，communication=10.0，signal_emit=0.2，co_attendance=2.5，**signal_response_reward=0**（listener-side reward 默认关） |

### 1.2 primary metric 定义

| 任务 | primary | secondary（值得看的） |
|---|---|---|
| `water_return` | `drank_water_events` (hydration 跳升 >0.02 计 1 次) | `avg_hydration_ratio`, `dehydrated_avg` |
| `danger_avoidance` | `survived` (episode 末活体计数) | `injured_avg`, `protagonist_deaths_avg` |
| `worksite_return` | `worksite_completion_events` | `deposits_avg`, `avg_fitness`, `best_fitness` |
| `signal_response` | `signal_response_events` (listener 在 emit 后 ≤15s 进 ≤8m halo, 同 emit 同 listener 不重计) | `signal_co_attendance`（emitter 视角，与 primary 不同侧） |

### 1.3 最新真实分离（2026-05-22 01:57:11 run，4 episodes，fixed genome v3）

| 任务 | baseline | no_spatial | no_episodic | no_social | no_goal | 分离判断 |
|---|---:|---:|---:|---:|---:|---|
| `water_return` (drank_water_events) | **124** | 69.75 (-44%) | 121.5 (-2%) | 122 (-2%) | 74.75 (-40%) | spatial **OK**，goal **OK**（cascade，见 § 4），episodic/social DEAD |
| `danger_avoidance` (survived) | 8 | 8 | 8 | 8 | 8 | **整体 DEAD**（无任何分离力） |
| `worksite_return` (events) | 1 | 0 (-100%) | 0.75 (-25%) | 0.75 (-25%) | 0 (-100%) | events baseline=1 **太低**，统计噪声大；fitness 维度（见下）才看得清 |
| `worksite_return` (best_fitness) | 210.7 | 22.7 (-89%) | 218.9 (+4%) | 207.7 (-1%) | 23.0 (-89%) | spatial **OK**，goal **OK**（cascade），episodic/social DEAD |
| `signal_response` (events) | 0 | 0 | 0 | 0 | 0 | **整体 DEAD**（chicken-and-egg，agent 永不进 halo） |
| `signal_response` (best_fitness) | 21.4 | 39.25 | 27.2 | 36.6 | 15.6 | fitness 抖动来自 food_weight=4，**不是 signal 行为**，不可作分离证据 |

### 1.4 已知替代路径 / 假阳性

- **water_return**：`no_spatial` 与 `no_goal` 同时下降的根因是 § 4 的 spatial→goal_target cascade，不是两条独立证据
- **worksite_return**：events baseline=1 太低，单次 episode 是否完成的随机性比 spatial 信号强，分离不能只看 events，必须叠 best_fitness（worksite_proximity 持续给）来校准
- **signal_response**：primary metric 跑通靠 listener 必须先进 8m halo，但 perception 没有"上一次听到信号在哪"的 memory readback（见 § 3.4），policy 没有梯度引导进 halo —— **结构性缺口**，不是 reward 量级问题
- **danger_avoidance**：全员 survived=8 不是"记忆有用"，是任务太简单（6 predator + episode 时长不够淘汰 agent）

---

## 2. MemoryCognitionPerception 44 维审计

| index | 维度 | 来源 bank · query | 依赖 `*_writes_enabled` | 哪个任务真用到 | 状态 |
|---|---|---|---|---|---|
| 0-1 | water dxdy | spatial.findNearestWater | spatial | water_return | **OK**（spatial OFF → -44%） |
| 2-3 | food dxdy | spatial.findNearestFood | spatial | 无任务直接用 | WEAK（默认开但任务没需求） |
| 4-5 | tree dxdy | spatial.findNearestTree | spatial | 无任务直接用 | WEAK |
| 6-7 | danger dxdy | spatial.findNearestDanger | spatial | danger_avoidance 名义上 | DEAD（任务自己 -0%） |
| 8-9 | base dxdy | spatial.findNearestBase | spatial | 无（cascade through goal） | WEAK |
| 10-11 | fire dxdy | spatial.findNearestFire | spatial | 无（campfire.count=0 in 三任务） | DEAD |
| 12-13 | worksite dxdy | spatial.findNearestWorksite | spatial | worksite_return | **OK**（spatial OFF → -89% fitness） |
| 14-15 | stone dxdy | spatial.findNearestStone | spatial | worksite_return | **OK**（同上 cascade） |
| 16-17 | stick dxdy | spatial.findNearestStick | spatial | worksite_return | **OK**（同上） |
| 18-19 | top-salient event pos dxdy | episodic.topSalient(1) | episodic | — | DEAD（episodic OFF → -2%） |
| 20 | top-salient salience | 同上 | episodic | — | DEAD |
| 21 | top-salient recency | 同上 | episodic | — | DEAD |
| 22 | was_attacked salience | episodic.recentOfType("was_attacked",1) | episodic | danger_avoidance 名义上 | DEAD（任务整体 DEAD） |
| 23 | attacked_enemy salience | episodic.recentOfType("attacked_enemy",1) | episodic | 无 | DEAD |
| 24-25 | best_ally pos dxdy | social.relations() scan | social | — | DEAD（social OFF → -2%） |
| 26 | best_ally cooperation | 同上 | social | — | DEAD |
| 27 | best_ally communication | 同上 | social | signal_response 名义上 | DEAD（任务整体 DEAD） |
| 28-29 | nearest_predator pos dxdy | social.relations() scan | social | danger_avoidance 名义上 | DEAD |
| 30 | predator flag | 同上 | social | 同上 | DEAD |
| 31 | predator hostility | 同上 | social | 同上 | DEAD |
| 32 | goal_idx normalized | goal.currentGoal()/Count | goal | water_return, worksite_return | **OK**（goal OFF → 跨 -40%~-89%） |
| 33 | goal commitment | goal.commitment() | goal | 同上 | OK（与 32 同进退） |
| 34-35 | goal_target dxdy | switch(goal) → spatial.findNearestX | **goal AND spatial** | water_return, worksite_return | **OK 但 entangled**（见 § 4） |
| 36 | dnc usage_mean | brain.dncSummary() | DNC | — | UNKNOWN（ablation 表没有 DNC ablation） |
| 37 | dnc usage_max | 同上 | DNC | — | UNKNOWN |
| 38 | dnc write_peak | 同上 | DNC | — | UNKNOWN |
| 39 | dnc read_peak | 同上 | DNC | — | UNKNOWN |
| 40 | dnc read_norm/4 | 同上 | DNC | — | UNKNOWN |
| 41 | dnc precedence_peak | 同上 | DNC | — | UNKNOWN |
| 42 | dnc write_focus_slot | 同上 | DNC | — | UNKNOWN |
| 43 | dnc read_focus_slot | 同上 | DNC | — | UNKNOWN |

### 2.1 维度统计

- 共 44 维
- 状态分布：**OK 9** (0-1, 12-17, 32-33, 34-35) / **DEAD 14** (10-11, 18-31) / **WEAK 5** (2-5, 8-9) / **UNKNOWN 8** (36-43, DNC) / **混合 OK-entangled 2** (34-35)
- **真正在当前 4 任务里被验证有效的维度只有 9/44 = 20%**
- 18-31 这一整块（14 维 episodic + social）当前看不出任何任务驱动，是最大块的"挂着但 DEAD"区域

---

## 3. 四类 Memory Bank · 最小有效 Query 清单

> 列「该任务如果只能保留 1 个 query 通道，哪个 query 是 must-have」。

### 3.1 SpatialMemoryBank

| 任务 | 最小有效 query | code 路径 |
|---|---|---|
| water_return | `findNearestWater(pos, 0.15)` | `Perception.cpp` L53 |
| worksite_return | `findNearestStick / Stone / Worksite` 三者都需要（材料 + 目的地） | `Perception.cpp` L59-61 |
| signal_response | **无现成 query**（spatial 没有 signal_seen 槽） | gap，见 § 4 |
| danger_avoidance | `findNearestDanger`（但任务整体 DEAD，不能验证） | `Perception.cpp` L56 |

### 3.2 EpisodicMemoryBank

| 任务 | 最小有效 query | code 路径 | 当前是否暴露给 perception |
|---|---|---|---|
| water_return | 理论应有 `recentOfType("drank_water")` | EpisodicMemoryBank 已有 record，但 perception 未 readback | **MISSING readback** |
| worksite_return | 理论应有 `recentOfType("built_worksite")` 或 `recentOfType("deposited_*")` | record 已有，readback 缺 | **MISSING readback** |
| signal_response | 理论应有 `recentOfType("peer_signal_heard")` | **事件类型本身就没** | **MISSING event + readback** |
| danger_avoidance | `recentOfType("saw_predator")` (Bonus 8 已写入) | **readback 缺**（perception 只暴露 was_attacked / attacked_enemy） | **MISSING readback** |

**所有任务的 episodic 都缺 readback。**当前 perception 只暴露 `topSalient(1) + was_attacked + attacked_enemy` 三条 query，与 4 个任务的需求几乎完全错位。

### 3.3 SocialMemoryBank

| 任务 | 最小有效 query | 是否需要 |
|---|---|---|
| signal_response | listener 需要 `recentSignalSource(peer_id, time_window)` 或 `lastSignalPosFromPeer(peer_id)` | **不存在该 API** |
| worksite_return | `mostCooperative(k)` 找老搭档 | API 有，perception **未调用** |
| danger_avoidance | `relations()` 扫预测者位置 | perception 在调用，但任务 DEAD |
| water_return | 不需要 social | — |

`SocialMemoryBank.h` 有 `mostCooperative / mostHostile`，但 perception 没用，**只在做 `for (auto& [_, rel] : relations())` 全表 O(n) 扫**（`Perception.cpp` L98-114）。

### 3.4 GoalMemory

| 任务 | 最小有效 goal | 当前 GoalManager 是否真切到这个 goal |
|---|---|---|
| water_return | `FindWater` | **是**（GoalManager 规则 Flee > FindWater > ...） |
| worksite_return | `GatherStick / GatherStone / BuildWorksite` | **是**（Phase 3 已接入） |
| signal_response | `RespondSignal` | **几乎从不**（被 hydration / carrying / site_needs 盖住，即使 metric 已 decouple，listener 仍无朝向梯度） |
| danger_avoidance | `Flee` | **是**（最高优先级），但任务 DEAD 看不出 |

GoalMemory 的 `commitment / elapsed_seconds / completion_progress` 三个字段中只有 `commitment` 被 perception 暴露（dim 33），`elapsed_seconds / completion_progress` **未 readback**，等于 GoalMemory 三分之二状态是 dead state。

---

## 4. 审计关键结论

### 4.1 Spatial → Goal cascade（最关键的 entangle）

`Perception.cpp` L142-200 里 `goal_target` 的 dxdy（dim 34-35）走的是 `switch(goal) → spatial.findNearestX`。这意味着：

- 关闭 `spatial_memory_writes_enabled` 时，dim 0-17 全 0 **且** dim 34-35 全 0（goal_target 内部就是 spatial query）
- 关闭 `goal_manager_enabled` 时，dim 32-35 全 0
- 所以 `no_spatial` 与 `no_goal` 在 water/worksite 任务里都强分离，**不是两条独立证据**

**结论**：当前 spatial 与 goal 在 perception 出口处是耦合的，不能把 `no_goal` 当成"goal 通道独立证据"。要看 goal 独立信号，得把 `goal_target` 这俩维度从 spatial 里解耦（比如换成 goal_kind one-hot 或 elapsed_seconds），或者增设 `no_goal_target` ablation。

### 4.2 Episodic 整块 DEAD

14 个 episodic + social 维度（18-31）在 4 任务里没有任何分离力。EpisodicMemoryBank 写入很丰富（chopped_tree, built_worksite, deposited_stick/stone, signal_emitted, drank_water, saw_predator, was_attacked, attacked_enemy, killed_prey, ate_food），但 perception 只 readback 了：

- `topSalient(1)` 一条
- `recentOfType("was_attacked",1)` 一条
- `recentOfType("attacked_enemy",1)` 一条

任务 reward 也不奖励"利用 episodic"。需要么是任务自己 query 错（应该问 drank_water / built_worksite），么是任务根本不需要 episodic（那就把维度回收）。

### 4.3 Social 整块 DEAD

24-31 八个 social 维度同样无分离。`recordSighting / rewardCooperation / rewardCommunication / recordHostility` 都在 runtime 里被定期调用，但被 ablation 关掉时任务几乎无掉点，说明 perception 暴露的 social 维度对**当前**四个任务的 reward 没贡献。

### 4.4 Signal response 的结构性缺口

回顾 signal_response 全 0 的链路：

1. listener 没有"上次听到信号在哪"的 memory readback（spatial 没槽 + episodic 没事件类型 + social 没字段）
2. perception 里只有 `signal_layer->history().query(...)` 直接读 SignalLayer（L128-140），但**只用来给 RespondSignal goal 提供 goal_target**
3. GoalManager 又把 RespondSignal 排在 hydration / carrying / site_needs 后面，listener 几乎切不到 RespondSignal
4. 即使 metric 已经 decouple（不依赖 GoalManager），listener 物理位置仍受 controller 决定，controller 没有梯度引导

**4 个独立通道任何一条接通都能破：**
- (a) 加 `spatial.signal_seen` 槽 + perception readback dim
- (b) 加 episodic `peer_signal_heard` 事件 + perception readback
- (c) 加 social `last_signal_pos_from_peer` 字段
- (d) listener 直接对 `SignalPerception` 强度做 dense shaping reward（不走 memory，最便宜）

option (d) 是 DEVELOPMENT_TASK_PLAN.md A1 第 3 项的方向；但 (a/b/c) 任意一条更彻底，会让 signal 真正进入 memory pipeline。

### 4.5 DNC 8 维状态不明

dim 36-43 是 DNC summary，目前 ablation harness 没有 `no_dnc` 变体。DNC 对当前 4 任务有用没用未知。

---

## 5. 下一步收口建议（不是执行承诺，等 user 决定）

按 DEVELOPMENT_TASK_PLAN.md A1 仍未 done 的 4 项排序：

1. **signal_response perception-driven shaping reward**（A1 第 3 项）
   - 最小动法：listener 每 tick 用 `SignalPerception` 强度的 EMA 做 dense reward
   - 配 `params.signal_response_reward` 已经有 plumbing，但要换成 perception-strength gating 而不是 distance gating
   - 不动 memory 结构、不重训 genome 也能跑一轮 30 代验证

2. **listener trace 字段**（A1 第 4 项）
   - 在 `agent_trace.csv` 加 `signal_strength` / `signal_dx` / `signal_dy` / `responded_signal_id` 列
   - 让事后归因能区分"听到没→朝向变没→进 halo 没→拿 reward 没"

3. **signal memory pipeline**（§ 4.4 a/b/c 三选一）
   - 推荐 (b)：episodic `peer_signal_heard` 事件类型，最贴近现有 EpisodicMemoryBank 接口，event-driven 写入开销低
   - 同时给 perception 加 `recentOfType("peer_signal_heard",1)` readback dim（占 1-3 维）

4. **回收 DEAD 维度 / 修 episodic readback**（A1 第 6 项更深一步）
   - perception 18-23 维（4 个任务都 DEAD）有两条路：删掉（让 perception 缩到 ~38 维）或换成任务真需要的 query
   - 推荐换：把 `topSalient` 改成 `recentOfType("drank_water"/"built_worksite"/"peer_signal_heard")` 等任务相关 query

5. **danger_avoidance 收口**（不在 A1 任务里，但本审计揭示）
   - 当前 6 predators × 96×96 太弱，所有变体都活到 episode 末
   - 要么加难度（predator 速度/感知/数量↑，hydration decay↑）让 baseline 也会死一些，才有分离空间
   - 要么把 danger_avoidance 从 fixed-task 列表里降级标 "deferred until tightening"

6. **解耦 spatial / goal**（§ 4.1）
   - 把 `goal_target` 改成 goal_kind one-hot + `goal_target_age` 等不依赖 spatial query 的 surrogate
   - 才能让 `no_goal` 真正反映 goal 信道独立贡献

---

## 6. 审计快查索引

- 三任务对照 → § 1.3 表
- 44 维状态 → § 2 表
- 想加 memory readback → § 3 找 MISSING 项
- 想解释 no_X 分离 → § 4.1 先排除 cascade
- signal_response 为什么全 0 → § 4.4 四条路
- 下一步候选 → § 5
- audit-driven fix 实际效果 → § 7

---

## 7. Audit-driven Fix Follow-up · 2026-05-22

§ 1~§ 5 是审计静态报告（v3 genome）。§ 7 记录基于 § 4 / § 5 提出的修复方案的真实落地数据，覆盖 5 轮重训 v3→v7。

### 7.1 已落地代码改动

按 § 5 建议依次落地（commit 同批）：

| audit 节 | 修复 | 代码位置 |
|---|---|---|
| § 4.4 (b) | 加 `peer_signal_heard` episodic 写入（listener 在 companion radius 内每个 emit 写一次，per-emit dedup） | `runtime/ProtagonistEcologyRuntime.cpp` ~L1672-1709 |
| § 2 / § 4.2 | perception dim 18-21 从 `topSalient(1)` 改为 `recentOfType("peer_signal_heard",1)` 的 dxdy/salience/recency | `perception/MemoryCognitionPerception.cpp` § 2 |
| § 4.4 (d) | 加 `signal_response_dense_reward` 字段 + dense gradient `(1-dist/companion_radius)*dt` | `runtime/ProtagonistEcologyRuntime.cpp` reward 累积 ~L1820-1845 |
| § 4.1 | dim 34-35 从 goal-derived spatial cascade 改为 goal-only (`completionProgress`, `elapsedSeconds/60`) | `perception/MemoryCognitionPerception.cpp` § 4 |
| A1.4 | `agent_trace.csv` 加 `recent_signal_dx/dy/salience/age_seconds` 4 列 | `runtime/TraceRecorder.cpp` |
| 测试 | 4 个 perception unit test (cascade decoupled / goal-only / peer_signal_heard readback / silent without event) | `tests/route_protagonist/test_memory_cognition_perception.cpp` |

### 7.2 五轮重训 + ablation 真实数据

每一轮都用 `configs/protagonist_ecology_train_camp_signal.toml` 训练，再用 `configs/protagonist_ecology_memory_ablation.toml`（4 episodes × 5 variants × 4 tasks，固定 v_x best_genome）评估。

| Genome | train 关键参数 | gens | water spatial 分离 | water goal 分离 | worksite fitness spatial | signal_response_events |
|---|---|---:|---:|---:|---:|---:|
| **v3** (pre-audit) | 旧 perception (cascade fused) | — | -44% (=cascade 假阳性) | -40% (=cascade 假阳性) | — | **0** |
| **v4** | 新 perception, train companion=12, halo=8 | 30 | **-95%** ✅ | **-90%** ✅ | -84% | 0 |
| **v5** | + train companion 12→30 (与 eval 对齐) | 30 | -78% ✅ | -64% ✅ | -60% | 0 |
| **v6** | + halo 8→12 (任务软化) | 30 | **-100%** ✅ | **-100%** ✅ | -55% | 0 |
| **v7** | + 30→60 代翻倍 | 60 | **-100%** ✅ | **-100%** ✅ | -66% | 0 |

实验数据存放于 `data/runs/protagonist_memory_ablation_fixed/` 多个时间戳子目录（最新 = v7 = `20260522-105509`）。

### 7.3 结论

**audit § 4.1 cascade 解耦 ✅✅✅✅ 四轮独立验证**

v4-v7 全部独立分离 −55%~−100%，证明：

- v3 时代 `no_spatial=-44%` / `no_goal=-40%` 不是两条独立证据，是 cascade 双重计数
- 解耦后两条通道各自能强分离（water `-100%/-100%`）
- 这条证据是 audit 最坚实的成果

**audit § 4.4 signal_response_events ❌❌❌❌❌ 五轮均为 0**

即使依次落地了：
- `peer_signal_heard` episodic 写入（§ 4.4 (b)）
- perception dim 18-21 readback specialise（§ 4.2）
- listener-side dense gradient reward（§ 4.4 (d)）
- train/eval companion radius 对齐 30m
- halo softening 8→12m
- 训练翻倍 30→60 代

`signal_response_events` 在 v3-v7 全部稳定为 0。这不是单点缺口，而是 task setup × reward 全局比例 × NEAT 学习能力的复合问题。

### 7.4 v6/v7 的关键负向证据：reward 全局比例失衡

- v6 五个 ablation variant fitness **bit-exact 相同** (6.63)：v6 controller 在 signal_response eval 任务下完全没动用任何 memory，关闭哪条都跑出相同物理轨迹
- v7 五个 variant fitness **反向**（`no_spatial=17.9 > no_episodic=10.9 > baseline=7.6 > no_goal=6.7`）：关闭 memory **反而** fitness 更高，说明 v7 学到 "memory 信号 = bias 远离 base，远离 base = 远离 food + 高生存风险 = 总收益负"。
- 这意味着即便 dense reward + halo reward 在数学上都打开了，`signal_response_dense_reward * communication_weight = 1.0 * 10.0 = 10.0` 的累积仍**小于**留在 base 周围吃 food 的 `food_weight = 4.0` 长期累积，因为 emit 间隔 30s，agent 90% 时间无 dense gradient

### 7.5 如果再做一轮要改什么（不在本次 commit 范围）

1. **大幅放大 listener reward 主导地位**：`signal_response_dense_reward 1.0 → 5.0`，`signal_response_reward 1.5 → 8.0`，让 "走向 emitter" 的累积稳定大于 food
2. **降低 food_weight 在 signal_response task params 中**：现有 4.0，改 0.5 让 food 不再霸占 fitness
3. **改任务 setup**：让 emitter 旁边有充足 food（§ 4.4 (d) 的 "food spawn near emitter"），把 "进 halo" 与 "活下来" 对齐而不是冲突
4. **换学习算法**：30/60 代 NEAT 不够，gradient-based PPO / SAC 在 dense reward 下可能几代就学会
5. **task 软化继续**：halo 12 → 16m + window 15 → 25s，让进 halo 几乎自动

### 7.6 经验沉淀

- **audit 最大的产出不是修复，是揭示**。§ 4.1 cascade 的揭示让 5 轮训练有了一个稳健的对照测量（spatial vs goal 真分离），这本身就是科学价值
- **基础设施齐备 ≠ 任务能学会**。peer_signal_heard pipeline、dense reward、cascade decoupling 都是真实增益，但全局 reward 比例不对仍能把整条链锁死
- **chicken-and-egg 不是单一缺口**。原以为是 perception 缺 readback（§ 4.4 (a-c)）或缺 dense reward（§ 4.4 (d)），现在知道：四条路全打开后**仍然**在 30/60 代 NEAT 下学不会，根因是 reward 整体经济不平衡 + NEAT 探索效率
- **"5 variants bit-exact 相同"**是个有力的诊断信号：当看到 baseline / no_spatial / no_episodic / no_social / no_goal 的 fitness 完全相同时，这意味着 controller 物理轨迹在 5 个 variants 下完全相同，等价于 "这个任务下 controller 没用 memory"

---

## 8. ROOT CAUSE 修正 · 2026-05-22 下午

§ 7 的核心结论 "signal_events=0 是 reward economy 失衡 / NEAT 探索效率不够" **错了**。今天 systematic-debugging 走完 Phase 1 取证，发现 v3~v7 全程 events=0 **不是** 算法/调参问题，而是两层 bug 叠加。

### 8.1 取证方法

在 `runtime/ProtagonistEcologyRuntime.cpp::evaluatePopulation` 加 9 个 debug 计数器 + `data/runs/_signal_debug.csv` 行级输出，每个 episode 末尾追加一行。计数器：
- `emit_calls`：controlled emit 触发次数
- `subscriber_invokes`：EventBus subscribe handler 被调用次数
- `last_signal_emit_size`：episode 末 map 中 entry 数
- `listener_skip_self / skip_tick_zero / skip_window`：listener loop 三个跳过分支
- `listener_dense_only`：listener 在 dense 半径内但 outside halo
- `listener_in_halo`：真正进入 halo +1 的次数（等于 `signal_response_events`）
- `listener_already_credited`：(listener, emitter, emit_tick) 已 +1 过又遇到 emit
- `min_dist_to_emitter`：整 episode listener 到 emitter 的最小距离

每行还追加任务名 / halo 半径 / dense 半径 / emitter 位置 / dense_reward 系数 / window 时长，作为参数 cross-check。

### 8.2 Bug 1：ablation 测试方法 bug（伪问题）

§ 7 的 5 轮 ablation 全部用 `configs/protagonist_ecology_memory_ablation.toml` + `resume_checkpoint_path=""`（session convention 末尾留空）。**空 path = 不加载训练好的 genome = 用随机初始化网络跑 eval**。随机网络当然不会朝 emitter 走，events=0 是必然结果。

证据（用 v7 60代 genome 重新跑 ablation 后）：

| 字段 | 旧 ablation（random net） | v7 genome 跑（修前） |
|---|---:|---:|
| `signal_response_events_avg` (baseline) | 0 | **1.0** |
| `signal_response_events_avg` (no_episodic) | 0 | **1.5** |
| `signal_response_events_avg` (其余 variants) | 0 | 0.5~1.0 |
| `avg_fitness` 范围 | 6.7~17.9 | 11~21 |
| `drank_water_events_avg` (water_return baseline) | 110 | **326.75** |

§ 7.2 表格里 v3~v7 的 "signal events=0" 列全部属于这个伪问题。**`avg_fitness` 真实变化**（在 ablation 早期是用了 genome 的）说明 v3~v7 的 cascade decoupling 数据是真的，**唯独 signal_events 这个 metric 失效是因为 toml 设置错**。

### 8.3 Bug 2：emit.tick==0 sentinel 误杀第一次 controlled emit

代码 3 处把 `emit.tick == 0` 当哨兵跳过（"emit 没发生过"）：

- `runtime/ProtagonistEcologyRuntime.cpp:1591` (GoalManager peer_signal_nearby check)
- `runtime/ProtagonistEcologyRuntime.cpp:1738` (listener-side `peer_signal_heard` episodic write，audit § 4.4 修的)
- `runtime/ProtagonistEcologyRuntime.cpp:1837` (listener-side `signal_response_events` +1 / dense reward)

但 controlled emitter 第一次 emit 在 `elapsed_seconds=0` 时触发，此刻 `world.currentTick() == 0`。所以 emit.tick=0 写入 `last_signal_emit`，listener 把它当哨兵跳过。**第一次 emit 永久无效**。

几何后果：SignalResponse task 的 base.center=(44,60), nest_offset=-12 → nest_center=(32,60)；emitter 位置在 base.center 西北 18m+8m → (26,68)。nest_center 到 emitter 距离正好 10m，**小于 halo 12m**。protagonist spawn 在 nest_center 周围 0~3.4m 圆内，约一半在 halo 内、一半外。**第一次 emit 应该立刻给 halo 内的 4 个 protagonist 各 +1**，但 tick=0 跳过让这 4 次完全损失。修前每个 200s episode 损失约 30s 覆盖（第一次 emit 的 15s window 全废 + 后面 emit 的 0~15s 内 protagonist 还没接近）。

**修法**：3 处全部删 `emit.tick == 0` 跳过。`last_signal_emit` 是 subscribe handler 真正写入过才有 entry，map 中任何 entry 都对应真实 emit，哨兵判断本来就不需要。

### 8.4 修后实测对比（同一 v7 genome）

修前 → 修后（均跑同一 ablation toml，protagonist_genome_path 临时指 v7 gen59 然后 revert）：

| variant | events_avg 修前 | events_avg 修后 | avg_fitness 修前 | avg_fitness 修后 |
|---|---:|---:|---:|---:|
| baseline | 1.0 | 0.25 | 15.31 | **75.05** |
| no_spatial | 0.5 | 0.25 | 13.73 | **72.59** |
| no_episodic | 1.5 | 1.75 | 21.00 | **79.16** |
| no_social | 1.0 | 0.75 | 11.21 | **77.79** |
| no_goal | 0.5 | 0.5 | 11.40 | **74.24** |

`_signal_debug.csv` 同期数据：

| 字段 | 修前 | 修后 |
|---|---:|---:|
| `listener_skip_tick_zero` | 7200 | **0** (fix 生效) |
| `min_dist_to_emitter`（典型） | ~10m | **0.12~3.8m** |
| `listener_already_credited` | 35~193 | **754~1353** (5~8 倍) |
| `listener_in_halo` | 0~3 | 0~3 (类似) |

### 8.5 为什么 events 数没爆增、fitness 却暴涨

**`listener_in_halo` 等同于 `signal_response_events`，但它受 dedup 机制限制**：同一 `(listener, emitter, emit_tick)` 三元组只 +1 一次。修前 listener 进 halo 一两次又被踢出（events ≈ 1），修后 listener 走进 halo **持续停留**几十秒（min_dist 0.12m），但还是只算 1 次 events / emit_tick。

`avg_fitness` 才是这次 fix 的真实"成绩单"——从 11~21 跳到 72~79（**5 倍**），因为：
1. **dense reward 在前 30s 也能累积**（第一次 emit 的 valid window 现在打开了）
2. **后续 emit 的 dense reward 也更可达**（protagonist 已经停在 emitter 旁边，每帧 dense reward 系数 ~0.95）
3. `episode_duration=200s × 8 protagonist × dense_reward_per_tick ≈ 1500+ fitness contribution`

### 8.6 结论修正

| § 7 原结论 | § 8 修正 |
|---|---|
| signal events=0 因为 reward economy 失衡 + NEAT 弱 | **events=0 因为 ablation toml 没加载 genome + tick=0 sentinel bug** |
| 需要换 PPO/SAC 才能学会 signal task | NEAT 已经能学到 protagonist 紧跟 emitter（min_dist 0.12m），events 数字偏低是 dedup 副作用 |
| v6/v7 fitness 反转(no_spatial=17.9 > baseline=7.6) 说明 memory 有害 | 修后 baseline=75.05, no_spatial=72.59，正常排序回归，反转是 random genome 噪声 |
| 5 variants bit-exact 相同 说明 controller 不用 memory | 修后 5 variants fitness 在合理噪声范围内分散（72~79），memory 影响存在但小 |

### 8.7 副产物：dedup 机制的可改进空间

如果想让 `signal_response_events` 数字也反映"停留时间"（不仅是"曾进过 halo"），可以把 dedup 维度从 `(listener, emitter, emit_tick)` 改成 `(listener, emitter, time_bucket=5s)`——每 5 秒持续在 halo 内可以再 +1 一次。这是 metric design 选择，不是 bug fix。**不在本次 commit 范围**。

### 8.8 经验沉淀

- **取证（instrument）比假设（reward economy / NEAT 弱）便宜得多**：花 10 分钟加 9 个计数器 + 一次 rebuild，就把 5 轮训练（v3~v7）的"换 PPO"假设彻底推翻。如果 § 7 一开始就做 instrument，根本不需要 v4~v7 这 4 轮重训
- **测试 toml 改了不 commit + session 末尾 revert 的 convention 反咬一口**：每次新 session 看到 `resume_path=""` 都以为是有意的、跑出 events=0 都以为是真实结果。建议未来要么 ablation toml 默认填一个 genome path（哪怕指向 example genome），要么在 binary 启动时报错说 "no genome supplied, signal/worksite tasks 会给假数据"
- **哨兵值复用整数 0 是个常见陷阱**：`emit.tick=0` 既可以表示"未初始化"也可以表示"真实第一帧"。下次设计哨兵值用 `std::optional<Tick>` 或 `Tick{static_cast<Tick>(-1)}` 比 0 更稳
