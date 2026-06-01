# SPECIES_DIFFERENTIATION_SPEC.md · 8 物种现实差异化设计稿 v2

> **状态**：B 阶段 L2 设计稿 v2（2026-05-22 重写）。**未实施，等用户 review**。
> **v1 已废弃**：v1 是"给每个物种打 reward 标签"思路（30 个 reward 字段），是用户原话"奖励太直接"的反面教材。**v2 是基于 10 篇 paper / 工程实践的"机制压出来"思路**。

---

## 0 · 核心哲学：让"机制相互关联"，不要"奖励标签"

### 0.1 v1 v2 的根本差异

| ❌ v1 思路（奖励标签） | ✅ v2 思路（机制压出来） |
|---|---|
| `pack_kill_bonus_factor 2.5`（成群打 +bonus） | 单狼 damage **不够 kill**，必须 ≥3 同时命中累积 → 自然聚群 |
| `solitary_penalty -0.05/s`（孤立 -分） | drive 模型自带 `loneliness` state；社交才下降；不主动给 -分 |
| `meat_food × 5`（食腐者吃尸体加成） | 食腐者**消化效率**对尸体高、对活猎低（capability 数值，不是 reward） |
| `apex_solo_kill_large_prey_bonus +12` | 老虎一次饱很久（drive 模型），自然不需要打小 prey |

### 0.2 v2 的 4 个层次

| 层 | 内容 | 主要引用 |
|---|---|---|
| **L0 物质条件**（capability + 物理） | damage threshold / 速度 / 隐蔽 / 资源消化效率，让物质本身分化 | Independent PPO Sharks (xa0)、CompetEvo 2024、Nolfi & Floreano、Wolfpack NeurIPS |
| **L1 homeostatic drive** | 5-7 个 drive state，reward = drive_t-1 − drive_t 而不是事件 | Hull 1943、Barto IMRL、City Univ eLife 2014、Continuous Homeostatic RL (HAL 2021) |
| **L2 资源 niche partitioning** | 不同物种用同资源**不同方式**或**不同消化效率** | Chesson 2000、HHMI BioInteractive、Jarman-Bell principle |
| **L3 autocurriculum** | 训练 task 难度自适应（regret-based），多样化 episodes | POET (Wang 2019)、ACCEL (Parker-Holder 2023)、AAMAS 2022 Velocity Ratio、OpenAI Hide-and-Seek |

每层都有具体 paper 直接支持。

### 0.3 防"过度分化" — ESS 多策略共存

> Maynard Smith & Price 1973、Vincent & Brown 1984：现实 ESS 通常是 **多策略稳定共存**（例：97% 狼群 + 3% 独狼）。

设计 reward 不能让"所有狼必须群猎"——要保留独狼的 niche（少数狼能在低密度区独立活）。所以：drive `loneliness` 应该是软压力（孤立活也能活，但不舒服），不是硬 cap。

---

## 1 · L0 · 物质条件分化（capability + 物理）

### 1.1 共同原则

**让物质条件本身造成行为分化**，不是给 reward 标签。
引用：xa0 Independent PPO 鲨鱼速度减半 → emerge 包抄；CompetEvo morph 变化 → emerge wrestling/throwing。

### 1.2 8 物种 capability 改造表

| 物种 | 现状 | v2 改造 | 行为意图 | 引用 |
|---|---|---|---|---|
| **主角** | hunt 5m/dmg 10 | 不变；新加：hunt 时**消耗 stamina**（每秒 attack 累积 fatigue），饱状态下打不动 | 衡量 + 不能持续战 | Hull drive |
| 大型食草 | flee 7m | 加 **alarm pheromone** 半径 6m，附近同物种 5s 内自动 +flee_speed × 1.5 | 鹿群同步逃 | Reynolds boids (HHMI) |
| 小采集 | 现状 | 加 **hide_in_terrain**：在 cave 内 predator 看不见（line-of-sight check） | 钻洞躲 | "ambush_predator" 同款 vision check |
| 顶级掠食 | hunt 6m/dmg 18 | 不变；加 **digest_satiety**：一次 kill 大 prey 后 60s 内 stamina 极低（吃饱了不动） | 独居懒散 | drive 自然产生 |
| 群猎（狼） | hunt 5m/dmg 10 | **关键改**：dmg 10→4，但 prey hp **threshold = 12**（一击不死）；4 狼同时命中累积 16 才能 kill | **单狼必死，必须 ≥3 同时打** | Independent PPO Sharks、Wolfpack NeurIPS |
| 伏击（豹） | hunt 4.5m/dmg 14 | 加 **stealth_state**：静止 ≥3s 进入 stealth → 对 prey 不可见（vision filter）；移动后 1s 失效；从 stealth 突袭 dmg ×2 | 偷袭 efficient，正面战 inefficient | Multi-scale habitat tiger/leopard paper |
| 食腐 | hunt 4m/dmg 7 | 加 **scavenge_efficiency**：吃尸体（meat from kill 事件）energy_gain ×3；活猎 energy_gain ×0.4 | 专吃尸体 | niche partitioning |
| 杂食 | hunt 4m/dmg 7 | 不变；加 **family_protect**：被攻击时同物种 ≤6m 内自动 +charge_speed × 1.5 | 集体冲 | herd antipredator |

