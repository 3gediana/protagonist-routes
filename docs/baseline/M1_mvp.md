# M1 · 连续 2D MVP

> 第一次让进化跑起来。最小化验证整个进化循环可工作。

---

## 目标

在 M0 脚手架上填业务逻辑，跑通完整的"评估—选择—繁殖"循环。
**不上 NEAT，不上可视化**。先证明进化机制本身能跑。

跑起来后，应该能在 100 代内观察到适应度曲线明显上升（CSV 形式）。

> 注：本页出现的种群规模、tick 数、reward 权重、倍率目标等具体数值，默认都只作为**起始示例**，不是预先验证过的真值；后续以真实实验结果为准。

---

## 前置条件

- M0 完美通过（`baseline-M0-done` tag）
- 你已经决定了基础奖励函数（见 🧠 补充点）

---

## 关键产出

### 网络：固定拓扑前馈 NN

**为什么不上 NEAT**：先验证进化循环。NEAT 的拓扑变异有自己的 bug 和复杂度，混在一起出问题排查难。
**结构**：输入层 → 一个隐藏层（10 神经元）→ 输出层。激活函数 tanh。
**接口**：实现 `IBrain` 和 `IGenome`。`Genome` 只存权重数组（fixed topology, fixed size）。

```
src/core/brain/fixed/
├── FixedTopologyGenome.h
├── FixedTopologyGenome.cpp
├── FixedTopologyBrain.h
└── FixedTopologyBrain.cpp
```

### 世界：连续 2D，无地形

**坐标**：浮点 `Position { float x, y; }`，世界尺寸 200×200（可配）。
**层次**：
- `AgentLayer` —— 所有活个体
- `ResourceLayer` —— 食物点（位置 + 能量值；被吃后随机重生）

**没有地形**——M5 才加。M1 的世界是一张光秃秃的 2D 平面。
**没有信号**——F-COMM 才加。

```
src/core/world/
├── World2D.h
├── World2D.cpp
└── layers/
    ├── AgentLayer.h
    ├── AgentLayer.cpp
    ├── ResourceLayer.h
    └── ResourceLayer.cpp
```

### 个体：最小可活配置

```
Agent
├── Genome (FixedTopologyGenome)
├── Brain (FixedTopologyBrain)
├── State { Position, Velocity, Energy, Age }
├── Perceptions:
│     ├── SelfPerception           能量、年龄归一化
│     └── NearestFoodPerception    最近食物方向（dx, dy）+ 距离
└── Capabilities:
      ├── MoveCapability           读取 brain 输出 (vx, vy)，更新位置和速度
      └── EatCapability            位置足够近时吃掉食物，能量增加
```

**输入维度**：~5 维（能量 + 年龄 + 食物 dx/dy + 食物距离）
**输出维度**：2 维（vx, vy）

### 进化：经典 GA

代际制（generational），不是连续进化。每一代：
1. 创建 100 个 Agent，初始化随机基因
2. 让所有 Agent 同时在 World 中跑 60 秒（3600 tick）
3. 计算每个 Agent 的 fitness
4. 选择前 20% 精英 + 锦标赛选择剩下 80%
5. 交叉 + 高斯权重变异，产生下一代 100 个 Genome
6. 重置世界，进入下一代

```
src/core/evolution/
├── ClassicGA.h
├── ClassicGA.cpp
└── selection/
    ├── EliteSelection.h
    └── TournamentSelection.h
```

### 奖励：两个 RewardComponent

```
src/core/reward/
├── SurvivalReward.cpp        每 tick 存活 +0.01
├── FoodReward.cpp            每吃一个食物 +10
└── IntrinsicReward.cpp       内在动机：好奇心驱动（见下）
```

**SurvivalReward + FoodReward 是基础**。
**IntrinsicReward**：用一个简化版本——奖励"访问到此前没访问过的网格区域"（把世界离散为 20×20 格子，每访问新格子 +1）。这是 ADR-0002 决定从 M1 就引入的关键，避免早期死亡螺旋。

### 观察者：CSV 输出

