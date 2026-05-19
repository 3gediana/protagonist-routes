# 架构设计（ARCHITECTURE）

> 这份文档解决一件事：**当未来你想加一个新功能时，应该改哪里、不应该改哪里。**

---

## 一、顶层结构（三层）

```
┌─────────────────────────────────────────────────────┐
│                      runtime                         │
│  ┌───────────────┐         ┌──────────────────────┐ │
│  │   headless    │         │     interactive      │ │
│  │ (训练 / batch)│         │  (演示 / GUI / 调试) │ │
│  └───────┬───────┘         └─────────┬────────────┘ │
└──────────┼───────────────────────────┼──────────────┘
           │                           │
           ▼                           ▼
┌─────────────────────────────────────────────────────┐
│                       viz                           │
│  renderer / ui / plot   (只在 interactive 中使用)    │
└─────────────────────────┬───────────────────────────┘
                          │ (订阅事件，纯只读)
                          ▼
┌─────────────────────────────────────────────────────┐
│                       core                           │
│  ┌──────────────────────────────────────────────┐   │
│  │  EventBus  ←─── 所有跨模块通信走这里 ────→   │   │
│  └──────────────────────────────────────────────┘   │
│   genome / brain / agent / world / evolution        │
│   perception / capability / reward / observer       │
└─────────────────────────────────────────────────────┘
```

**铁律**：
- `core` 永远不引用 `viz`，永远不引用 `runtime`
- `viz` 只读 `core` 的状态、订阅 `core` 的事件
- `runtime` 是组装层，决定"用什么 core 配置 + 是否带 viz"
- 跨模块通信优先走 EventBus，不优先走直接函数调用

---

## 二、核心数据模型

### 世界 = 多层叠加

世界不是一个大对象，而是**多个独立的层（Layer）叠在一起**：

```
World
├── TerrainLayer      静态地形：高度图 H(x,y) + 山洞标记 Cave(x,y)
├── ResourceLayer     食物点（位置 + 种类 + 再生规则）
├── SignalLayer       信息素地图（衰减场，N 个通道）—— F-COMM 模块加入
├── DiseaseLayer      病原体传播（pathogen-driven，非固定参数）—— F-DISEASE 模块加入
├── ObjectLayer       可移动物体（石头/工具）—— F-TOOL 模块加入
├── AgentLayer        所有活着的个体
└── ...               未来任何"世界的一部分"都是新加一层
```

每层都实现 `IWorldLayer`，独立 tick、独立查询、独立序列化。**新增功能 = 加一层，不改其它层**。

### 个体 = 基因 + 大脑 + 能力组合

```
Agent
├── Genome              所有可遗传信息（网络拓扑/权重 + 元参数 + immunity_gain_rate）
├── Brain               推理器（NEAT 网络的前向传播实例）
├── State               生命周期内的可变状态（位置/速度/能量/年龄/感染进度/免疫进度）
├── Pathogen            当前感染的病原体（null 如未感染）—— 有自己的 NN 和可进化行为
├── ImmuneMemory        恢复过的毒株签名列表 —— 抗原漂移机制
├── Perception[]        感知组件列表（视觉/听觉/地形/信号/感染）
└── Capabilities[]      能力组件列表（移动/吃/攻击/发信号/拾取）
```

**新增一种行为 = 加一个 Capability，不改 Agent 类**。
**新增一种感官 = 加一个 Perception，不改 Agent 类**。

### 适应度 = 奖励组件叠加

```
Fitness = sum( reward_i × weight_i )

reward_i 来自独立的 IRewardComponent 实现：
- SurvivalReward      活得久
- FoodReward          吃到东西
- NoveltyReward       探索新区域 (F-NOVELTY)
- CooperationReward   合作成功 (F-COOP)
- CombatReward        战斗胜利 (F-FIGHT)
- ...
```

**新增一种奖励 = 写一个新的 IRewardComponent，挂上去**。权重在 TOML 里配。

---

## 三、关键抽象接口（M0 必须实现）

> 完整 C++ 头文件签名见 `src/core/interfaces/`，下面是设计意图。

| 接口 | 职责 | 替换演习对象 |
|---|---|---|
| `IBrain` | 给定输入向量，输出动作向量 | M3 用 NEAT 网络替换固定拓扑 |
| `IGenome` | 可序列化、可变异、可交叉的基因 | M3 替换 |
| `IWorld` | 所有世界状态的容器 / 提供查询接口 | M5 增加地形层时压力测试 |
| `IWorldLayer` | 世界的"一层"（地形/食物/信号...） | 每个 F-* 加新层 |
| `IAgent` | 个体的最小契约 | 各 F-* 加 Capability |
| `IPerception` | 一种感官，把世界状态翻译成网络输入向量 | 各 F-* 加感知 |
| `IAgentCapability` | 一种能力（移动/攻击/发信号...） | 各 F-* 加能力 |
| `IRewardComponent` | 一种奖励项 | 各 F-* 加奖励 |
| `IEvolutionStrategy` | 选择/交叉/变异策略 | F-NOVELTY / F-COEVO 替换 |
| `IObserver` | 订阅事件，写入数据/文件/统计 | F-TREE / F-HEAT / F-REPLAY 等 |
| `IEventBus` | 事件发布订阅 | 全局唯一，通常一个实现就够 |