### 1.3 关键："damage threshold" 机制（狼群核心）

```cpp
// 当前：每次 hunt 直接扣血
target->setEnergy(target->energy() - actual_damage);

// v2：累积 damage 在 prey 身上的 "threshold"，超过才一次 kill
// 单狼一次 attack 只造成 small damage，prey 能 regen
// 多狼短时间内同时命中累积超过 threshold，prey 才掉血致死
target->accumulateThreatDamage(attacker_id, actual_damage, kThreatWindow);
if (target->threatDamageInWindow() >= target->damageThreshold()) {
    target->setEnergy(target->energy() - target->threatDamageInWindow());
}
```

`threshold` 是 prey-side 属性，不是 attacker-side。large_herbivore threshold=20（结实），small_forager threshold=4（脆弱）。

这是 v2 的**核心机制**——单狼 damage 4 永远 attack 不到 large_herbivore 的 threshold 20，必须 5 狼同时命中（4×5=20）才能扣血一次 kill。

### 1.4 stealth + line-of-sight（豹专精）

```cpp
// vision filter：豹 stealth_state=true 时，prey perception 看不到豹
bool AgentSocialPerception::shouldSeeAgent(self, other) {
    if (other->isStealth() && !directlyAdjacent(self, other)) return false;
    return inSenseRadius(self, other);
}
```

实施在 perception layer，`isStealth()` 是 Agent 字段，由 capability 在 stealth 进入/退出时切换。

---

## 2 · L1 · Homeostatic Drive State

### 2.1 5 个 drive state（所有物种共用，参数物种特化）

引用：City Univ eLife 2014、HAL Continuous Homeostatic RL 2021、Barto IMRL 2013。

| Drive | 描述 | 增长（per tick） | 减少触发 | 主角 setpoint | 备注 |
|---|---|---|---|---|---|
| `hunger` | 饥饿 | +0.001/s × metabolism | food eaten 事件 → ×energy_gain | 0.0 | 饿到 1.0 时持续掉 hp |
| `thirst` | 口渴 | +0.0015/s × metabolism × heat | drink 事件 → ×water_amount | 0.0 | 已有 hydration，可复用 |
| `fatigue` | 疲劳 | +0.0008/s × velocity_magnitude | 静止 + 食物消化 | 0.0 | 影响 hunt damage / move speed |
| `fear` | 恐惧 | predator 进 sense_radius → +0.5 + 0.05/s 持续 | 无 predator 视距内 1s 后 −0.1/s | 0.0 | 影响 perception 噪声 |
| `loneliness` | 孤独 | sense_radius 内无同物种 → +0.001/s | 同物种 ≤6m → −0.005/s | 0.0 | 软压力（独居物种 setpoint=0.4） |

物种特化（**setpoint 不同 = 该物种"舒服"的状态**）：

| 物种 | hunger setpoint | thirst | fatigue | fear setpoint（健康水平） | loneliness setpoint |
|---|---|---|---|---|---|
| 主角 | 0.0 | 0.0 | 0.0 | 0.0 | 0.0 |
| 大型食草 | 0.0 | 0.0 | 0.0 | 0.0 | **0.0**（强群居） |
| 小采集 | 0.0 | 0.0 | 0.0 | 0.0 | 0.2（弱群居） |
| 顶级掠食 | 0.0 | 0.0 | 0.0 | 0.0 | **0.6**（独居） |
| 群猎 | 0.0 | 0.0 | 0.0 | 0.0 | **0.0**（强群居） |
| 伏击 | 0.0 | 0.0 | 0.0 | 0.0 | **0.6**（独居） |
| 食腐 | 0.0 | 0.0 | 0.0 | 0.0 | 0.3（弱群） |
| 杂食 | 0.0 | 0.0 | 0.0 | 0.0 | 0.0（家族） |

