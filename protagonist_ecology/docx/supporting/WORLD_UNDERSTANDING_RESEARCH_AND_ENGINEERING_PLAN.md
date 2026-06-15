# Protagonist Ecology · 世界理解与复杂行为涌现长期研究工程方案

> 位置：`routes/protagonist_ecology/docx/supporting/WORLD_UNDERSTANDING_RESEARCH_AND_ENGINEERING_PLAN.md`
>
> 范围：只服务 `routes/protagonist_ecology/` 这个当前项目。
>
> 目标：不是 MVP，不是“先随便做个能跑的”。这份文档面向长期、系统、可持续训练的完整方案：让主角群体从“能刷行为奖励”逐步走向“能在世界中形成记忆、地点概念、因果链、协作、信号语义、泛化能力和复杂生态行为”。

---

## 0. 结论先行

当前 protagonist 生态路线已经证明：系统不是空转。

最近一次中规模训练结果：

```text
配置：configs/protagonist_ecology_train_camp_building_evolve.toml
训练：120 generations × 3 episodes × 1400 ticks
运行目录：data/runs/protagonist_train_camp_building/20260520-134723

主角 best fitness peak = 3855.02
主角 final avg fitness = 586.36
tail20 avg fitness = 614.44

completed_worksites = 218
craft_successes = 2021
throw_hits = 843
hunt_successes = 4509
signal_co_attendance = 82864
checkpoint_gen119 = 24 protagonist genomes
```

这说明当前训练集已经能推动出：

- 采集
- 搬运
- 砍树
- 做矛
- 投掷
- 狩猎
- 建造
- 信号共现
- 多物种生态压力下的行为适应

但是，如果目标是“理解自己所在的世界”并进一步涌现复杂行为，当前系统仍然主要卡在以下瓶颈：

1. **记忆结构太弱**：只有一段向量式短期 memory，不足以形成地点、事件、路线、危险区和同伴状态。
2. **局部反应多，世界模型弱**：agent 更像“看见刺激就反应”，不是“记住世界后规划”。
3. **奖励仍偏行为计数**：容易训练出刷分行为，而不是真正因果链行为。
4. **合作信用分配粗**：谁真正贡献了建造、围猎、通信，当前统计不够精细。
5. **通信还未被迫形成语义**：现在信号有共现，但不是稳定“词义”。
6. **课程还不够开放式**：固定训练场会导致固定套路，难以持续产生新行为。
7. **缺少完整 replay / trace**：无法精确判断涌现是真机制还是日志假象，也不利于未来 3D 可视化。
8. **缺少完整 resume 与大规模实验制度**：有 genome 和 checkpoint 存档，但还未形成跨运行持续进化闭环。

本方案的核心路线是：

```text
先补观察与可恢复性
    → 再升级专门记忆区
    → 再做因果信用分配
    → 再设计通信/合作任务
    → 再引入开放式课程与质量多样性
    → 再加世界模型与层级目标
    → 最后做长期大规模训练 + replay + 3D 展示
```

---

## 1. 文献调研依据

下面不是为了“显得学术”，而是为了给工程方案找可落地的方向。

### 1.1 NEAT 与神经进化

**Stanley & Miikkulainen, 2002, Evolving Neural Networks through Augmenting Topologies**

- 关键词：NEAT、拓扑增长、speciation、历史标记、从简单到复杂。
- 与本项目关系：项目已有 NEAT 系统和多物种演化基础。长期目标不是只调固定拓扑权重，而是继续利用结构增长、物种保护和多样性维持，防止复杂行为早期被淘汰。
- 工程启发：
  - 主角路线当前 `ProtagonistGenome` 是固定拓扑大网络；后续可升级为“模块化固定大脑 + 可进化连接/门控”的混合结构。
  - 背景物种继续使用 `NeatGenome / NeatEvolution`，主角可单独走更复杂的 genome v2。

### 1.2 CTRNN 与连续时间动态行为

**Beer / evolutionary robotics / CTRNN 相关工作**

- 关键词：连续时间循环神经网络、动态系统、适应行为、时序控制。
- 与本项目关系：项目目标中本来就包含 CTRNN / 神经调节方向。当前 `ProtagonistBrain` 是离散 tick 的 gated memory MLP，不是真正连续动力系统。
- 工程启发：
  - 对运动、逃跑、围猎、追踪、节律行为，CTRNN 比纯前馈输出更自然。
  - 后续可以把 movement / approach / flee / pursuit 交给一个小型 CTRNN motor core；高级目标系统只调制其参数。

### 1.3 空间记忆与认知地图

**Parisotto & Salakhutdinov, 2017, Neural Map: Structured Memory for Deep Reinforcement Learning**

论文摘要重点：智能体在部分可观测环境中需要 memory；简单 LSTM 或最近 k 帧不够。Neural Map 使用空间结构化 2D memory，把写入地址与 agent 当前位置对应，使 agent 能长期存储环境信息，并在 2D/3D maze 中泛化。

- 关键词：结构化空间记忆、2D memory image、partial observability、maze generalization。
- 与本项目关系：当前 agent 在连续 2D 世界里活动，有地形、水、树、工地、营地、危险区、信号场。最需要的不是再塞更多瞬时输入，而是让 agent 拥有“地图”。
- 工程启发：
  - 增加 route-local `SpatialMemoryLayer` 或 agent-local `SpatialMemoryBank`。
  - 按世界坐标或 egocentric 坐标写入水源、资源、危险、基地、工地、尸体、同伴聚集等通道。
  - 感知不再只返回最近目标，而是返回若干 memory query 结果。

**Zou et al., 2021, Neuroevolution of a Recurrent Neural Network for Spatial and Working Memory in a Simulated Robotic Environment**

