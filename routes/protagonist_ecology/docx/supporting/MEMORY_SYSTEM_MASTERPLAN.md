# Protagonist Ecology · 记忆系统总设计（Memory System Masterplan）

> 位置：`routes/protagonist_ecology/docx/supporting/MEMORY_SYSTEM_MASTERPLAN.md`
>
> 范围：只服务 `routes/protagonist_ecology/` 这个当前项目。
>
> 目标：把当前零散的 Memory V2 脚手架，升级成一套**完整、系统、可训练、可分析、可扩展**的主角记忆系统，让主角不再只是“看到刺激就反应”，而是真正能在脑中维持对世界、事件、同伴、目标与因果链的持续表征。

---

## 0. 结论先行

这份文档先把路线说死：

```text
先把记忆系统设计完整
    -> 记忆分区与读写机制成体系
    -> 主行为控制脑同步扩参，不做“大头娃娃”
    -> 因果账本、通信语义、长期目标一起纳入
    -> 再规划 protagonist dense brain 的 GPU 化
    -> 同时把 CPU 并行从 episode 级扩到更能吃满 16C/32T
```

本方案坚持以下核心立场：

- 不是只把 `memory_cells` 从 48 改成 512 就算“有记忆”。
- 不是只做一个很大的记忆区，而主行为控制器还停在小网络。
- 不是把“记住整个世界”理解成逐 tick 全量录像式存储。
- 真正要做的是：**让主角在整个 episode 内，对任务相关的世界区域、事件、同伴关系、目标进程和因果链有持续、可查询、可忘却、可再利用的内部表示。**

一句话版本：

**记忆系统必须和主行为脑一起变大、一起结构化、一起进入闭环。**

---

## 1. 当前代码实况审计

这一节不是空谈理论，而是先把现在代码里到底有什么、缺什么讲清楚。

### 1.1 现在已经存在的东西

当前路线已经有这些记忆相关组件：

- `brain/MemoryBank.*`
- `brain/SpatialMemoryBank.*`
- `brain/EpisodicMemoryBank.*`
- `brain/SocialMemoryBank.*`
- `brain/GoalMemory.*`
- `perception/MemoryCognitionPerception.*`
- `brain/ProtagonistBrain.*`
- `brain/ProtagonistGenome.*`
- `runtime/ProtagonistEcologyRuntime.cpp` 中的一部分 memory 写入逻辑

当前主角脑结构大致是：

```text
external observations + working memory vector
    -> hidden trunk
    -> action outputs
    -> memory candidate / write gate
```

此外，route 还已经有：

- continuous-time 主链
- episode 级 trace
- checkpoint / resume
- 多 episode 评估
- 7 个背景物种共同训练

所以现在不是从 0 开始；问题是**记忆系统只接了一半**。

### 1.2 当前真实闭环状态

| 组件 | 当前状态 | 主要问题 |
|---|---|---|
| `MemoryBank` | 真闭环 | 只是无结构向量短期记忆 |
| `SpatialMemoryBank` | 已接入真实闭环 | 9 个语义槽已拆开，runtime 主要对象类型都在写；但仍缺更深任务化使用 |
| `EpisodicMemoryBank` | 已进入基础决策闭环 | 结构化事件已写入且能经 `MemoryCognitionPerception` 读回，但 query 深度还浅 |
| `SocialMemoryBank` | 已进入基础决策闭环 | cooperation / communication / hostility 已有真实写入与 readback，但还没有通信语义任务和专门指标 |
| `GoalMemory` | 已有最小闭环 | runtime 里的 rule-based 选择逻辑会持续刷新 goal，但还不是层级 Goal System |
| `MemoryCognitionPerception` | 已存在且已扩维 | 当前是 44 维，已能承载 Spatial / Episodic / Social / Goal / DNC 摘要 |
| `ProtagonistBrain` | 已开始结构化扩容 | `BrainV3` 最小壳层已落地，但参数面仍兼容旧 genome，不是完整新脑结构 |

### 1.3 当前代码中的具体缺口

#### 1.3.1 Working memory 是真的，但太原始

`MemoryBank` 是当前唯一真正进入核心前向闭环的记忆。

优点：

- 真正随时间更新
- 有 write gate
- 有 decay
- 参数已进入 `ProtagonistGenome`
- 可保存/恢复 genome 权重

问题：

- 完全没有语义结构
- 不知道哪些槽存水源，哪些槽存风险，哪些槽存近期动作上下文
- 更像“可进化 latent scratchpad”，而不是可解释记忆系统