### 2.2 Reward 公式

```
reward_t = - sum_drives ( drive_i_t - drive_i_setpoint )^2
          + outcome_specific_rewards (small)
```

也就是 **reward = drive 距离设定点的负平方和**。每个物种"舒服"的状态不同：
- 主角 fear=0 是舒服 → 害怕时 reward 下降，跑回 nest（fear 降）reward 上升
- 顶级掠食 loneliness=0.6 是舒服 → 同类靠近 loneliness=0 → 反而扣分（territorial 排斥自然出现）
- 群猎 loneliness=0 是舒服 → 单狼 loneliness=1.0 → 强烈扣分（必须找同伴）

**关键**：reward 不再是"杀狼 +6"——杀狼降低 fear（害怕的猎物现在被消除），通过 fear 间接给奖。**饱状态下杀狼，fear 已经在 0，杀的 reward 很小**——主角不会"勇敢猛打"。

### 2.3 防 drive collapse

- "stepping back from the brink"（City Univ）：drive 极端值 → 用 cost prediction 避免
  - 实施：drive ≥ 0.85 时 perception "感受" 主导（fear=0.85 → perception 主要看 predator）
  - 此时 NN 决策受 fear 主导，自然恢复
- 多 drive 互相抑制：饱状态下 hunger reward=0，hunt 不再有 reward 来源 → 不会"无脑 spam"

### 2.4 现有系统兼容

- `hydration` 字段已存在 → 直接当 thirst 用
- `health` 字段已存在 → 受 hunger > 1.0 / fear > 1.0 持续扣
- 新增字段：`hunger`, `fatigue`, `fear`, `loneliness` （4 个 drive state，约 +16 bytes/agent）

---

## 3 · L2 · 资源 niche partitioning

引用：Chesson 2000、HHMI Niche Partitioning（kudu/nyala/bushbuck）、Jarman-Bell。

### 3.1 食物 niche 分化

当前所有物种吃一种 plant_food（ResourceLayer）。v2 加 **3 种植物 + 1 种尸体**：

| 资源 | 物种偏好（消化效率） |
|---|---|
| `low_quality_grass`（量多） | 大型食草 1.0 / 小采集 0.5 / 杂食 0.6 / others 0.0 |
| `high_quality_browse`（量少） | 大型食草 0.5 / 小采集 1.0 / 杂食 0.6 / others 0.0 |
| `fruit_seasonal`（季节性） | 杂食 1.0 / 小采集 0.8 / 大型食草 0.3 / 食腐 0.5 |
| `corpse`（来自 kill 事件） | 食腐 1.0 / 杂食 0.4 / 群猎 0.5 / apex 0.5 |

实施：把 `ResourceLayer` 的 `amount` 拆成 4 通道（or 4 个 layer）。每物种 capability 调用 `eatResource(type, amount)` 时按 efficiency 转 hunger 减少。

### 3.2 捕食 niche 分化（已在 L0 covered）

L0 已经定义：
- 老虎独猎大型 prey（damage 18，一击致命）
- 狼群猎中型 prey（damage threshold 必须 ≥3 同时）
- 豹隐蔽伏击（stealth + first strike）
- 食腐者吃 kill 事件留下的 corpse（efficiency ×3）

### 3.3 时空 niche 分化（curriculum 关联）

引用：HHMI termite mound 增加 niche 异质性、Niche partitioning paper 季节性。

L3 训练 task 设计要包含：
- 不同地形 episode：grassland（大型食草占优）/ forest（豹占优）/ rock cave（小采集占优）
- 不同 time_of_day：白天（小采集出洞）/ 晚上（apex 偏夜行）
- 不同季节（已有 SeasonLayer）：fruit 只在 summer 有

---

## 4 · L3 · Autocurriculum（环境 curriculum, 不是 reward shape）

引用：POET (Wang 2019)、ACCEL (Parker-Holder 2023)、AAMAS 2022、OpenAI Hide-and-Seek 2019。

### 4.1 ACCEL-lite：regret-based level selection

不动 reward，只调**训练 task 难度**：