- 关键词：神经进化、空间记忆、工作记忆、认知地图、模拟机器人。
- 与本项目关系：这是直接支持“神经进化也可以做空间/工作记忆”的相关方向。
- 工程启发：
  - 不必完全改成梯度 RL；可以继续用进化搜索，但必须给网络足够结构化的记忆接口。

### 1.4 外部记忆与长期变量存储

**Graves et al., 2016, Hybrid computing using a neural network with dynamic external memory / Differentiable Neural Computer**

论文摘要重点：普通神经网络擅长感知和序列学习，但缺少能长期存储变量和数据结构的外部记忆；DNC 让神经网络可读写外部 memory matrix。

- 关键词：external memory、read/write head、memory matrix、long-term storage。
- 与本项目关系：主角 agent 需要记住：哪个位置有水、哪里危险、谁刚才发信号、哪个工地缺材料、自己当前目标是什么。
- 工程启发：
  - 不一定实现完整 DNC；但可以实现可进化读写门控的多区 memory matrix。
  - 记忆不应只是一个 `memory_cells` 向量，而应分区：工作记忆、空间记忆、事件记忆、社会记忆、目标记忆。

### 1.5 World Models 与预测式世界理解

**Ha & Schmidhuber, 2018, World Models**

论文摘要重点：通过无监督方式训练生成式世界模型，学习压缩的空间和时间表示；agent 可以使用 world model 特征作为输入，甚至在 hallucinated dream 中训练策略。

- 关键词：world model、latent dynamics、predictive model、dream training。
- 与本项目关系：如果我们要说 agent “理解世界”，最好不只是 memory，而是能预测：我这么做会发生什么？水会在哪里？同伴会不会响应？捕食者会不会靠近？
- 工程启发：
  - 先不需要完整梦境训练；第一步是训练/进化一个 observation-action-next-observation predictor。
  - 用预测误差作为 curiosity，也用预测特征作为 agent 的额外输入。

### 1.6 好奇心与探索

**Pathak et al., 2017, Curiosity-driven Exploration by Self-supervised Prediction**

论文摘要重点：外部奖励稀疏时，curiosity 可作为 intrinsic reward；通过预测自身动作后果的误差驱动探索，并忽略 agent 无法影响的环境部分。

**Burda et al., 2018, Exploration by Random Network Distillation**

论文摘要重点：RND 用固定随机网络特征和预测网络误差作为探索 bonus，在困难探索 Atari 游戏中有效。

- 关键词：intrinsic reward、prediction error、novelty、hard exploration。
- 与本项目关系：复杂行为不会只靠手写奖励出现。需要让 agent 主动探索新地点、新组合、新工具链。
- 工程启发：
  - 当前 `evaluateIntrinsicReward()` 只奖励新格子，太浅。
  - 后续应升级为 state novelty / event novelty / causal novelty / social novelty。

### 1.7 层级控制与长期目标

**Vezhnevets et al., 2017, FeUdal Networks for Hierarchical Reinforcement Learning**

论文摘要重点：Manager 低频设置抽象目标，Worker 高频执行原始动作；这种结构有利于长时间尺度信用分配和子策略涌现。

- 关键词：hierarchical RL、manager-worker、long horizon credit assignment、sub-policy。
- 与本项目关系：当前主角每 tick 输出所有动作，很容易短视。建造、找水、围猎都是长链任务。
- 工程启发：
  - 引入 Goal/Intention head：每 64 或 128 ticks 选择目标。
  - Action head 根据当前目标执行低层动作。
  - 目标包括：喝水、找食物、采集、搬运、建造、狩猎、逃跑、回应信号、探索。

### 1.8 多智能体通信

**Foerster et al., 2016, Learning to Communicate with Deep Multi-Agent Reinforcement Learning**

论文摘要重点：多 agent 在部分可观测任务中需要学习通信协议；提出 RIAL / DIAL，强调 centralized learning decentralized execution。

**Lazaridou et al., 2017, Emergence of Grounded Compositional Language in Multi-Agent Populations**

论文摘要重点：多 agent 任务中可以涌现基本组合语言；也会出现非语言沟通，如 pointing 和 guiding。

- 关键词：emergent communication、partial observability、grounded language、signaling。
- 与本项目关系：当前已有 `SignalLayer` 和 `EmitSignalCapability`，但信号只是数值场和 reward 共现，尚无语义压力。
- 工程启发：
  - 设计必须通信才能解决的任务。
  - 做信号 ablation：关闭信号后任务成功率明显下降，才能证明信号有意义。
  - 统计信号与环境状态 / 行为响应之间的互信息。

### 1.9 多智能体信用分配

**Foerster et al., 2018, Counterfactual Multi-Agent Policy Gradients / COMA**

论文摘要重点：COMA 使用 centralized critic 和 decentralized actors，并用 counterfactual baseline 解决多 agent credit assignment：比较某 agent 当前动作与替换成默认动作时的全局回报差异。

**Rashid et al., 2018, QMIX**

论文摘要重点：集中训练、分散执行；用 monotonic mixing network 把个体 action-values 混合成团队价值。

- 关键词：centralized training decentralized execution、credit assignment、counterfactual baseline、value decomposition。
- 与本项目关系：当前合作 reward 粗略分红，容易奖励路过者。
- 工程启发：
  - 不一定上完整 actor-critic，但要引入“贡献账本”和“差分奖励”。
  - 对建造、围猎、通信响应建立 event lineage，按真实贡献分配 reward。

### 1.10 质量多样性与开放式演化

**Lehman & Stanley, 2011, Abandoning Objectives: Evolution Through the Search for Novelty Alone**

- 关键词：novelty search、deceptive objective、open-endedness。
- 工程启发：复杂行为可能不是直接优化目标带来的，而是通过探索新行为形态出现的。

**Mouret & Clune, 2015, MAP-Elites**

论文摘要重点：不是只找单一最优解，而是在用户定义的行为维度空间里保存每个格子的 elite，从而得到高质量且多样的解。