#### 1.3.2 Spatial memory 不再混槽，但还没有被用到足够深

当前 `SpatialMemoryBank` 已有：

- 32 x 32 global grid
- `water_seen / food_seen / tree_seen / stone_seen / stick_seen / danger_seen / base_seen / fire_seen / worksite_seen / last_visit_age`
- 最近目标查询函数

当前 runtime 已稳定写入的主要有：

- 水源
- 树
- 基地
- 篝火
- 工地
- 石头 / 木棍物体
- 资源
- 危险个体

所以现在比旧版状态更进一步：

- **语义混槽问题已经解决**：`campfire / worksite / base` 已分开写
- 但 **任务化使用仍不足**：当前更多是“可查询记忆”，还不是稳定驱动复杂地点策略的核心

#### 1.3.3 Episodic memory 已经不是纯 write-only，但还远不够强

当前 episodic 主要记录：

- `ate_food`
- `attacked_enemy`
- `killed_prey`
- `chopped_tree`
- `built_worksite`
- `deposited_stick / deposited_stone`
- `signal_emitted`
- `drank_water`
- `saw_predator`

而且现在已经能把：

- `topSalient()`
- `recentOfType()`

通过 `MemoryCognitionPerception` 返回给主角脑作为**基础读回输入**。

但它仍然存在明显限制：

- 读回维度仍然很克制
- query 还没有专门围绕“建造链 / 信号链 / 因果链”设计
- 现在只能说“episodic 已进入基础决策闭环”，还不能说“已经形成强事件记忆策略”

#### 1.3.4 Social memory 已接活，但还没有进入“语义通信阶段”

当前 social memory 已有：

- `recordSighting()`
- `rewardCooperation()`
- `rewardCommunication()`
- `recordHostility()`
- `mostCooperative()`
- `mostHostile()`

但现实状态已经比旧版更进一步：

- `recordSighting()`、`rewardCooperation()`、`rewardCommunication()`、`recordHostility()` 都有真实事件链写入
- `MemoryCognitionPerception` 会把“最佳盟友 / 最近捕食者 / cooperation / communication / hostility”摘要读回 brain
- `TraceRecorder` 也已经能看到 social score 的非零轨迹

它现在能开始支撑：

- 记住谁更像盟友
- 记住最近高风险敌人
- 让 cooperation / communication / hostility 分数进入决策输入

但它仍然**不能**支撑完整的通信语义研究，因为：

- 还没有 scout-worker / danger-warning / build-request 这类专门任务
- 还没有互信息 / ablation / listener shift 这类指标

#### 1.3.5 Goal memory 已经不是“假闭环”，但还只是前层规则系统

`GoalMemory` 类型和枚举本身设计得不差，但现在有一个硬事实：

- runtime 中已经有一套 rule-based goal refresh 逻辑
- 它当前稳定会在 `Flee / FindWater / BuildWorksite / GatherStick / GatherStone / RespondSignal / RestNearFire / FindFood / Explore` 之间切换
- 代码里已经放了 `EmitSignal` 分支，但由于当前 `predator_nearby` 会先被 `Flee` 分支截走，`EmitSignal` 还不能算稳定可达的现行 goal
- `MemoryCognitionPerception` 已把当前 goal / commitment / elapsed / progress 输出给 brain

所以现在不能再说它“基本无效”，更准确的说法是：

- 它已经承担了一个**低频、规则驱动的目标层**
- 但这还不是长期计划式的 Manager/Worker 双脑头
- 也还没有 goal progress metrics 与 long-chain eval

#### 1.3.6 当前主角脑已经不小，但扩容仍是“兼容面优先”

在当前 camp-building 主训练配置里：

- sensory dim = 128
- hidden dim = 512
- memory cells = 48
- action dim = 13
- protagonist controller 参数量约在 `355k` 量级

这说明：

- 主角脑已经不是小玩具
- 但它当前的升级策略仍然是：**先结构拆壳、再考虑参数面扩容**
- 也就是说，`BrainV3` 现在是代码结构语言已经出现，但 `GenomeV2 / Manager-Worker / world-model latent` 这些还没真正落地

### 1.4 当前并行模式对算力利用也不够满

当前 protagonist route 的并行不再只是旧版的 episode 级：

- 外层仍有 **按 episode 并行** 的 `EvaluationPool`
- 内层已经新增 **per-world agent decision threads**
- `compute.agent_decision_threads` 会和外层 episode 并行联动调度

