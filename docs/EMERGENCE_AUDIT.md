# 涌现行为审计 · 全面诚实复盘

> 2026-05-19
> 用户提出关键质疑：现有 19 个模块 MVP 通过，标准是否过低？
> 这份文档是 AI 助手对**代码 + 文档**做的全面审计，列出每个机制的真实状态。

---

## 0 · 摘要

| 项 | 数字 |
|---|---|
| 之前声称完成的模块 | 19 |
| 实际达到原始文档"完美标准"的模块 | **0** |
| 完全没做但我以为做了的模块 | 1（F-TOOL 原版） |
| 没出现在我清单里的模块 | 1（F-CAVE） |
| 真正未做的模块（文档存在） | 6 (F-TOOL 原版, F-CAVE, F-REPLAY, F-GOD, F-LLM, F-COMPARE) |

**核心问题**：我用的标准是"代码能跑 + fitness 有数变化 = ✅"，
而文档原定的标准是"**涌现行为可被语言描述 + 跨 seed 一致 + 互信息/相关性 ≥ 阈值**"。

---

## 1 · 完美标准的鸿沟

### 我的"实际"标准

```
✅ MVP = build 通过 + 单次运行 fitness 有变化
```

### 文档"原定"标准

```
✅ MVP   = 基础功能能跑
✅⛳⛳  = 跑 N 代不崩 + 视觉/统计可观察
✅✅⛳ = 跑长代不崩 + 关键统计指标可用
✅✅✅ = 跨 seed 涌现 + 互信息/相关性达标 + 可被语言描述
```

**我的 ✅⛳⛳ 实际上只达到了文档 MVP 标准。**

---

## 2 · 逐模块真实状态

### Group A · 涌现类模块（最严重）

#### F-COMM · 通信演化

**文档完美标准**：
- 信号-情境互信息 ≥ 0.3 bits（统计显著）
- 至少 1 个信号 → 行为绑定可被讲清楚

**我的实现**：
- `EmitSignalCapability.cpp`：NN 输出 → 信号场（45 行）
- `SignalLayer`：信号衰减 + 扩散
- `SignalPerception`：感知邻域信号

**关键缺失**：
1. ❌ **接收方奖励回溯**（backpropagation_factor）—— 文档说这是涌现 **90% 关键**
2. ❌ 信号-情境互信息分析脚本
3. ❌ 没测过任何 channel 是否有语义

**真实状态**：🟡 **基础设施 ✅，涌现机制 ❌**

**最坏情况推测**：agent 学会**忽略**信号通道（信号权重接近 0），因为没有奖励驱动让信号有用。

---

#### F-COOP · 合作

**文档完美标准**：
- 观察到稳定的协同行为：每代成功完成 ≥ 5 次重型任务
- 协同时间分布有规律（不是随机巧合）

**我的实现**：
- `HeavyResourceLayer`：重型资源数据
- `CoopReward`：合作奖励

**关键缺失**：
1. ❌ 没统计每代"重型任务完成次数"
2. ❌ 没分析"主动等待 vs 碰巧聚集"
3. ❌ 没分析"互惠行为"（同伴选择倾向性）

**真实状态**：🟡 **机制 ✅，协同验证 ❌**

---

#### F-NEUROMOD · 神经调节

**文档完美标准**：
- 至少 30% 个体网络中包含调节神经元
- 调节神经元激活值 vs 情境相关性 ≥ 0.3

**我的实现**：
- `NodeType::MODULATORY` 类型存在
- `add_modulatory_node` 变异

**关键缺失**：
1. ❌ 没统计"含调节神经元个体比例"
2. ❌ 没分析"调节激活 vs 情境特征"相关性
3. ❌ 可能调节节点权重接近 0，等同未使用

**真实状态**：🟡 **节点存在 ✅，使用率/相关性 ❌**

---

#### F-LEARN · 终生学习

**文档完美标准**：
- 拉马克 vs 达尔文两种模式对比图
- 适应度曲线差异可见

**我的实现**：
- `HebbianLearning` capability（36 行）
- 拉马克模式：`lamarck_mode` 配置

**关键缺失**：
1. ❌ 没做"达尔文 vs 拉马克"对照实验
2. ❌ 没量化"终生学习贡献" vs "先天进化贡献"

**真实状态**：🟡 **机制 ✅，对照实验 ❌**

---

#### F-NOVELTY · 新颖性搜索

**文档完美标准**：
- 适应度曲线出现明显的"阶梯"
- 至少一次"突破"被讲清楚

**我的实现**：
- `NoveltyArchive` + `NoveltyReward`
- 行为特征：position + visited_cells + food + damage

**关键缺失**：
1. ❌ 没做"开启 vs 关闭" novelty 的对照实验
2. ❌ 没标记"突破事件"
3. ❌ 没分析适应度曲线是否有阶梯

**真实状态**：🟡 **archive ✅，阶梯分析 ❌**

---

### Group B · 演化类模块

#### F-COEVO · 共进化

**文档完美标准**：观察到"红皇后"动力学（双方均不断变化但相对均势）