- 工程启发：保存不同类型主角：高建造、高狩猎、高通信、高探索、高生存、高合作，而不是只保存单一 best fitness。

**Wang et al., 2019, POET**

论文摘要重点：环境和 agent 一起开放式演化，自动产生越来越复杂和多样的环境挑战；解可以跨环境迁移，成为 stepping stones。

- 工程启发：不要永远只训一个 camp_building 世界。让地图、资源、捕食者、季节、任务结构也演化。

---

## 2. 当前 protagonist route 的工程现状

### 2.1 已有能力

当前路线已有以下核心能力：

- 独立 route：`routes/protagonist_ecology/`
- 独立可执行：`neural-eco-protagonist`
- 主角 brain：`brain/ProtagonistBrain.*`
- 主角 genome：`brain/ProtagonistGenome.*`
- 主角 evolution：`runtime/ProtagonistEvolution.*`
- 多物种背景：`SpeciesArchetype` + 7 个背景物种
- 世界层：水、地形、资源、对象、树、营地、工地、篝火、信号、疾病等
- 能力：移动、吃、喝水、砍树、做矛、投掷、狩猎、逃跑、发信号、搬运/放下
- 感知：自身、食物、水、同伴/敌人、树、携带物、库存、武器、物体物流、危险、工地、地形、信号
- 训练：多 episode、多线程 `EvaluationPool`
- 输出：`fitness.csv`、`evolution.csv`、`species_metrics.csv`、`protagonist_behaviors.csv`
- 存档：`best_genome_*`、`checkpoint_genN/genome_*.bin`
- eval 加载：`runtime.protagonist_genome_path`
- Trace / Resume：`event_trace.jsonl`、`agent_trace.csv`、`world_trace.csv`、`replay_manifest.json`、`checkpoint_state.toml`
- continuous-time：`episode_duration_seconds + substep_dt`
- 标准 DNC 主链：`DNCMemory` 已接入 protagonist brain / genome / evolution / runtime
- Memory Phase A：`Spatial / Episodic / Social / Goal` 四类显式 memory 已进入主角闭环
- `MemoryCognitionPerception` 已扩到 44 维，并带 DNC 活动摘要
- worksite / hunt credit 已有 partial 实装（但不是完整 EventLineage）
- world model 数据集脚本与 offline MAP-Elites 脚本/Archive 已有第一版
- DNC-forcing micro-task 已完成最小 build/test/smoke 验证
- `ProtagonistBrainV3` 最小壳层已落地，并开始把 body/working summaries 折回现有 learnable path
- CUDA build 路径已可编译运行，但当前 camp-building 训练总耗时尚未证明优于 CPU

### 2.2 当前大脑结构

当前 `ProtagonistBrain` 本质是：

```text
external observations
 + MemoryCognitionPerception(44 dims)
 + DNC read vectors
    -> dense controller trunk
    -> action outputs
    -> DNC interface decode
```

同时它已经不再只是“一个大 dense 块”：

```text
ObservationEncoder
MemoryQueryEncoder
BodyStateMemory
WorkingMemoryBank
BehaviorFusionTrunk
MotorCore
ActionHeads
InterfaceHead
```

优点：

- 仍保持旧 genome / DNC / dense GPU 兼容面
- `BodyStateMemory.summary()` 已折叠回 observation slice
- `WorkingMemoryBank.summary()` 已折叠回 memory-query slice
- `WorkingMemoryBank.summary()` 还会继续调制 `motor_target`
- 因此 body / working memory 已经开始进入现有 learnable path，而不再只是旁路状态对象

不足：

- `ProtagonistBrainV3` 目前仍是**结构拆壳优先**，不是完整的新参数面
- `ProtagonistGenomeV2` 还没做，Manager/Worker 双脑头还没做
- working/body memory 还不是独立参数化的主路径
- DNC micro-task 当前只完成了最小验证，不能证明“大脑已经学会系统性用记忆”

### 2.3 当前奖励结构

主角 reward 当前包括：

- survival
- food
- intrinsic new-cell
- combat damage/kill
- worksite proximity/completion
- social proximity
- signal emit with companions
- signal co-attendance with food
- joint hunt proximity

这已经足以推动中等复杂行为，但还不够让行为变成“理解世界”。

---

## 3. 总体设计原则

### 3.1 不再只问“能不能跑”

后续阶段的目标不是 smoke pass，而是：

- 能解释行为从哪里来
- 能复现实验结果
- 能 ablation 证明机制必要
- 能在陌生地图泛化
- 能被 replay 和未来 3D 面板展示

### 3.2 只读可视化，不污染训练逻辑

未来 3D 面板读取：

```text
World2D 状态
Agent 状态
EventBus 事件
Trace 文件
```

3D 不能反向影响训练核心。

### 3.3 先做观测，再做大改

复杂系统如果没有 trace，就无法知道失败原因。因此顺序必须是：

```text
trace / resume / experiment metadata
    -> memory / reward / curriculum / world model
```

### 3.4 允许大模型和高资源

用户明确表示时间和资源不是主要限制。因此可采用：

- 更大的 hidden dim
- 更大的 memory 区
- 多进程、多 seed、长训
- 质量多样性 archive
- world model 辅助训练
- replay trace 大量落盘

---

## 4. 瓶颈一：记忆不足

### 4.1 当前问题

当前 `memory_cells=48`，且是无结构向量。

即使把它直接提高到 512 或 1024，也只能缓解容量，不会自动得到“地图”。真正问题是：

- 不知道哪个 memory cell 对应水源
- 不知道哪个 cell 对应危险
- 不知道哪个 cell 对应某个坐标
- 不知道哪个 cell 对应某个同伴
- 不知道哪些记忆应该保留很久
- 不知道哪些记忆应该快速衰减

### 4.2 目标

把主角大脑升级为“专门记忆区”结构：