```
每代开始：
  1. 维护 N=20 个 task templates（不同 predator_count、地形、初始资源量）
  2. 上一代 fitness 低（regret 高）的 task → 这一代多采样（×2 episode）
  3. 上一代 fitness 高（regret 低）的 task → 减少采样（×0.5 episode）
  4. 引入 1-2 个新 task（随机 mutate 现有 template）
  5. 太简单（连续 3 代 regret < 5%）的 task 退役
```

效果：训练 distribution 自动跟随 agent 能力。

### 4.2 课程 task templates（v2 启动 set）

| Template | 初始 set | 难度方向 |
|---|---|---|
| `solo_hunt_easy` | 1 主角 + 1 mild prey | 入门，建立 hunt 链 |
| `solo_hunt_predator` | 1 主角 + 1 wolf | 学会 flee（不能打过） |
| `group_vs_pack` | 4 主角 + 3 wolves | 衡量、围殴 |
| `proactive_build` | 4 主角 + 0 predator + tree + worksite | 防微杜渐建造 |
| `wave_attack` | 4 主角 + worksite + 30s 后 5 wolves | 建墙后用墙 |
| `mixed_ecology` | 8 主角 + 8 forager + 6 wolves + 2 apex + ... | 完整生态 |

每代每个 template 跑 4-8 episodes（按 regret 加权）。

### 4.3 与 POET 的关系

POET 完整版需要 environment generator + minimal criterion + novelty + transfer，工程量很大。v2 用 **POET-lite**：
- 手动定义 6 个 templates（上面）
- regret-based 自适应采样
- 不做 transfer（先稳）
- 后续 L3.2 可以加 mutation 把 template 自动变难

---

## 5 · 实施清单

### 5.1 代码改动概览

| 模块 | 改动 | 行数估计 |
|---|---|---|
| `Agent.h/cpp` | 加 4 个 drive state 字段 + getter/setter + tick 更新 + accumulator | +120 |
| `Agent.h` | 加 stealth_state / threat_damage_window 字段 | +30 |
| `core/capability/PredatorHuntCapability.cpp` | damage threshold 机制 + stealth 检查 | +50 |
| `routes/protagonist_ecology/capability/*.cpp` | hunt/move/eat capability 加 drive 调用 | +80 |
| `core/perception/AgentSocialPerception.cpp` | line-of-sight + stealth filter | +40 |
| `routes/protagonist_ecology/world/ResourceLayer.cpp` | 4-channel resource | +60 |
| `routes/protagonist_ecology/runtime/ProtagonistEcologyRuntime.cpp` | reward 函数全改成 drive-delta；archetype 各自的 setpoint | +200 |
| `routes/protagonist_ecology/runtime/BootstrapPopulation.cpp` | capability 数值差异化（狼 dmg 4，threshold；豹 stealth 等） | +40 |
| `routes/protagonist_ecology/runtime/AutoCurriculumLayer.h/cpp` | 新文件，regret-based task selection | +250 |
| Configs | 6 个 task template tomls + a3_v10 配置 | +400 |
| Tests | 各 capability + drive + threshold + stealth + curriculum 单测 | +400 |
| **合计** | | **~1670 行** |

### 5.2 分阶段实施

| 阶段 | 内容 | 工作量 | 重训次数 |
|---|---|---|---|
| **L2.1** | L0 物质条件改造（damage threshold + stealth + scavenge efficiency） + capability 数值差异化 | 4-5 小时 | v10 |
| **L2.2** | L1 drive state 5 个 + reward 函数全改 drive-delta + 物种 setpoint | 6-8 小时 | v11 |
| **L2.3** | L2 资源 niche partitioning（4 channel resource + 物种偏好） | 3-4 小时 | v12 |
| **L2.4** | L3 autocurriculum（regret-based task selection + 6 templates） | 4-6 小时 | v13 |
| **L2.5** | trace 验证 + 数据分析 + 微调 setpoint/threshold/efficiency | 2-3 小时 | v14（如需要） |
| **合计** | | **20-26 小时** | **4-5 次重训** |

每次重训 30-60 分钟（取决于 task template 总数，估计比 v9 慢 1.5x，因为新增 drive 计算 + 多通道 resource）。

### 5.3 验收标准