这意味着现在的真实判断应当是：

- CPU 并行基础设施已经明显比“只跑 2~3 个 episode”更深一步
- 但它是否**真的把机器吃满**，还需要 profile，而不能只凭架构推断

这件事很重要，因为它意味着：

- 现在还不是“CPU 已经毫无优化空间了”
- 后续 memory / brain 扩大后，需要同时规划 **CPU 并行升级** 和 **GPU protagonist brain 路线**

---

## 2. 总体设计原则

### 2.1 记忆系统不是单独模块，而是 cognition 主轴

后面主角脑不再是：

```text
感知 -> 大 hidden -> 动作
```

而应该是：

```text
感知 -> 记忆写入/读取 -> 目标更新 -> 行为控制 -> 动作
```

也就是说，memory 不再是“额外附赠的一点 latent state”，而是 cognition 主轴的一部分。

### 2.2 记忆变大，行为控制脑必须一起变大

这是本方案的硬约束：

- 不允许只把 memory bank 变成大容量仓库
- 主行为控制 trunk、goal manager、motor core、action heads 都必须同步扩容
- 否则就会出现“存了很多，但用不动”的大头架构

后面所有参数规模建议都默认遵守这条。

### 2.3 记忆必须显式、可分析、可消融

我们不是要做一个“看起来高级但完全不可解释”的黑盒。

后续每类记忆都必须满足：

- 有清晰职责
- 有清晰写入来源
- 有清晰查询接口
- 能单独 ablation
- 能落到 trace / metrics 上分析

### 2.4 不把整张世界原样塞进脑子，而是做多尺度 memory + query

“记住整个世界”不等于：

- 每 tick 保存全图像
- 每次决策把全世界信息直接喂进网络

真正合理的做法是：

- 用结构化 memory 保存任务相关的世界信息
- 通过 query/read head 每次只取当前决策需要的摘要

### 2.5 写入优先走事件，不优先靠散落在 runtime 各处的手写逻辑

后续各类记忆写入，应尽量基于：

- `EventBus`
- 明确 world events
- 明确 capability outcome events

而不是在 `runtime` 某一大段循环里临时 if/else 塞写入。

### 2.6 必须兼容 continuous-time 项目底座

后续所有记忆：

- 不再强依赖纯 tick 语义
- 要同时支持 `time_seconds`
- 目标更新、记忆衰减、事件 salience 都要能按真实时间处理

### 2.7 必须为后续 GPU/CPU 计算拆分留接口

记忆系统的设计现在就要考虑：

- 哪些部分更适合 CPU 事件逻辑
- 哪些部分更适合 dense tensor compute
- 哪些部分将来可先混合 GPU 化

否则后面会被架构反噬。

---

## 3. 目标架构总览

目标版本记为：`ProtagonistBrainV3`。

总体结构：

```text
ProtagonistBrainV3
├── ObservationEncoder          感知编码器
├── WorkingMemoryBank           多时间尺度短期工作记忆
├── SpatialMemorySystem         全局+局部空间记忆
├── EpisodicMemoryBank          关键事件记忆
├── SocialMemorySystem          同伴/敌人/群体关系记忆
├── GoalManager                 低频持续目标管理
├── BodyStateMemory             身体慢变量记忆
├── CausalMemory / Contribution 外部因果账本与贡献汇总
├── MemoryQueryEncoder          统一记忆查询编码
├── BehaviorFusionTrunk         记忆+感知+目标融合主干
├── MotorCore                   持续动作/CTRNN 子系统
├── ActionHeads                 move/use/build/hunt/signal 等动作头
└── MemoryWriteHeads            working memory 的写门/擦除/保留控制
```

运行信息流：

```text
observations + body state
    -> ObservationEncoder
    -> query memory systems
    -> GoalManager update
    -> BehaviorFusionTrunk
    -> MotorCore
    -> ActionHeads
    -> capabilities / events
    -> EventBus
    -> MemoryWriteCoordinator / ContributionLedger / TraceRecorder
```

这里有两个关键点：

- `WorkingMemory` 更像神经网络内部状态延伸
- `Spatial / Episodic / Social / Goal / Body / Causal` 是显式认知子系统

---

## 4. 记忆分区详细设计

## 4.1 WorkingMemoryBank

### 4.1.1 角色

Working memory 保存：

- 最近数秒内的连续上下文
- 当前行为片段状态
- 正在执行但未完成的短链动作
- recent attention / gating residues

它不是认知地图，也不是事件日志，而是“脑内草稿区”。