```text
ProtagonistBrainV2
├── WorkingMemory       短期连续状态
├── SpatialMemory       空间地图 / 认知地图
├── EpisodicMemory      事件记忆
├── SocialMemory        同伴/敌人记忆
├── GoalMemory          当前目标/意图
├── BodyStateMemory     饥饿、缺水、受伤、携带状态的慢变量
└── MotorCore           动作控制 / CTRNN 子模块
```

### 4.3 具体方案

#### 4.3.1 WorkingMemory

用途：保存最近几十 tick 的动态上下文。

建议配置：

```toml
[brain.memory.working]
enabled = true
cells = 256
decay_min = 0.01
decay_max = 0.20
```

工程实现：

- 扩展 `MemoryBank` 为 `WorkingMemoryBank`。
- 写入门由 genome 参数控制。
- decay 可进化或配置。

#### 4.3.2 SpatialMemory

用途：形成认知地图。

推荐两套并存：

1. **Global coarse map**：世界坐标格子。
2. **Egocentric local map**：以自身为中心的局部记忆。

建议通道：

```text
water_seen
food_seen
tree_seen
stone_seen
stick_seen
corpse_seen
predator_seen
safe_base
worksite_seen
fire_seen
signal_seen
terrain_cost
danger_score
last_visit_age
success_score
```

建议配置：

```toml
[brain.memory.spatial]
enabled = true
global_grid = [64, 64]
local_grid = [17, 17]
channels = 14
write_decay = 0.002
visit_decay = 0.0005
```

工程落点：

- 新增 `brain/SpatialMemoryBank.h/cpp`
- 新增 `perception/SpatialMemoryPerception.h/cpp`
- `ProtagonistBrain` 输入增加 spatial query 向量
- `ProtagonistGenome` 增加 spatial read/write 参数
- `ProtagonistGenome::saveToFile()` 升级 version

关键点：

- 不需要把整张 map 都输入 brain。
- 每 tick 只输入若干 query：
  - nearest remembered water direction
  - nearest remembered danger direction
  - nearest remembered resource direction
  - best remembered safe direction
  - remembered worksite direction

#### 4.3.3 EpisodicMemory

用途：记住发生过的关键事件。

事件类型：

```text
AteFood
DrankWater
TookDamage
KilledPrey
SawPredator
DepositedObject
CompletedWorksite
CraftedSpear
ThrowHit
SignalEmitted
SignalResponded
AllyNearbySuccess
DeathNearby
```

每条事件记录：

```text
tick
position
event_type
agent_id
peer_id(optional)
object_type(optional)
reward_delta
salience
```

工程落点：

- 新增 `brain/EpisodicMemoryBank.*`
- 从 `EventBus` 订阅事件写入
- 提供 top-k salient events 给 brain

#### 4.3.4 SocialMemory

用途：让 agent 记住同伴和敌人的趋势。

记录维度：

```text
peer_id_hash
last_seen_pos
last_seen_tick
species/archetype
was_helpful
was_dangerous
responded_to_signal_count
co_hunt_success_count
co_build_success_count
```

如果不做 identity，也至少做“局部群体记忆”：

```text
最近同伴聚集点
最近捕食者高发区
最近共同成功区域
```

#### 4.3.5 GoalMemory

用途：让 agent 有持续目标，而不是每 tick 改主意。