```
src/core/observer/
├── FitnessRecorder.cpp       每代结束时把 平均/最大/最小 fitness 写入 data/runs/<ts>/fitness.csv
└── PopulationRecorder.cpp    每代记录种群多样性指标
```

订阅 `GenerationEnded` 事件。

### 主循环

```
src/runtime/headless/HeadlessRuntime.cpp:
  loadConfig()
  initEventBus()
  initWorld()
  initPopulation(100)
  for gen in 0..config.generations {
    evaluatePopulation()       // 让世界跑 60 秒
    recordFitness()            // 通过事件发布，CSV observer 接住
    nextGeneration()           // GA 选择/交叉/变异
  }
  dumpFinalSnapshot()
```

---

## 三档完美标准

### MVP ✅⛳⛳
- [ ] 代码编译通过
- [ ] `neural-eco --mode headless` 能跑 10 代不崩溃
- [ ] CSV 文件正常生成

### 完整 ✅✅⛳
- [ ] 跑 100 代不崩溃
- [ ] 每代评估时间稳定（不随代数膨胀）
- [ ] 单元测试覆盖：FixedTopologyBrain forward / GA 交叉变异 / Reward 累计

### 完美 ✅✅✅
- [ ] **CSV 适应度曲线显示明显上升趋势**：100 代后平均 fitness ≥ 初始 × 3
- [ ] 至少跑 5 次独立实验，曲线趋势一致（不是运气）
- [ ] 用 Python 脚本（`scripts/plot_fitness.py`）画出适应度曲线，截图归档到 `data/runs/<best>/result.png`
- [ ] 实验日志记入 `docs/experience_log.md`：调参经历、失败原因、最终参数
- [ ] git tag `baseline-M1-done`

---

## 🧠 你的补充点（重要）

### 1. 奖励函数权重

提供建议初值，但你可以调：
```toml
[reward]
survival_weight = 1.0    # 每 tick 存活
food_weight = 10.0       # 每吃一个食物
intrinsic_weight = 5.0   # 探索新区域
```

**调整原则**：
- food_weight 太高 → 全员"贪吃蛇"，无多样性
- intrinsic_weight 太高 → 全员只探索不吃东西
- survival_weight 太高 → 全员消极保守

**这是核心设计活**，需要你试错。

### 2. 初始种群多样性

我会用"全随机"作为默认初始化。你可以选：
- 全随机（默认）
- 用一组手工设计的"种子"基因混合（更稳但少惊喜）

### 3. 食物再生策略

默认：吃掉一个，5 秒后在随机位置重生（保持总量恒定）。
你可以试：
- 集群再生（在吃掉位置附近重生 → 涌现"食物点位"）
- 随机再生（默认）
- 周期性扫荡（每 30 秒大量重生一波）

---

## 🔌 接入点

- M2 会订阅 `WorldTick` / `AgentMoved` 等事件做实时渲染，**不改 M1 代码**
- M3 会用 `NeatGenome` 替换 `FixedTopologyGenome`、`NeatBrain` 替换 `FixedTopologyBrain`，**进化循环代码不动**
- M5 加地形会再加一个 `TerrainLayer` 和 `TerrainPerception`，**Agent 类不动**

> 如果 M3 / M5 时发现要改 M1 代码，说明 M0 的接口设计有问题，要回头修 M0。

---

## ⚠️ 易踩坑

1. **奖励太密集**：DeepSeek 在 #78 强调"只奖励结果"——你可能受影响想砍 SurvivalReward。**别砍**。M1 阶段密集奖励是为了让进化能起步；M3+ 才能逐步稀疏化
2. **种群规模太大**：100 是甜点。50 可能多样性不够，200 训练慢
3. **每代评估时间过长**：60 秒（3600 tick）是合理上限。再长 100 代要跑几小时
4. **fitness 全是 0**：检查食物分布密度。世界 200×200 应该有约 30-50 个食物点
5. **adaptive 失败的 silent 模式**：CSV 看不出毛病但行为像随机——加个 episode 末尾的"位置标准差"统计，如果种群从来没散开过，说明问题

---

## 下一阶段

→ [M2 · 可视化接入](M2_visualization.md)