### 4.1.2 当前问题

现在的 `MemoryBank` 只有一组同质 cells，缺少：

- 多时间尺度分区
- 结构化槽组
- 独立读写统计

### 4.1.3 目标版本

建议改成多时间尺度结构：

```text
WorkingMemoryBank
├── fast_cells      32~128   适合瞬时动作上下文
├── medium_cells    64~256   适合任务片段上下文
├── slow_cells      32~128   适合更久的内部状态
└── meta_cells      16~64    适合注意力/门控摘要
```

### 4.1.4 写入机制

- write gate 可进化
- erase gate 可进化
- decay 可进化或配置上限
- 支持 group-wise decay，不同组遗忘速度不同

### 4.1.5 查询方式

不直接把全 working memory 裸输出给所有后续模块。

而是：

- 允许对 whole bank 读一份压缩摘要
- 对特定 group 单独读摘要
- 允许 goal manager / motor core 读取不同切片

### 4.1.6 推荐起始规模

```toml
[brain.memory.working]
enabled = true
fast_cells = 64
medium_cells = 128
slow_cells = 64
meta_cells = 32
```

---

## 4.2 SpatialMemorySystem

### 4.2.1 角色

Spatial memory 是“认知地图”，负责让主角记住：

- 哪里有水
- 哪里常出食物
- 哪里危险
- 哪里有树/石头/木棍/尸体
- 哪里是基地/火点/工地/庇护点
- 哪里近期成功过/失败过
- 哪些区域地形代价高

### 4.2.2 当前问题

当前实现只有 global coarse map，而且存在：

- 写入源不全
- `base / fire / worksite` 混槽
- 没有局部 egocentric map
- 没有 success / failure trace
- `danger_seen` 等字段定义了但未系统写入

### 4.2.3 目标结构

建议两层并存：

```text
SpatialMemorySystem
├── GlobalCoarseMap      64x64 or 96x96，长期稳定认知地图
└── LocalEgocentricMap   17x17 or 21x21，以自身为中心的局部动态记忆
```

### 4.2.4 建议通道

建议至少拆到以下通道：

```text
water_seen
food_seen
tree_seen
stone_seen
stick_seen
corpse_seen
predator_seen
base_seen
worksite_seen
fire_seen
shelter_seen
signal_seen
terrain_cost
threat_score
last_visit_age
success_score
failure_score
social_density
```

### 4.2.5 写入来源

写入应来自两类路径：

- **直接感知写入**
  - 看见水源
  - 看见树
  - 看见资源
  - 看见未携带物体
  - 看见火点/工地/基地
  - 感知到 terrain slope/cost

- **事件驱动写入**
  - 在某处被攻击 -> `threat_score + failure_score`
  - 在某处成功喝水/进食/建造 -> `success_score`
  - 在某处收到信号并随后成功 -> `signal_seen + success_score`

### 4.2.6 查询接口

不要把整张 map 直接输入网络。

每 tick 通过 query 输出摘要，例如：

- 最近记忆中的水源方向 + 置信度
- 最近记忆中的危险方向 + 风险分数
- 最近记忆中的可采资源方向
- 最值得回去的工地方向
- 最近成功补水地点方向
- 最近失败/受伤区域方向
- 安全返巢方向
- 高价值社会聚集方向

### 4.2.7 推荐配置

```toml
[brain.memory.spatial]
enabled = true
global_grid = [64, 64]
local_grid = [17, 17]
channels = 18
base_decay = 0.0002
resource_decay = 0.003
danger_decay = 0.008
success_decay = 0.001
failure_decay = 0.002
```

---

## 4.3 EpisodicMemoryBank

### 4.3.1 角色

Episodic memory 记录：

- 关键事件
- 它发生在哪里
- 什么时候发生
- 涉及谁
- 是否成功/失败
- salience 多高

它不是连续状态，也不是地图，而是“值得记住的事”。

### 4.3.2 当前问题

当前 episodic 基本是：

- 事件类型少
- 写入点少
- 没进入决策读回

### 4.3.3 目标事件类型

至少支持：

```text
AteFood
DrankWater
TookDamage
SawPredator
KilledPrey
CraftedSpear
ThrowHit
DepositedStick
DepositedStone
CompletedWorksite
SignalEmitted
SignalResponded
AllyNearbySuccess
DeathNearby
BaseReachedWhileLowHydration
EscapedDanger
```

### 4.3.4 每条事件记录字段