目标枚举：

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
```

目标每 K tick 更新一次，例如：

```toml
[brain.goal]
update_interval_ticks = 64
goal_dim = 14
commitment_decay = 0.03
```

### 4.4 参数规模建议

当前不要害怕参数增加。建议分档：

```text
当前档：hidden=512, memory=48
增强档：hidden=1024, working_memory=256, spatial_query=64, goal_dim=14
高档：hidden=2048, working_memory=512, spatial_memory=64x64x14, episodic=128 events
研究档：多模块网络 + 外部 memory + world model latent，单 agent 参数可上百万
```

因为训练是离线 headless，且已有 `EvaluationPool`，参数上去是可以接受的。真正要避免的是“参数大但结构错”。

### 4.5 验收指标

记忆升级不能只看 fitness，要看：

- **Water return test**：见过水后被传送/远离，缺水时能返回。
- **Danger avoidance test**：某区域曾被捕食者攻击后，后续路径绕开。
- **Worksite memory test**：离开工地后仍能带材料回到工地。
- **Signal memory test**：听到信号后，短时间内朝信号源移动。
- **Ablation test**：关闭 spatial memory 后，上述指标显著下降。

---

## 5. 瓶颈二：缺少世界模型

### 5.1 当前问题

agent 当前没有显式预测模型。

它不知道：

- 向某方向走会不会更危险
- 发信号后同伴是否会靠近
- 砍树后是否有可搬运资源
- 带材料去工地是否会推进进度
- 夜晚是否应该回火堆

### 5.2 目标

建立 route-local world model：

```text
z_t = Encoder(observation_t, memory_t)
z_{t+1} = Dynamics(z_t, action_t)
reward_hat = RewardHead(z_t, action_t)
event_hat = EventHead(z_t, action_t)
```

第一阶段不要求 dream training，只要求：

- 预测下一步关键状态
- 输出 novelty / surprise
- 给 agent 提供 latent features
- 给训练分析提供“哪些事件可预测，哪些不可预测”

### 5.3 工程方案

#### 5.3.1 先做数据

必须先有 replay trace：

```text
observation_trace.bin/jsonl
action_trace.bin/jsonl
event_trace.jsonl
agent_trace.csv
world_summary.csv
```

否则 world model 没数据。

#### 5.3.2 预测目标

不要一开始预测全部世界像素。预测关键变量：

```text
next hydration delta
next energy delta
nearest food distance delta
nearest water distance delta
damage risk
kill probability
deposit probability
worksite progress delta
signal response probability
alive probability in N ticks
```

#### 5.3.3 训练方式

可选两条路：

1. **离线监督训练**：用 trace 训练小 MLP / GRU predictor。
2. **进化式 predictor**：把预测误差作为适应度，进化世界模型。

推荐先走离线监督，因为更容易验证。

### 5.4 验收指标

- next-step key variable prediction error 下降。
- world model latent 加入 brain 输入后，eval 泛化提升。
- curiosity 使用 prediction error 后，探索覆盖面积提升，但无效抖动不增加。
- 在陌生地图中，找水、回营地、避险成功率提升。

---

## 6. 瓶颈三：奖励偏刷分，不够因果链

### 6.1 当前问题

当前 reward 已能驱动行为，但存在风险：

- 一直乱砍也可能拿到收益。
- 一直发信号可能刷共现。
- 站在工地附近可能吃分红。
- 投掷大量尝试可能偶然命中。
- 社交接近可能变成挤成一团。

### 6.2 目标

把 reward 从“动作计数”升级为“因果链条”。

例如：

```text
ChopTree -> StickCreated -> PickedByAgent -> DepositedToBase -> FireBuilt -> NightSurvivalImproved
```

```text
SawPrey -> SignalEmitted -> PeersApproached -> JointAttack -> Kill -> MeatShared
```

```text
FoundWater -> RememberedWater -> LaterDehydrated -> ReturnedToWater -> Drank -> Survived
```

### 6.3 工程方案：Event Lineage Graph

新增事件因果账本：

```text
EventLineageGraph
├── event_id
├── event_type
├── tick
├── position
├── primary_agent
├── contributing_agents
├── consumed_objects
├── produced_objects
├── parent_events
├── outcome_events
└── delayed_reward
```

工程落点：

- 新增 `runtime/EventLineageTracker.*`
- 订阅 `EventBus`
- 给 `WorksiteLayer` / `NurseryLogisticsLayer` / `TreeLayer` / `Combat` / `SignalLayer` 增加事件记录
- 训练时 reward 不再直接粗分，而从 lineage tracker 结算

### 6.4 差分奖励

借鉴 COMA / difference rewards，不需要完整 actor-critic，也能做：

```text
agent_credit = global_outcome - outcome_without_agent_contribution_estimate
```

近似实现：

- 工地完成：按真实搬入材料数量、停留装配 tick、砍树贡献分配。
- 围猎成功：按伤害、吸引仇恨、靠近包围、信号召集分配。
- 通信成功：发送者、响应者、最终收益三段分配。

### 6.5 验收指标

- 工地完成 reward 中，路过者贡献接近 0。
- 发信号但无人响应不高奖。
- 发信号后同伴响应并产生实际收益才高奖。
- 投掷奖励按命中和后续收益，而非尝试数。
- 关闭 lineage reward 后，长期协作指标下降。

---

## 7. 瓶颈四：通信还没有语义

### 7.1 当前问题

已有信号：

- `SignalLayer`
- `EmitSignalCapability`
- `SignalPerception`
- signal co-attendance reward

但现在最多说明：

```text
信号与同伴/进食/共现有关
```

还不能说：

```text
信号 A 表示水
信号 B 表示危险
信号 C 表示过来搬东西
```

### 7.2 目标

让信号在任务压力下形成可验证语义。

### 7.3 任务设计

#### 7.3.1 Partial Observability Communication Task

设计：

- scout 能看到远处水/食物/危险。
- worker 看不到，但能收到信号。
- 只有 worker 到达目标才算团队成功。

验收：

- scout 发信号后 worker 移动方向与目标相关。
- 关闭信号后成功率下降。

#### 7.3.2 Danger Warning Task

设计：

- 某 agent 先看到捕食者。
- 其他 agent 看不到。
- 信号可提前触发群体回避。

验收：

- 信号强度/通道与捕食者接近概率存在互信息。
- 信号 ablation 后死亡率升高。

#### 7.3.3 Build Request Task

设计：

- 工地缺材料。
- 有材料的 agent 发信号或响应信号。
- 需要多个 agent 分工搬运。

验收：

- 信号后材料搬运方向向工地偏移。
- 不同通道与不同材料或目标阶段相关。

### 7.4 信号评估指标

新增 `communication_metrics.csv`：

```text
generation
channel
emit_count
response_count
response_latency_avg
mutual_information_signal_food
mutual_information_signal_water
mutual_information_signal_danger
mutual_information_signal_worksite
ablation_success_drop
listener_policy_shift
```

关键指标：

- **互信息**：信号和环境状态是否相关。
- **响应延迟**：同伴是否快速改变行为。
- **方向一致性**：听者是否朝信号源/目标移动。
- **消融损失**：关掉信号后任务是否明显变差。

---

## 8. 瓶颈五：合作信用分配太粗

### 8.1 当前问题

当前合作 reward 有：

- social proximity
- worksite proximity/completion
- joint hunt proximity

这会推动聚集，但不一定推动分工。

### 8.2 目标

让系统能识别：

- 谁找到了材料
- 谁搬了材料
- 谁砍了树
- 谁守在危险边界
- 谁发出了有效信号
- 谁响应信号
- 谁参与了围猎
- 谁只是路过

### 8.3 工程方案

#### 8.3.1 ContributionLedger

新增贡献账本：

```text
ContributionLedger
├── material_contributions[agent_id]
├── chop_contributions[agent_id]
├── assembly_ticks[agent_id]
├── signal_summons[agent_id]
├── response_to_signal[agent_id]
├── damage_contributions[agent_id]
├── tanking_or_baiting[agent_id]
└── protection_radius_ticks[agent_id]
```

#### 8.3.2 Worksite credit

工地完成时：

```text
reward = base_completion_reward
分配：
- 50% 给实际搬入材料者
- 20% 给砍树/产出材料者
- 20% 给装配区域内持续工作者
- 10% 给有效召集信号者
```

#### 8.3.3 Hunt credit

猎杀成功时：

```text
reward = kill_reward
分配：
- 伤害贡献
- 最后击杀
- 包围贡献
- 信号召集贡献
- 吸引仇恨/承伤贡献
```

### 8.4 验收指标

- 分工行为出现：部分 agent 长期偏采集，部分偏狩猎，部分偏基地。
- cooperation ablation 后建造/围猎下降。
- contribution entropy 不为 0：不是所有 reward 都集中到单个个体，也不是均分。

---

## 9. 瓶颈六：训练环境不够开放式

### 9.1 当前问题

固定训练世界会训练出固定套路。

当前 `camp_building_evolve` 已经很好，但它只是一个训练场。如果长期只训它，agent 可能学到：

- 固定基地结构
- 固定资源距离
- 固定捕食压力
- 固定工地流程

这不是世界理解。

### 9.2 目标

让环境本身成为可演化对象。

### 9.3 Environment Genome

新增环境基因：

```text
EnvironmentGenome
├── world_size
├── terrain_seed
├── terrain_height_scale
├── resource_density
├── water_source_count
├── predator_density
├── tree_density
├── object_density
├── base_position
├── worksite_count
├── worksite_required_materials
├── day_night_cycle
├── season_strength
└── disease_pressure
```

### 9.4 POET-like 方案

维护环境-种群对：

```text
EnvironmentArchive
├── env_001 + protagonist_population_A
├── env_002 + protagonist_population_B
├── env_003 + protagonist_population_C
...
```

流程：

```text
1. 每个环境训练对应 agent population
2. 周期性变异环境
3. 新环境必须满足 minimal criterion：不能太简单，也不能太难
4. 不同环境之间尝试转移 best genomes
5. 保存能产生新能力的环境和 agent
```

### 9.5 MAP-Elites 行为档案

不是只保留 fitness 最高的 agent，而是保留不同行为格子的 elite。

行为维度可选：

```text
survival_score
exploration_radius
build_score
hunt_score
communication_score
cooperation_score
risk_tolerance
home_loyalty
tool_use_score
memory_dependency_score
```

输出：

```text
qd_archive/
  build_high_hunt_low_signal_high/genome.bin
  explore_high_build_low/genome.bin
  hunt_high_coop_high/genome.bin
