# F-NOVELTY · 新颖性搜索

> 不奖励"做得好"，奖励"做得和别人不一样"。早期死亡螺旋的救命药。

- **难度**：🟡 中
- **确定性档**：🟢 高
- **前置条件**：M3 完美完成（如果 M1 已经引入了基础内在动机，本模块是更完整版本）
- **状态**：📋 待开发

---

## 一句话

定义个体的"行为特征向量"（最终位置、移动距离、访问区域等），新颖性 = 当前个体的特征向量与最近邻居们的距离。把新颖性作为额外的 fitness 项。

---

## 你会看到什么效果

### 完美版
进化在适应度高峰附近停滞时，新颖性奖励驱使部分个体往陌生方向探索。结果：发现一个之前从未到达过的高峰（如山顶稀有资源、远端食物点）。
适应度曲线经常呈现"先平台、后跳跃"的阶梯形——每次跳跃对应一次"新发现"。

### 标准版
种群多样性显著高于纯 fitness 进化，但是否带来更高最终适应度不一定。

---

## 实现要点

### 行为特征向量（Behavior Characterization, BC）

```
src/core/evolution/novelty/
├── NoveltyArchive.cpp            存所有历史个体的 BC
├── BehaviorCharacterization.cpp  从 agent 一生轨迹提取特征向量
└── NoveltyReward.cpp             计算 novelty score
```

BC 设计建议（任选 + 可组合）：
- 最终位置 (x, y)
- 一生移动距离总和
- 访问的网格区域 bitmap（hash 后取部分维度）
- 平均能量
- 信号发出统计

### 新颖性计算

```cpp
float novelty(const BC& bc, const NoveltyArchive& archive) {
    auto neighbors = archive.kNearest(bc, k=15);
    return mean_distance(bc, neighbors);
}
```

### 与 fitness 混合

```toml
[F-NOVELTY]
novelty_weight = 0.3
# fitness_total = original_fitness * (1 - 0.3) + novelty_score * 0.3
```

或者用多目标进化（pareto）—— 这是 F-NSGA 的事。

### Archive 维护

- 每代结束时，把"足够新颖"的个体加入 archive
- archive 有上限（如 1000），满了删最旧
- archive 用 KD-tree 加速 k-NN 查询

---

## 三档完美标准

### MVP ✅⛳⛳
- [ ] BC 提取和 novelty 计算工作
- [ ] novelty 作为 fitness 项混合进化能跑

### 完整 ✅✅⛳
- [ ] 跑 200 代不崩
- [ ] 种群多样性指标显著高于无 novelty 的基线

### 完美 ✅✅✅
- [ ] 适应度曲线出现明显的"阶梯"
- [ ] 至少一次"突破"被讲清楚（哪个新颖的个体打破了平台）
- [ ] git tag `module-F-NOVELTY-done`

---

## 🧠 你的补充点

| 项 | 决定什么 |
|---|---|
| **BC 维度选择** | 决定"什么算新颖"。位置类BC → 探索新区域；行为类BC → 探索新策略 |
| **k 邻居数** | 太小不稳定，太大新颖性饱和 |
| **novelty_weight** | 与 fitness 的混合比例 |

---

## ⚠️ 易踩坑

1. **BC 维度过高**：维度越高距离越接近（curse of dimensionality）。**BC 控制在 5-20 维**
2. **archive 爆炸**：不限制大小内存炸。**用 LRU 或定期清理**
3. **纯新颖性反而差**：完全用 novelty 替代 fitness 会跑偏。**混合比例 0.2-0.5 是甜点**

---

## 答辩话术

> "传统进化只奖励'有用'。但有用的策略往往很快收敛，进化卡死在局部最优。新颖性搜索奖励'独特'——哪怕你做得不好，只要你做的和别人不一样，你也能活下去。结果是种群保持多样性，更容易发现真正高的山峰。这是对达尔文'适者生存'的一个反思：**不是只有'最适应者'才有进化价值，'最不一样者'也是**。"