---

## 四、运行循环（每 tick 发生什么）

```
Runtime.tick():
  1. World.preTick()                    各层准备（信号衰减、食物再生、地形不变）
  2. for each Agent (并行):
       2.1 Perception.observe(world) → 输入向量
       2.2 Brain.forward(输入) → 动作向量
       2.3 Capabilities.dispatch(动作) → 发布动作事件到 EventBus
  3. World.applyEvents()                收集所有动作事件，统一应用（避免顺序依赖）
  4. RewardSystem.accumulate()          每个 RewardComponent 检查事件并加分
  5. World.postTick()                   死亡/出生/年龄+1
  6. EventBus.flush()                   通知所有 Observer
  7. (训练模式) EvolutionEngine.maybeStep()  如果到代界，执行选择/繁殖
```

**为什么 Step 2.3 用事件而不是直接修改 World**：避免 Agent A 的动作影响 Agent B 的感知，确保所有人在"同一时刻的世界"里决策。**否则会出现谁先 tick 谁占便宜的不公平**。

---

## 五、地形：逻辑 2.5D 是世界的一部分

**重要决策（见 ADR-0001）**：地形不是渲染装饰，是真实物理。

### 地形对个体的影响

| 影响 | 怎么实现 |
|---|---|
| 移动速度 | `effective_speed = base × terrain_factor(slope, surface_type)` |
| 能量消耗 | 上坡耗能多，下坡省能 |
| 视野 | 高地形遮挡视线（raycasting 时检查高度差） |
| 山洞 | `cave_mask(x,y) = true` 时：从外部不可见 / 庇护 / 可能氧气受限 |
| 资源分布 | 不同高度刷不同资源（高山稀有资源、低谷常见食物） |

### 个体能感知地形

`TerrainPerception` 组件输出：
- 自身当前高度
- 当前坡度向量（dH/dx, dH/dy）
- 是否在洞内（bool）
- 视野方向上的高度变化（raycasting 几条线的样本）

这些进入网络输入。**网络可以进化出"喜欢山洞的基因"或"偏好平原的基因"**。

### 但渲染只是 2.5D

- 不真的做 3D
- 用**等距投影（isometric）** 把 (x, y, H(x,y)) 投到屏幕
- 个体作为 2D 精灵贴在地形上
- 山洞用半透明覆盖层 / 内外切换显示
- 详见 [docs/baseline/M5_terrain.md](baseline/M5_terrain.md)

---

## 六、扩展点速查表

新增功能时查这张表：

| 你想加的东西 | 加在哪里 | 不需要改什么 |
|---|---|---|
| 一种新能力（攻击/搬运/求偶） | 写 `IAgentCapability` 实现 | Agent 类 / World / 其它 Capability |
| 一种新感官（嗅觉/温觉） | 写 `IPerception` 实现 | Brain / 其它 Perception |
| 一种新奖励 | 写 `IRewardComponent` 实现 | 现有奖励 / Agent |
| 一种新世界元素（信号场/陷阱） | 写 `IWorldLayer` 实现 | 其它 Layer |
| 一种新观测（统计/录制/可视化） | 写 `IObserver` 订阅事件 | 任何核心代码 |
| 一种新进化策略 | 写 `IEvolutionStrategy` 实现 | 其它 |
| 一种新大脑（CPPN/Transformer） | 写 `IBrain` + `IGenome` 实现 | 进化框架 / Agent |
| 一种新 UI 面板 | 在 viz/ui 下加 widget，订阅事件 | core 完全不动 |
| 一种新干预（GodButton 上加按钮） | 在 GodPanel 注册一个 IInterventionAction | core 不动 |

如果一个新需求**不在这张表里**——优先思考能不能转化成上述某种形式。
**如果真的需要改核心接口**，就是一次架构升级，写 ADR。

---

## 七、配置驱动

所有可调参数走 TOML：

```toml
[world]
size = [200.0, 200.0]
terrain_seed = 42
cave_density = 0.05

[evolution]
population = 100
elite_ratio = 0.1
mutation_rate = 0.05

[reward]
survival_weight = 1.0
food_weight = 10.0
novelty_weight = 5.0   # F-NOVELTY 启用时

[modules]
enabled = ["F-COMM", "F-COOP", "F-TREE", "F-GOD"]
```

**模块开关 = 配置项**，不需要重新编译。

---

## 八、为什么这样设计——快速版

详见 [DESIGN_DECISIONS.md](DESIGN_DECISIONS.md) 和 [adr/](adr/) 目录。

| 决策 | 一句话理由 |
|---|---|
| core / viz / runtime 三层 | headless 训练时 0 渲染开销；演示时插入 viz |
| 多层世界 (IWorldLayer) | 加新世界元素不改既有 |
| 能力 + 感知 + 奖励 三套组件化 | 加新行为不改 Agent |
| 事件总线 | 加新观测点不改核心 |
| 配置驱动 | 调参不重编 |
| 逻辑层感知地形 | 个体能基于地形进化策略，不只是渲染好看 |
| M3 替换 Brain / M5 加地形 | 故意安排的接口压力测试 |
| TOML 而不是 JSON | C++ 生态成熟（toml++），更适合人类编辑 |
| raylib 而不是 SFML/SDL | API 更简洁，C 风格直接，AI 助手写得快 |