**我的实现**：双独立种群、独立 InnovationTracker、共享 World

**关键缺失**：
1. ❌ 没分析 pred/prey fitness 时间序列的**反相关性**
2. ❌ 没统计"追逐成功率"和"逃跑成功率"随代际变化
3. ❌ 没分析速度/感知能力的相互拉升

**真实状态**：🟡 **共进化 ✅，红皇后证据 ❌**

---

#### F-DISEASE · 病原体进化（你升级版）

**文档完美标准**：
- 观察到 agent 行为对密度敏感
- 看到 SIR 模型曲线

**我的实现**：
- `PathogenGenome`（31 权重 NN）
- `Pathogen` 实体
- 抗原漂移、strain-based immunity
- 跨物种传播

**关键缺失**：
1. ❌ 没分析"宿主免疫强度" vs "病原体毒性"相关性
2. ❌ 没分析"agent 行为对密度敏感"（避免密集区？）
3. ❌ 没绘制 SIR 曲线

**真实状态**：🟡 **病原体进化 ✅，行为/相关性证据 ❌**

---

#### F-NSGA · 多目标进化

**文档完美标准**：Pareto 前沿可见 + 目标 trade-off

**我的实现**：
- `Nsga2Evolution` (492 行)
- 现在能输出 `pareto_front.csv`

**关键缺失**：
1. ❌ 没分析目标间相关性（如果全正相关，Pareto 无意义）
2. ❌ 没识别"专精 vs 通才"物种

**真实状态**：🟡 **NSGA-II ✅，trade-off 分析 ❌**

---

### Group C · 我搞错了的模块

#### ⚠️ F-TOOL · 重大概念混淆

**文档实际定义**（`F-TOOL_tools.md`，🔴 高难度）：
- **可移动物体**（石头/木棒）
- agent 有 PickUpCapability / DropCapability
- 物体可推动、堆叠
- 目标：涌现"用石头堵洞防御"行为
- **明确标注"工具使用涌现是论文级难题"**

**我做的**（`F-TOOL_terrain_mod.md`）：
- 完全不同的东西：**改变地形高度**
- 我新写了一个 .md 文件名加了 `_terrain_mod`
- 实际上应该是 F-DIG 或 F-TERRAIN_MOD 之类的名字

**真实状态**：
- 原版 F-TOOL（可移动物体）：🔴 **完全没做**
- 我做的 terrain_mod：🟡 **是新机制，但不是文档里的 F-TOOL**

**建议**：
- 把 `F-TOOL_terrain_mod.md` 改名为 `F-DIG_terrain_modification.md`
- 文档 README 里恢复原版 F-TOOL 为 📋 待开发
- 新增 F-DIG 作为单独模块

---

#### ⚠️ F-CAVE · 我完全没注意过

**文档定义**（`F-CAVE_cave_system.md`，🟡 中难度）：
- 多层洞穴生成（不是 M5 的单层 mask）
- 洞穴内部连通图
- 洞口可被堵塞（配合 F-TOOL 原版）
- 目标：演化"堵洞防御"行为

**我的状态**：从来没读过这份文档，从来没列入清单

**真实状态**：🔴 **完全未实现**

---

### Group D · 工具类（相对扎实）

#### F-TREE · 谱系树
- `phylo_tree.jsonl` 已输出
- 文档完美标准没设量化指标
- 真实状态：🟢 **基本完成**（但没可视化）

#### F-HEAT · 热力图
- 现在有物种分色 + 捕食者/猎物分离
- 真实状态：🟢 **基本完成**（但分析维度还可以加）

#### F-STAT · 高级统计
- `AdvancedStatsObserver` 输出 Shannon 多样性
- 真实状态：🟢 **基本完成**

#### F-FORK · 平行宇宙
- 快照保存/恢复已验证
- 真实状态：🟢 **基本完成**（但没"分支多宇宙"UI）

---

### Group E · 完全未实现

| 模块 | 难度 | 描述 | 状态 |
|---|---|---|---|
| F-TOOL（原版） | 🔴 高 | 可移动物体 + 工具防御 | 📋 |
| F-CAVE | 🟡 中 | 多层洞穴系统 | 📋 |
| F-REPLAY | 🟡 中 | 录制 / 回放 | 📋 |
| F-GOD | 🟢 低 | 上帝按钮（需 M2） | 📋 |
| F-LLM | 🟡 中 | LLM 解说员（需 M5+） | 📋 |
| F-COMPARE | 🔴 高 | 对照实验左右分屏 | 📋 |

---

## 3 · 深挖路线建议

### 🎯 阶段 R1：建涌现审计基础设施

不加新模块，先建**审计观察器**：