```

### 9.6 验收指标

- 多样化策略出现，而不是单一套路。
- agent 能跨环境迁移。
- 新环境难度逐渐上升。
- 某些复杂行为只在开放式环境链条中出现，直接训练无法出现。

---

## 10. 瓶颈七：缺少 replay / trace，无法判断涌现真假

### 10.1 当前问题

现在有 CSV 汇总，但无法回答：

- 某个 agent 为什么去那里？
- 信号后谁响应了？
- 某个工地是谁完成的？
- 某次围猎有几个人参与？
- 某个行为是不是随机巧合？
- 未来 3D 面板如何复现一次训练中的行为？

### 10.2 目标

建立完整 replay trace 系统。

### 10.3 Trace 文件设计

#### 10.3.1 agent_trace.csv / binary

```text
run_id,generation,episode,tick,agent_id,genome_id,species,x,y,vx,vy,alive,energy,health,hydration,carried_object,current_goal,action_0,...,action_n
```

#### 10.3.2 event_trace.jsonl

```json
{"tick":120,"event":"SignalEmitted","agent":17,"pos":[80.2,44.1],"channel":1,"intensity":0.8}
{"tick":148,"event":"ObjectDeposited","agent":21,"object":"Stick","pos":[60.0,100.0]}
{"tick":220,"event":"WorksiteCompleted","site":2,"contributors":[17,21,33]}
```

#### 10.3.3 world_trace.csv

```text
tick,living_agents,protagonists_alive,active_fires,completed_worksites,resource_count,corpse_count,signal_mass_channel_0,signal_mass_channel_1
```

#### 10.3.4 replay_manifest.json

```json
{
  "config": "configs/protagonist_ecology_train_camp_building_evolve.toml",
  "genome": "best_genome_overall.bin",
  "generation": 119,
  "episode": 0,
  "ticks": 1400,
  "schema_version": 1
}
```

### 10.4 未来 3D 面板接口

3D 面板不需要知道训练细节，只读：

```text
agent_trace
object_trace
event_trace
terrain snapshot
resource snapshot
```

行为映射：

```text
SignalEmitted -> 身体发光 / 地面波纹
ObjectDeposited -> 放下材料动画
ChopSuccess -> 砍树动作
CraftSuccess -> 手上出现矛
ThrowHit -> 投掷物命中
AgentDied -> 倒下 + 尸体资源
WorksiteCompleted -> 建筑几何变化
```

### 10.5 验收指标

- 任意 eval run 可完整 replay。
- replay 中能定位一次工地完成的贡献链。
- replay 可用于 3D 面板，不依赖实时训练。
- replay 与 CSV 指标一致。

---

## 11. 瓶颈八：缺少完整持续训练 resume

### 11.1 当前状态

已有：

```text
best_genome_overall.bin
best_genome_latest.bin
best_genome_genN.bin
checkpoint_genN/genome_*.bin
runtime.protagonist_genome_path
```

但还缺：

```text
runtime.resume_checkpoint_path
```

以及完整的：

- protagonist population resume
- background species population resume
- RNG state resume
- generation index resume
- innovation tracker resume
- species state resume
- archive state resume
- world model state resume

### 11.2 目标

让训练真正可持续：

```text
今天训到 119 代
明天从 119 代继续到 500 代
中途断电也不怕
可以回滚任意 checkpoint
```

### 11.3 工程方案

新增 checkpoint manifest：

```json
{
  "schema_version": 1,
  "generation": 119,
  "config_path": "configs/protagonist_ecology_train_camp_building_evolve.toml",
  "protagonist_population": ["genome_2857.bin", "genome_2858.bin"],
  "background_species": {
    "large_herbivore": ["genome_100123.bin"],
    "pack_hunter": ["genome_400321.bin"]
  },
  "rng_state": "rng_state.bin",
  "innovation_state": "innovation_tracker.bin",
  "metrics_last": "fitness.csv"
}
```

配置：

```toml
[runtime]
resume_checkpoint_path = "data/runs/.../checkpoint_gen119"
resume_mode = "population"
```

### 11.4 验收指标

- 从 checkpoint resume 后 genome ids 不碰撞。
- resume 后第 120 代训练可继续输出。
- 同 seed + 同 checkpoint 多次 resume 结果一致。
- 中断恢复不丢失 best genome / archive / metrics。

---

## 12. 瓶颈九：探索与新行为不足

### 12.1 当前问题

当前 intrinsic reward 是 new cell：

```text
1 / sqrt(visited_cells)
```

它能鼓励走新地方，但不能鼓励：

- 新工具链
- 新协作模式
- 新信号用法
- 新路线策略
- 新战斗结构

### 12.2 目标

建立多层 intrinsic motivation：

```text
spatial novelty
state novelty
event novelty
social novelty
causal novelty
skill novelty
```

### 12.3 工程方案

#### 12.3.1 Behavior Descriptor

每个 genome 每代生成行为描述：

```text
explore_radius
unique_cells
water_visits
base_returns
object_deposits
worksite_contrib
hunt_participation
signal_response
signal_influence
risk_exposure
memory_query_use
```

#### 12.3.2 Novelty Archive

维护最近 N 代行为 descriptor archive：

```text
novelty = avg distance to k nearest descriptors
```

fitness 改为多目标：

```text
score = task_fitness + alpha * novelty + beta * diversity_niche_bonus
```

或进入 NSGA-II / MAP-Elites。

#### 12.3.3 RND / ICM

如果有 world model：

- RND：未见状态预测误差高。
- ICM：动作导致的可控变化预测误差高。

注意：必须避免 stochastic noise trap，即随机但无意义的状态不应高奖。

### 12.4 验收指标

- 行为 descriptor 分布扩大。
- 不同 archive niche 有不同策略。
- 训练后期不会全部收敛为单一刷分行为。
- novelty ablation 后新行为数量下降。

---

## 13. 瓶颈十：长链任务需要层级目标

### 13.1 当前问题

主角每 tick 直接输出动作，容易短视。

建造这样的任务需要：

```text
找树 -> 砍树 -> 找掉落物 -> 搬回 -> 放入库存 -> 解锁石头 -> 搬石头 -> 工地装配
```

这不是单步反应能稳定完成的。

### 13.2 目标

建立 Manager / Worker 结构。

```text
Manager：每 64~128 ticks 选目标
Worker：每 tick 根据目标输出动作
Memory：保存目标承诺和完成状态
```

### 13.3 工程方案

#### 13.3.1 Brain 输出拆分

当前 action dim 是 13。后续改为：

```text
low_level_actions = 13
goal_logits = 14
commitment_gate = 1
attention_weights = N
```

#### 13.3.2 目标输入到 Worker

Worker 输入：

```text
observation + memory + current_goal_onehot + goal_age + goal_progress
```

#### 13.3.3 目标 reward

目标不是硬脚本，而是内在评价：

```text
FindWater -> hydration increased
GatherStick -> carrying stick or deposited stick
BuildWorksite -> worksite progress increased
Hunt -> damage/kill/meat
RespondSignal -> distance to signal source decreased or peer outcome improved
```

### 13.4 验收指标

- 目标持续时间不为 1 tick。
- 目标切换与环境状态相关。
- 建造链条连续性提升。
- 关闭 Manager 后长链任务成功率下降。

---

## 14. 完整实施路线

### Phase 1：Trace + Resume 基础设施

目标：所有后续研究都可观察、可恢复。

实现：

- `TraceRecorderLayer`
- `event_trace.jsonl`
- `agent_trace.csv/bin`
- `world_trace.csv`
- `checkpoint_manifest.json`
- `runtime.resume_checkpoint_path`

验收：

- 训练、eval、preview 都可 trace。
- 任意 checkpoint 可继续训练。
- replay 数据能复现 CSV 行为总量。

### Phase 2：Memory V2

目标：专门记忆区。

实现：

- `WorkingMemoryBank`
- `SpatialMemoryBank`
- `EpisodicMemoryBank`
- `SocialMemoryBank`
- `GoalMemory`
- `ProtagonistGenomeV2`
- 兼容旧 genome 的 loader 或明确 version gate

验收：

- 空间记忆任务通过。
- 记忆 ablation 明显降分。
- eval 泛化提升。

### Phase 3：因果信用分配

目标：从刷动作变成奖励因果链。

实现：

- `EventLineageTracker`
- `ContributionLedger`
- worksite/hunt/signal 贡献结算
- `credit_metrics.csv`

验收：

- 路过者不吃工地完成大分。
- 真实贡献者得分更高。
- 分工指标提升。

### Phase 4：通信语义任务

目标：信号从共现变成有语义压力。

实现：

- scout-worker partial observability 场景
- danger warning 场景
- build request 场景
- `communication_metrics.csv`
- signal ablation eval

验收：

- 信号与状态互信息上升。
- 关闭信号后成功率下降。
- listener policy shift 可测。

### Phase 5：Quality Diversity + Open-Ended Curriculum

目标：不只找一个最优解，而是生成多类能力。

实现：

- `BehaviorDescriptor`
- `NoveltyArchive`
- `MAPElitesArchive`
- `EnvironmentGenome`
- `POETCoordinator`

验收：

- archive 里出现多类策略。
- 环境难度逐步上升。
- 迁移产生 stepping stones。

### Phase 6：World Model

目标：引入预测式世界理解。

实现：

- trace dataset builder
- observation-action predictor
- latent feature extractor
- prediction-error curiosity
- world model eval reports

验收：

- 预测误差下降。
- latent 输入提升 eval。
- curiosity 提升探索但不导致无效抖动。

### Phase 7：Hierarchical Goal System

目标：长链任务稳定化。

实现：

- Manager head
- Worker head
- goal memory
- goal progress metrics

验收：

- 长链建造、回水、响应信号更稳定。
- 目标持续性合理。
- 关闭 goal system 后性能下降。

### Phase 8：Long Training + 3D Replay Panel

目标：最终展示。

实现：

- 多 seed 长训
- overnight / multi-day training
- best archive eval
- replay export
- 3D panel reads trace/state/event

验收：

- 用户能看见代际行为变化。
- 3D 模型动作由真实事件驱动。
- 有代表性的高光 replay：回巢、围火、搬运、建造、围猎、信号召集。

---

## 15. 推荐实验制度

### 15.1 每次大改必须三类实验

```text
smoke：3~5 generations，确认不崩
mid：50~150 generations，确认趋势
long：500~2000 generations，多 seed，看是否稳定
```

### 15.2 每个机制必须 ablation

例如 memory：

```text
full memory
no spatial memory
no episodic memory
no social memory
no goal memory
```

通信：

```text
full signal
signal disabled
signal noise increased
one channel only
listener blind to signal
```

合作：

```text
full lineage credit
uniform team reward
individual-only reward
no social reward
```

### 15.3 指标不能只看 fitness

必须同时看：

- survival
- hydration stability
- base return rate
- object delivery success
- worksite completion
- tool use
- hunt success
- signal response
- contribution fairness
- behavior diversity
- eval generalization
- ablation drop

---

## 16. 对“理解世界”的工程定义

本项目里，“理解世界”不能抽象说。必须定义成可测能力：

### Level 1：反应性生存

- 看到食物就吃
- 看到水就喝
- 看到敌人就跑

### Level 2：地点记忆

- 记得水源
- 记得基地
- 记得危险区
- 记得工地

### Level 3：因果链

- 知道砍树会产生材料
- 知道搬材料能推进工地
- 知道火/庇护能提升生存
- 知道信号能召集同伴

### Level 4：长期目标

- 缺水时优先找水
- 携带材料时回基地/工地
- 夜晚回安全区
- 狩猎失败后撤退或召集

### Level 5：社会协作

- 分工
- 共同搬运
- 共同围猎
- 信号召集
- 响应同伴

### Level 6：泛化

- 陌生地图仍能找水、回营地、建造、避险
- 资源位置变化后能重新适应
- 捕食者压力变化后能调整策略

### Level 7：开放式创新

- 出现未显式奖励但有用的策略
- 出现多样化角色
- 出现新的路线/防御/诱敌/共享资源方式

---

## 17. 当前优先级建议

既然目标是不怕复杂、不怕耗时、不做 MVP，推荐立即按下面顺序做：

1. **TraceRecorder + resume checkpoint**
2. **Memory V2 专门记忆区**
3. **EventLineageTracker + ContributionLedger**
4. **通信语义任务 + ablation**
5. **MAP-Elites / Novelty Archive**
6. **EnvironmentGenome / POET-like curriculum**
7. **World Model predictor**
8. **Hierarchical goal system**
9. **多 seed 长训 + 3D replay panel**

其中第 1 步最不酷，但最关键。没有 trace 和 resume，后面的复杂系统无法可靠调试。

---

## 18. 与未来 3D 可视化的关系

这份方案会直接服务 3D 面板。

3D 不是最后硬套模型，而是从现在开始让数据结构可视化友好：

```text
Agent state -> 位置、朝向、速度、健康、携带物
Action state -> 移动、砍树、投掷、吃、喝、逃跑、攻击
Event trace -> 发信号、命中、死亡、完成工地
Memory trace -> 记忆中的水源/危险/基地热区
Goal trace -> 当前目标颜色/图标
Social trace -> 谁跟谁合作、谁响应谁
```

这样最后 3D 不是假的动画，而是：

```text
真实训练/评估 trace 驱动的行为可视化
```

---

## 19. 风险与诚实预期

### 19.1 能比较确定做到的

- 更强记忆后，回水/回基地/工地定位会提升。
- trace 后，行为真假能被验证。
- 贡献账本后，合作奖励会更干净。
- signal ablation 后，可以判断信号是否真有用。
- resume 后，可以多天连续训练。

### 19.2 不保证但值得追求的

- 稳定分工。
- 语义化信号。
- 新策略自发出现。
- 多物种长期军备竞赛。
- 类似“原始聚落秩序”的观感。

### 19.3 最大风险

- 参数变大后训练速度下降。
- reward 更复杂后调参更难。
- trace 数据量很大。
- memory 结构过强可能导致依赖手工先验。
- open-ended curriculum 可能生成过难/过简单环境。

### 19.4 对策

- 每个机制都配 ablation。
- 每个阶段保留旧配置作为 baseline。
- 每次大改做 smoke/mid/long 三档实验。
- 所有新输出写 manifest，保证实验可复现。
- 先让 trace 能解释失败，再做下一层复杂化。

---

## 20. 最终目标图景

如果完整执行，本路线最终可以从现在的：

```text
能训练出一些搬运/砍树/狩猎/信号行为的生态训练场
```

推进到：

```text
一群拥有空间记忆、事件记忆、目标记忆和社会记忆的进化智能体，
在多物种连续 2.5D 世界中长期生存，
通过信号和协作完成采集、建造、狩猎、避险，
并在开放式环境课程中产生多样化策略。
```

用户最终看到的不是“我们写了很多 if-else NPC”，而是：

```text
训练前：乱跑、短视、死亡、偶然完成任务
训练中：开始回营、搬运、围绕火点活动、使用工具
训练后：有路线、有地点、有信号响应、有协作倾向、有生态压力下的适应行为
```

这就是本项目里“理解世界”的工程含义。