```text
time_seconds
tick
position
event_type
self_agent_id
peer_id(optional)
object_type(optional)
reward_delta
health_delta
hydration_delta
salience
confidence
```

### 4.3.5 读取方式

不要把整个事件列表原样喂给主脑。

而是提供：

- top-k salient events embedding
- recent danger event embedding
- recent water-success event embedding
- recent worksite-success event embedding
- recent signal-success event embedding

### 4.3.6 推荐容量

```toml
[brain.memory.episodic]
enabled = true
capacity = 128
salience_threshold = 0.25
max_query_events = 4
```

---

## 4.4 SocialMemorySystem

### 4.4.1 角色

Social memory 负责让主角记住：

- 谁是常见同伴
- 谁更可能合作
- 谁更危险
- 谁常响应信号
- 谁常一起建造/围猎成功
- 最近哪里常有同伴聚集
- 最近哪里常遇到敌对个体

### 4.4.2 当前问题

当前 social memory 更接近“看见过谁”，而不是“知道谁对我意味着什么”。

### 4.4.3 目标结构

建议拆成两层：

```text
SocialMemorySystem
├── EntityRelationTable      对已见个体/敌人的关系表
└── RegionalSocialTrace      对局部群体活动的区域记忆
```

### 4.4.4 关系字段

```text
peer_id_hash
last_seen_pos
last_seen_time_seconds
species_or_role
cooperation_score
communication_score
hostility_score
co_build_success_count
co_hunt_success_count
responded_signal_count
betrayal_or_collision_count
```

### 4.4.5 写入来源

- 看见同伴/敌人
- 一起完成工地
- 围猎成功
- 对方响应信号
- 对方攻击/阻碍/抢资源

### 4.4.6 查询接口

- 最近最可信同伴方向
- 最近最常响应信号的同伴方向
- 最近高 hostility 目标方向
- 当前局部同伴密度
- 当前局部敌对密度
- 最近共同成功热点方向

### 4.4.7 推荐容量

```toml
[brain.memory.social]
enabled = true
entity_capacity = 64
regional_grid = [32, 32]
regional_channels = 4
max_query_entities = 4
```

---

## 4.5 GoalManager / GoalMemory

### 4.5.1 角色

Goal system 的任务是：

- 低频选择当前目标
- 维持目标一定持续时间
- 允许在强打断条件下切换目标
- 给低层动作系统提供明确“正在干什么”的上下文

### 4.5.2 当前问题

当前 goal 类型虽多，但真闭环不足，导致：

- 目标很可能只是默认枚举值
- 不能承担 long-horizon behavior scaffold

### 4.5.3 目标枚举

建议保留并扩充当前集合：

```text
Explore
ReturnToBase
FindWater
FindFood
GatherStick
GatherStone
ChopTree
BuildWorksite
CraftWeapon
Hunt
Flee
RespondSignal
EmitSignal
RestNearFire
EscortCarry
PatrolDangerZone
```

### 4.5.4 更新机制

建议 goal manager 不是每个 substep 都更新，而是按真实时间低频更新：

- 默认每 `3~12 seconds` 评估一次 goal
- 遇到以下高优先级事件允许立即打断：
  - 严重受伤
  - 严重缺水
  - 近距离捕食威胁
  - 紧急信号

### 4.5.5 输出给主脑的内容

- 当前 goal one-hot / embedding
- commitment
- goal elapsed time
- progress
- interruption pressure

### 4.5.6 推荐配置

```toml
[brain.goal]
enabled = true
update_interval_seconds = 6.0
interrupt_damage_threshold = 0.25
interrupt_thirst_threshold = 0.20
commitment_decay = 0.03
```

---

## 4.6 BodyStateMemory

### 4.6.1 角色

Body state 不是简单把当前 `health/hydration` 再输一遍，而是提供慢变量：

- 饥渴趋势
- 疲劳积累
- 受伤恢复趋势
- 携带负重压力
- 近期高风险经历后的警觉残留

### 4.6.2 为什么需要独立出来

如果没有这类慢变量记忆，主角经常会表现得过于短视：

- 只有掉到阈值以下才突然去喝水
- 刚受伤也立刻回去打
- 长时间劳动和短时间劳动没有内部差异

### 4.6.3 建议字段

```text
thirst_urgency
hunger_urgency
pain_residue
fear_residue
fatigue_residue
carry_burden_residue
warmth_need
social_need
```

---

## 4.7 CausalMemory / ContributionLedger

### 4.7.1 角色