```cpp
// src/core/observer/EmergenceAuditor.h
class EmergenceAuditor : public IObserver {
    // 输出 emergence_audit.csv，每代一行：

    // F-COMM 涌现审计
    double signal_behavior_mutual_info[K];  // 每 channel 与 N 种行为的 MI

    // F-NEUROMOD 涌现审计
    double modulatory_node_usage_ratio;     // 含调节节点的个体比例
    double modulatory_activation_corr;      // 调节激活 vs 能量水平的相关性

    // F-COEVO 红皇后审计
    double pred_prey_fitness_correlation;   // pred/prey fitness 时间序列反相关

    // F-COOP 协同审计
    int heavy_tasks_completed_this_gen;
    double avg_workers_per_task;

    // F-DISEASE 军备竞赛审计
    double host_immunity_vs_pathogen_virulence_corr;

    // F-TOOL/DIG 偏好审计
    double species_dig_preference[max_species];  // 每物种 dig/build 偏好
    double cave_occupancy_ratio;                 // agent 在洞内时间占比
};
```

### 🎯 阶段 R2：F-COMM 真正涌现机制

**关键改动**：实现"接收方奖励回溯"

```cpp
// 新增 SignalBackpropReward
class SignalBackpropReward {
    // 维护近 N tick 的信号 emit 历史 (agent_id, channel, position, time)
    // 当 agent X 接收到信号 S 后，下一个 reward_window tick 内：
    //   - 如果 X 的 reward > baseline，把 reward * backprop_factor 加给信号源
    //   - 信号源也获得"间接奖励"
};
```

这才是 F-COMM 涌现成功的真正机制。

### 🎯 阶段 R3：对照实验框架

用 F-FORK 快照做 ablation：

```toml
[experiments.ablation]
modules = ["F-COMM", "F-COOP", "F-NEUROMOD", "F-NOVELTY"]
seeds = [1, 2, 3, 4, 5]
generations = 300
```

每个模块跑 (开启 × 5 seeds) + (关闭 × 5 seeds) = 10 次 ×N 代
比较 fitness/multivariate metrics → 量化贡献

### 🎯 阶段 R4：状态符号更新

跑完审计后，按真实标准更新模块状态：

```
✅✅✅ 完美：跨 seed 涌现 + 互信息/相关性达标
✅✅⛳ 完整：机制 ✅ + 统计支持 ✅，但涌现不显著
✅⛳⛳ MVP：机制 ✅，统计未做
⚠️ 降级：禁用 vs 启用没差异
```

预测真实状态：
- 大部分模块会从 ✅⛳⛳ 降级到 ✅⛳⛳（保留）或 ⚠️（如果禁用无差异）
- F-COMM 大概率会变成 ⚠️（因为没奖励回溯）
- F-COEVO 可能升级到 ✅✅⛳（红皇后有概率自然涌现）

---

## 4 · 论文级发现的真正方向

如果阶段 R1-R3 都做完，可挖：

1. **F-COMM 涌现的奖励回溯阈值**：backprop_factor 在什么范围内才能涌现语义？
2. **F-TOOL/DIG 生态工程师效应**：地形复杂度增长率 vs 物种多样性
3. **F-COEVO 红皇后周期分析**：fitness 反相关性的频谱（FFT 看周期）
4. **F-NEUROMOD vs F-LEARN 谁更重要**：可塑性 vs 终生学习对适应性的贡献
5. **F-DISEASE 株系动力学**：宿主多样性 → 株系多样性的因果路径
6. **跨模块协同效应**：F-COMM + F-COOP 是否产生 1+1>2 效应？

---

## 5 · 给未来 AI 助手的话

### 旧规则（错的）

```
模块能跑 + fitness 变化 = ✅⛳⛳
```

### 新规则

```
新加模块前，先想清楚：
1. 这个模块的"涌现行为"是什么？能用一句话描述吗？
2. 怎么量化这个行为？需要新增什么 observer / metric？
3. 没有这个模块时，应该看到什么差异？
4. 跨 5 个 seed 都能复现吗？

完美标准 = 上面 4 个全部 ✅
否则只能标 ✅⛳⛳ (MVP) 或 ⚠️ (降级)
```

### 不要再做的事

- ❌ 不要把"build 通过"当成"机制起作用"
- ❌ 不要把"fitness 数字变化"当成"涌现"
- ❌ 不要跳过对照实验
- ❌ 不要把没读过的文档说成"已完成"
- ❌ 不要新造模块名（如 F-TOOL_terrain_mod.md）和已有模块重名

### 必须做的事

- ✅ 每加模块前，把对应文档**完整读完**
- ✅ 每加模块后，输出对应的**审计指标**
- ✅ 标 ✅✅✅ 之前，必须做**对照实验**
- ✅ 不达标就**老实标 ⚠️ 或降回 MVP**

---

## 6 · 当前最高优先级行动项

按 ROI 排序：

1. **建 EmergenceAuditor**（最重要，所有模块复用）
2. **F-COMM 加奖励回溯** + 互信息分析（论文最有潜力的方向）
3. **F-COEVO 加红皇后审计**（这个是几乎肯定有的涌现，量化它就行）
4. **F-LEARN 拉马克 vs 达尔文对照实验**（经典对照，结果稳定）
5. **F-TOOL 重命名 + 实现原版可移动物体**（修复历史错误）
6. **F-NEUROMOD 调节节点使用率统计**（先看用没用，再看相关性）

每完成一项，在 `docs/experience_log.md` 写一段实验记录。
