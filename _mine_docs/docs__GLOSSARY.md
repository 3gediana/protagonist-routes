# 术语表（GLOSSARY）

> 这个项目里出现的术语和它们的实际含义。学完这页就能看懂所有文档。

---

## A · 算法与机制

### NEAT（NeuroEvolution of Augmenting Topologies）
一种**同时进化网络结构和权重**的算法（Stanley & Miikkulainen 2002）。从最简单的"输入直连输出"开始，每一代可能：
- 随机加一个隐藏神经元
- 随机加一条新连接
- 随机改一个连接的权重

**特色**：每个连接有"创新编号"（innovation number），用来在不同结构的网络间做有意义的交叉。
**用在**：M3 阶段的 Brain 实现。

### CTRNN（Continuous-Time Recurrent Neural Network）
连续时间递归神经网络。每个神经元有自己的"时间常数 τ"，状态按 ODE 缓慢更新：

```
τ · dy/dt = -y + activation(weighted_sum_of_inputs)
```

**通俗讲**：神经元有"惯性"，反应不是立刻到位，会慢慢趋近目标值。
**视觉效果**：个体行为有节律、有惯性，看起来不像金鱼。

### 神经调节（Neuromodulation）
某些"调节神经元"的输出不参与决策，而是**改变其它神经元的工作方式**（学习率、激活强度等）。
**视觉效果**：同一个体在不同情境下行为差异大，看起来像有"情绪"。

### 内在动机 / 好奇心驱动（Intrinsic Motivation）
不只看外部奖励，让网络对**预测误差**敏感。预测错了 = 看到陌生东西 = 内部奖励。
**用途**：破解早期"全员饿死"的死亡螺旋。

### 拉马克 vs 达尔文
- **达尔文**：后代只继承父母**出生时**的网络
- **拉马克**：后代继承父母**死亡时**的网络（学到的也传下去）

生物学上拉马克是错的，但**算法上拉马克更快收敛**。是有趣的对照实验。

### 新颖性搜索（Novelty Search）
不奖励"做得好"，奖励"做得和别人不一样"。
**用途**：避免种群过早收敛到局部最优。

### 协同进化（Coevolution）
多个种群相互作为对方的环境一起进化。
**典型**：捕食者 vs 猎物军备竞赛。

---

## B · 网络结构

### Genome（基因）
个体可遗传的所有信息：网络拓扑（神经元和连接列表） + 权重 + 元参数（学习率/调节标签等）。
**关键性质**：可序列化（存档/读档） + 可变异 + 可交叉。

### Brain（大脑）
基于 Genome 实例化的**前向推理器**。Brain 持有 Genome 引用 + 当前激活状态（CTRNN 的 hidden state）。
Genome 是数据，Brain 是计算。

### 拓扑变异（Topological Mutation）
- **add_node**：在某条连接中间插入一个隐藏神经元
- **add_connection**：在两个未连接的神经元间加边
- **disable_connection**：禁用一条边但保留（可能未来重激活）

### 权重变异（Weight Mutation）
对现有权重加高斯噪声，或重置为随机值。

### 物种（Species）
NEAT 用 compatibility 距离把网络聚类成"物种"，不同物种**不互相竞争**（只在物种内做选择），保护新出现的拓扑不被立刻淘汰。

### Compatibility 距离
两个 Genome 之间的"差异度"，由共有连接的权重差 + 不共有的连接数加权计算。

---

## C · 世界与个体

### Layer（层）
世界由多个独立的 Layer 叠加。每层管自己的数据：
- TerrainLayer 高度图
- ResourceLayer 食物点
- SignalLayer 信息素
- AgentLayer 个体
- ObjectLayer 可移动物体

加新世界元素 = 加一层。

### 山洞（Cave）
特殊地形结构。在 cave_mask 中标记，效果：
- 从外部不可见
- 视野受限
- 提供庇护（不被远程发现）

### Capability（能力）
个体的一种行为接口（移动/攻击/发信号/吃东西/拾取）。每个 Capability 监听 Brain 输出的对应通道，发出对应事件。

### Perception（感知）
把世界状态翻译成网络输入向量。一种感官 = 一个 Perception 实现。

### 信号场（Signal Field）
一张多通道的衰减地图。个体在自身位置写入信号向量，周围的个体能读取附近信号叠加值。**通信涌现的载体**。

---

## D · 评估与统计

### Fitness（适应度）
个体的"得分"。等于多个 RewardComponent 的加权和。
**关键认知**：你设计 Reward，进化决定怎么最大化 Fitness。**只奖励结果，不奖励过程**（详见 ADR-0002）。

### RewardComponent（奖励组件）
一种独立的得分项（生存/吃饭/合作成功/新颖）。可叠加，权重可配。

### 物种树（Phylogenetic Tree）
所有曾经存在过的个体的家谱图。看到分支灭绝、优势物种祖先、瓶颈期等。
**项目最震撼的可视化之一**。

### 行为热力图（Heatmap）
所有个体经过的位置频率叠加成颜色。展示"群体记忆 / 文明小径"。

### Compatibility 阈值
判断两个网络是否同物种的距离阈值。调高了物种少（容易过早收敛），调低了物种多（探索强但效率低）。**M3 调参的核心参数**。

---

## E · 工程概念

### Tick
世界更新的最小时间步。每个 tick 所有个体决策一次（或按异步周期决策一次）。

### Generation（代）
一次完整的"评估—选择—繁殖"循环。
注意：在持续进化模式下没有明显代界，只有出生/死亡事件。

### Episode（回合）
单次世界从初始化到所有个体死亡或时间用完的一段。

### Headless（无头模式）
不渲染、不接 GUI，纯逻辑跑。用于训练。

### Interactive（交互模式）
带 viz 渲染、带 GUI 控件的运行模式。用于演示和调试。

### Snapshot（快照）
某一代世界状态的完整序列化（所有个体的 Genome + 世界 Layer 状态）。F-FORK 模块用来做平行宇宙。

### EventBus（事件总线）
中心化的发布-订阅系统。所有跨模块通信优先走这里。

### ADR（Architecture Decision Record）
架构决策记录。每个不可逆决策一份单独 markdown，写明背景、选择、理由、影响。

### MVP / 完整 / 完美
三档完成度。详见 ADR-0004。

---

## F · 模块编号约定

```
F-XXX
└── 模块代号

例如:
F-COMM    Communication       通信信号场
F-COOP    Cooperation         合作机制
F-FIGHT   Combat              对抗机制
F-PRED    Predator-Prey       捕食者-猎物
F-TOOL    Tools               工具与物体
F-LEARN   Lifetime Learning   终身学习（拉马克开关）
F-DAY     Day-Night           昼夜循环
F-SEASON  Seasons             季节
F-DISEASE Disease             疾病传播
F-CAVE    Cave System         洞穴系统
F-TREE    Phylogenetic Tree   物种树
F-HEAT    Heatmap             行为热力图
F-FORK    Parallel Universe   平行宇宙存档
F-STAT    Advanced Stats      高级统计
F-GOD     God Panel           上帝按钮
F-LLM     LLM Narrator        LLM 解说员
F-REPLAY  Replay              录制回放
F-COMPARE Side-by-Side        对照实验
F-CURRICULUM Curriculum       课程学习
F-NOVELTY Novelty Search      新颖性搜索
F-COEVO   Coevolution         协同进化
F-NSGA    NSGA-II             多目标进化
F-NEUROMOD Neuromodulation    神经调节
```

---

如果碰到本表没有的术语，请告诉我，我加进来。