如果主角要“记住世界发生了什么”，那只记地点和事件还不够，还必须逐步记住：

- 为什么成功
- 为什么失败
- 谁贡献了什么
- 某个信号到底有没有带来结果

所以需要 route-level 的因果账本与贡献系统。

### 4.7.2 结构

```text
CausalMemory / ContributionLedger
├── EventLineageGraph
├── WorksiteContributionLedger
├── HuntContributionLedger
├── SignalEffectLedger
└── DelayedOutcomeRegistry
```

### 4.7.3 记录内容

示例：

```text
SawResource -> EmitSignal -> AlliesApproached -> JointCarry -> WorksiteCompleted
SawPredator -> EmitDangerSignal -> AlliesRetreated -> DeathsReduced
ChoppedTree -> StickSpawned -> CarriedToSite -> ShelterCompleted -> NightSurvivalImproved
```

### 4.7.4 作用

- 为 delayed reward 提供依据
- 为 communication semantics 提供证据
- 为主角脑 future query 提供高价值摘要
- 为 trace / replay / eval 提供解释链

### 4.7.5 注意

这一层不一定完全属于“agent 内部神经记忆”，但它是记忆系统总设计的一部分，因为它承载“世界发生了什么以及为什么”的外部结构化记忆。

---

## 5. 行为控制主干同步扩参方案

这一节是本方案最关键的硬约束：

**记忆系统升级时，主行为控制脑必须同步升级。**

## 5.1 当前问题

当前脑增长主要还是：

- bigger hidden trunk
- more memory cells

但还缺：

- 专门的 observation encoder
- 专门的 memory fusion trunk
- 专门的 goal manager 子网络
- 专门的 motor core
- 多动作头分化

## 5.2 目标结构

推荐主角脑主干拆成：

```text
ObservationEncoder
    -> MemoryQueryEncoder
    -> GoalManager head
    -> BehaviorFusionTrunk
    -> MotorCore (CTRNN / leaky recurrent core)
    -> ActionHeads
```

### 5.2.1 ObservationEncoder

职责：

- 压缩当前观测
- 提取局部世界模式
- 作为 memory query / goal update / motor core 的共享输入

### 5.2.2 MemoryQueryEncoder

职责：

- 把 working/spatial/episodic/social/body/goal 读出的信息编码到统一向量空间

### 5.2.3 BehaviorFusionTrunk

职责：

- 融合当前观察 + 所有 memory query + 当前目标
- 输出高层行为倾向

### 5.2.4 MotorCore

职责：

- 处理持续时间动作
- 支撑 move / flee / pursue / carry / build 等连续控制
- 后续适合做 route-local CTRNN 核心

### 5.2.5 ActionHeads

动作头应逐渐分化，而不是完全共享一层输出：

- locomotion head
- resource interaction head
- combat/hunt head
- build/craft head
- signal head
- attention/write-control head

## 5.3 参数规模建议

以下数字是路线档位，不是预先验证真值。

### 5.3.1 当前档

```text
sensory_dim ≈ 128
hidden_dim = 512
memory_cells = 48
read_heads = 4
params ≈ 355k
```

### 5.3.2 第一阶段：记忆闭环版

目标：真正把多记忆区接通，但先不进入极限大脑。

建议：

```text
observation_encoder = 256
behavior_fusion = 768~1024
motor_core = 256
working_memory = 256 total cells
spatial query = 48~96 dims
episodic/social/goal/body query = 64~128 dims total
single agent params ≈ 400k ~ 800k
```

### 5.3.3 第二阶段：复杂行为版

目标：围绕建造、围猎、回巢、信号语义、危险避让进入高阶长链行为。

建议：

```text
observation_encoder = 384~512
behavior_fusion = 1536~2048
goal_manager = 128~256
motor_core = 512
working_memory = 512 total cells
spatial/global+local memory fully active
single agent params ≈ 1.2M ~ 2.5M
```

### 5.3.4 第三阶段：研究档

目标：加入 world model latent、复杂因果记忆、开放式训练。

建议：

```text
single agent params ≈ 3M ~ 8M+
```

这一档不再强要求只靠 CPU。

---

## 6. 新配置与 genome 版本设计

## 6.1 为什么必须做 `ProtagonistGenomeV2 / V3`

当前 `ProtagonistGenome` 主要覆盖：

- input -> hidden
- hidden -> action
- hidden -> working memory candidate/gate

如果按本方案升级，就必须纳入：