| 物种 | 验收行为 | 怎么测 |
|---|---|---|
| 主角 | comm_danger_warning：peer < predator 时跑回 nest，peer ≥ predator 时聚群打 | trace 看决策与 perception 关联 |
| 大型食草 | predator 进 sense_radius，整群同向逃 velocity 相关性 > 0.7 | trace 看 velocity 协方差 |
| 小采集 | predator 出现时 80% 个体进 cave 或 base | trace 看位置分布 |
| 顶级掠食 | 3 只虎之间最近间距 > 15m（分散巡视） | trace 看距离 |
| 群猎 | 训练 trace 90% 以上 kill 事件 < 8m 内 ≥3 同伴 | events + position |
| 伏击 | 静止 stealth → 突袭一击 → 逃 模式占 > 50% kill | state machine trace |
| 食腐 | meat_food 中 corpse 来源 > 70% | resource 来源标签 |
| 杂食 | 被攻击时附近 ≥2 同伴的 episode > 50% | event 邻居统计 |

---

## 6 · 风险

| 风险 | 概率 | 缓解 |
|---|---|---|
| L1 drive collapse（某 drive 永远高） | 中 | 防 collapse 机制（§2.3）；先短训 10 代验证 |
| L0 damage threshold 让 prey 不死（系统僵局） | 低 | threshold 设计 = 中型 prey hp × 0.5，4 狼同时打必死 |
| L3 autocurriculum 过度简化（agent 学单一策略） | 中 | 保留 mixed_ecology template；引入 novelty bonus |
| 重训 v10/v11 学偏（fitness 不升） | 高 | 每个阶段独立验证；准备 rollback 路径 |
| 工程量超预估（>30 小时） | 中 | 严格分阶段提交，L2.1 完成可中止 |

---

## 7 · 未在本稿内的（推到 L3+）

- 自定义 perception per archetype（豹自己的 vision range / forager 的 hide perception）
- 多通道 communication（狼吼、鹿警报）
- 物种 capability evolution（CompetEvo 完整版）
- POET 完整 transfer + minimal criterion + env mutation
- 主世界 lifecycle（这是 R-002 / R-006 的事，不是 L2）

---

## 8 · 引用清单

1. **Hull, C. L. (1943).** *Principles of Behavior.* — drive reduction theory
2. **Barto, A. G., et al. (2013).** "Intrinsic Motivation and Reinforcement Learning." — IMRL framework
3. **Keramati, M. & Gutkin, B. (2014).** "Homeostatic reinforcement learning..." *eLife.* — 多维 drive RL
4. **Chesson, P. (2000).** "Mechanisms of maintenance of species diversity." — niche partitioning
5. **xa0.de blog (2019).** "Emergent Behaviour in Multi Agent Reinforcement Learning - Independent PPO" — 任务难度逼合作
6. **Wang, J. et al. (2024).** "Wolfpack Adversarial Attack for Robust MARL." *NeurIPS.* — target+helpers
7. **Parker-Holder, J. et al. (2023).** "Evolving Curricula with Regret-Based Environment Design (ACCEL)." — 自适应难度
8. **Wang, R., Lehman, J., Clune, J., Stanley, K. O. (2019).** "Paired Open-Ended Trailblazer (POET)." *arXiv:1901.01753.*
9. **Baker, B. et al. (2019).** "Emergent Tool Use From Multi-Agent Autocurricula." *arXiv:1909.07528.* — Hide-and-Seek
10. **Maynard Smith, J. & Price, G. R. (1973).** "The logic of animal conflict." *Nature.* — ESS
11. **Vincent, T. L. & Brown, J. S. (1984).** "Stability in an evolutionary game." — predator-prey ESS
12. **Mehta, B. et al. (2019).** "Active Domain Randomization." *PMLR.*
13. **CompetEvo (2024).** "Towards Morphological Evolution from Competition." *arXiv:2405.18300.*
14. **Nolfi, S. & Floreano, D. (1998).** "Co-evolving predator and prey robots." *Artificial Life.*

---

## 9 · 怎么决定 v2 vs v1

| 比较 | v1（30 reward 字段） | v2（4 层机制） |
|---|---|---|
| 工作量 | 6-10 小时 | 20-26 小时 |
| 重训次数 | 1-2 | 4-5 |
| paper 支持 | 弱（自己拍的标签） | 强（10 篇 paper 直接对应） |
| 用户原话符合度 | "奖励太直接 → 反涌现"（违反） | "机制相互关联让物质条件分化"（符合） |
| 真实度 | 中 | 高 |
| 复杂度 | 中 | 高 |
| 稳定性 | 低（reward landscape 容易塌） | 高（drive 多维互相抑制） |

v2 是**用户期望的方向**，但是**比 v1 工作量大 2-3 倍**。是否值得这么大投资取决于用户对"真实度"的优先级。

---

**等用户 review。** 不要在用户确认前动代码。