- observation encoder 权重
- memory fusion 权重
- goal manager 权重
- motor core 权重
- 分动作头权重
- working memory 多组门控参数
- spatial query/read 参数
- episodic/social query 参数
- body state integrator 参数

所以必须引入 versioned genome schema。

## 6.2 建议配置结构

```toml
[protagonist.brain]
obs_encoder_dim = 256
behavior_fusion_dim = 768
motor_core_dim = 256
action_head_dim = 192

[protagonist.memory.working]
fast_cells = 64
medium_cells = 128
slow_cells = 64
meta_cells = 32

[protagonist.memory.spatial]
global_grid = [64, 64]
local_grid = [17, 17]
channels = 18
query_dim = 64

[protagonist.memory.episodic]
capacity = 128
query_events = 4

[protagonist.memory.social]
entity_capacity = 64
query_entities = 4

[protagonist.goal]
update_interval_seconds = 6.0
dim = 16

[protagonist.body_state]
dim = 8
```

---

## 7. 文件落点与工程映射

## 7.1 现有文件需修改

- `brain/ProtagonistBrain.h/.cpp`
- `brain/ProtagonistGenome.h/.cpp`
- `perception/MemoryCognitionPerception.h/.cpp`
- `runtime/ProtagonistEcologyRuntime.cpp`
- `runtime/BootstrapPopulation.cpp`
- `runtime/TraceRecorder.h/.cpp`
- 主角相关配置文件

## 7.2 建议新增文件

- `brain/WorkingMemoryBank.h/.cpp`
- `brain/BodyStateMemory.h/.cpp`
- `brain/MemoryQueryEncoder.h/.cpp`
- `runtime/MemoryWriteCoordinator.h/.cpp`
- `runtime/EventLineageTracker.h/.cpp`
- `runtime/ContributionLedger.h/.cpp`
- `runtime/WorldModelDatasetBuilder.h/.cpp`
- `runtime/CommunicationMetricsTracker.h/.cpp`

## 7.3 工程职责分工

- `brain/*`：agent 内部状态与神经控制逻辑
- `perception/*`：把 memory query 转成 brain 输入
- `runtime/*`：memory 写入协调、因果账本、trace、dataset builder
- `world/*`：提供可订阅的事件与世界状态来源

---

## 8. 分阶段落地路线

## 8.1 Phase A · 先把当前假闭环修成真闭环

### 目标

不引入巨量新结构，先把当前 Memory V2 里那些“看起来有，实际上没接通”的地方接实。

### 必做项

- 让 `GoalMemory` 真正由 goal manager 更新，而不是默认值常驻
- 修正 `SpatialMemory` 通道混槽
- 把 `danger/tree/fire/worksite/base` 等通道写全
- 扩展 `MemoryCognitionPerception`，不再只有 12 维
- 把 episodic top-k 事件真正读回决策输入
- 把 social query 真正读回决策输入
- 把 cooperation/communication/hostility 写入链接通
- memory 写入改成更事件驱动

### 完成标准

- goal 不再长期停在默认 `Explore`
- spatial channels 在 trace/metrics 中可观测
- episodic/social 不再只是 write-only
- ablation 能看出这些 memory 的真实作用

## 8.2 Phase B · ProtagonistBrainV3 第一版

### 目标

让主角脑第一次从“单一 dense trunk + memory vector”升级成“真正的 cognition architecture”。

### 必做项

- `ObservationEncoder`
- `MemoryQueryEncoder`
- 多时间尺度 `WorkingMemory`
- `BodyStateMemory`
- `BehaviorFusionTrunk`
- `MotorCore`
- `ActionHeads` 初步分化
- `ProtagonistGenomeV2`

### 完成标准

- 单 agent 参数量进入 `400k ~ 800k`
- water return / danger avoid / worksite return / signal response 四类测试显著提升
- 关闭某类 memory 后行为明显回退

## 8.3 Phase C · 因果记忆与合作信用

### 目标

把“记住发生了什么”推进到“记住为什么发生，以及谁贡献了什么”。

### 必做项

- `EventLineageTracker`
- `ContributionLedger`
- worksite / hunt / signal delayed credit
- communication metrics
- causal trace 输出

### 完成标准

- 路过者不再吃主要合作分红
- 信号只有造成真实后果才高价值
- 围猎/建造贡献可被解释

## 8.4 Phase D · World Model 入口

### 目标

让主角不只“记住过去”，还开始“预测未来”。

### 必做项

- world model dataset builder
- offline predictor baseline
- prediction error metrics
- curiosity 改造入口

### 完成标准

- 可以训练 observation-action-next-state predictor
- 能把 latent/prediction error 作为输入或指标使用

## 8.5 Phase E · 计算路线承接

这一阶段不在 memory 总设计里直接实现，但必须在架构上预留：

- protagonist dense brain mixed GPU
- CPU 并行从 episode 级升级
- batched protagonist inference

---

## 9. 验收测试与指标体系

记忆系统完成不能只看 `fitness.csv`。

必须新增至少这些测试口径：

## 9.1 功能性测试

- **Water return test**
  - 曾见过水源，远离后缺水，能否回去

- **Danger avoidance test**
  - 某区域曾受攻击，后续是否显著绕开

- **Worksite return test**
  - 离开工地后，是否能带材料回去

- **Home return test**
  - 夜间/受伤/低水后是否主动回基地或火点

- **Signal response test**
  - 听到信号后是否朝正确目标或区域移动

- **Social partner reuse test**
  - 是否更倾向靠近高 cooperation / high response 同伴

## 9.2 指标文件建议

### `memory_metrics.csv`

```text
generation
water_return_rate
danger_avoid_rate
worksite_return_rate
home_return_rate
signal_response_rate
social_recontact_rate
goal_persistence_seconds_avg
episodic_reuse_rate
memory_ablation_drop
```

### `goal_metrics.csv`

```text
generation
goal_switch_count
goal_duration_avg
goal_interrupt_count
goal_success_rate
```

### `communication_metrics.csv`

```text
generation
channel
emit_count
response_count
response_latency_avg
signal_success_rate
signal_ablation_drop
```

### `causal_credit_metrics.csv`

```text
generation
worksite_credit_precision
hunt_credit_precision
signal_credit_precision
passerby_reward_ratio
```

## 9.3 必做消融

至少要能单独关闭：

- working memory
- spatial memory
- episodic memory
- social memory
- goal manager
- causal ledger reward

只要这些东西不能单独消融并看到行为退化，就不能说系统真的完成了。

---

## 10. 对 GPU / CPU 路线的承接条件

这一节不是现在就去实现 GPU，而是给后续算力路线定前提。

## 10.1 GPU 不应该先于记忆架构稳定发生

现在就把全项目重构成 GPU 版，不是最佳顺序。

正确顺序是：

- 先稳定 memory architecture
- 再稳定 protagonist brain 扩参结构
- 再做 protagonist dense compute 的 GPU 化

## 10.2 什么时候开始认真做 protagonist brain GPU 化

建议触发条件：

- `ProtagonistBrainV3` 架构已稳定
- 单 agent dense 主脑参数量进入 `0.5M ~ 1M+`
- profiling 显示 protagonist brain inference 成为主要耗时之一
- query / fusion / motor core 的 dense 部分已经清晰可拆

## 10.3 CPU 并行必须同时升级

当前 route 已不再只是 episode 级并行：外层有 episode/world 副本并行，内层已有 agent decision threads；但是否真正吃满 16C/32T，仍要看 profiling 与具体配置。

后续应规划：

- episode-level parallel
- in-world agent decision parallel
- species evaluation parallel
- protagonist dense inference batching
- trace / logging / metrics 异步化

换句话说，后续不是二选一：

- 不是“只上 GPU”
- 也不是“只压 CPU”

而是：

**记忆系统稳定后，主角 dense brain 走混合 GPU，世界逻辑和事件系统继续深挖 CPU 并行。**

---

## 11. 最终目标画面

当这套方案真正落地后，主角应该开始表现出这些能力：

- 见过的水源、基地、工地、火点、危险区会在后续行为中被稳定重用
- 被攻击过的区域会在后续路径中留下明显回避痕迹
- 目标不再每 tick 摇摆，而会持续几秒到几十秒
- 特定同伴开始被“偏爱”或“规避”
- 信号不再只是发了就算，而是开始和后续群体行为、收益绑定
- 建造、围猎、返巢、避险会更像连续因果链，而不是零散高分动作
- trace / replay 可以回答“它为什么去那”“为什么这次成功”“谁起了关键作用”

这时我们才能说：

**主角开始在脑中维持一个真正的世界。**

---

## 12. 一句话总纲

后续 protagonist 记忆系统的唯一正确方向是：

**把 working / spatial / episodic / social / goal / body / causal 这些记忆区做成真闭环，同时让主行为控制脑同步变大、同步结构化，再在这个稳定架构上承接 GPU protagonist brain 和更满载的 CPU 并行。**
