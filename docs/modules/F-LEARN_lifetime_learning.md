# F-LEARN · 终身学习 + 拉马克开关

> 让个体一生中能学习，并提供"达尔文 vs 拉马克"对照实验。

- **难度**：🟡 中
- **确定性档**：🟡 中（拉马克更快可能但差异未必显著）
- **前置条件**：M3 完美完成
- **状态**：📋 待开发

---

## 一句话

让 NeatBrain 在生命周期内根据外部 reward 信号微调权重。**拉马克模式**下，后代继承死亡时的权重；**达尔文模式**下，后代只继承出生时的权重。运行时一键切换两种模式。

---

## 你会看到什么效果

### 完美版
两个相同初始化的种群，左边达尔文，右边拉马克。
- 拉马克种群快速学到趋食行为（学到的传给下代 → 收敛快）
- 达尔文种群慢一些，但**最终适应度可能更高**（避免局部最优陷阱）

UI 一键切换两种模式，截图对比适应度曲线。

### 标准版
两种模式都能跑，但适应度曲线差异不大。

### 降级版
即使数值差异微小，**哲学讲解仍有价值**。

---

## 实现要点

### 学习规则选择

两种实现：

**A. 简单 RL 微调**
- 维护 baseline reward
- 每步 reward 减 baseline 的偏差作为 advantage
- 用 policy gradient 微调权重（小学习率）

**B. 赫布学习（更生物启发）**
- 每个连接按 "前后神经元同时激活程度 × 学习率" 更新权重
- 不需要外部 reward 信号
- 自组织，但可能跑偏

**推荐 A**（更可控，效果更稳）。可作为 F-LEARN-HEBB 子模块再加 B。

### 模式开关

```cpp
class LamarckSwitch {
public:
    enum Mode { DARWIN, LAMARCK };

    Genome reproduce(const Agent& parent_a, const Agent& parent_b) {
        if (mode == LAMARCK) {
            // 用死亡时的权重交叉
            return crossover(parent_a.brain.currentGenome(),
                             parent_b.brain.currentGenome());
        } else {
            // 用出生时的权重交叉
            return crossover(parent_a.birthGenome(),
                             parent_b.birthGenome());
        }
    }
};
```

### 启用条件

终身学习计算量增加 ~10-30%，可在 config 选择性启用：
```toml
[F-LEARN]
enabled = true
mode = "darwin"           # darwin / lamarck
learning_rate = 0.001
update_frequency = 10     # 每多少 tick 更新一次
```

---

## 三档完美标准

### MVP ✅⛳⛳
- [ ] 终身学习能跑，权重在生命周期内确实变化
- [ ] 模式切换可用

### 完整 ✅✅⛳
- [ ] 两种模式各跑 100 代不崩
- [ ] 适应度曲线收敛

### 完美 ✅✅✅
- [ ] **两种模式适应度曲线对比图截图归档**
- [ ] git tag `module-F-LEARN-done`

---

## 🧠 你的补充点

| 项 | 决定什么 |
|---|---|
| **学习率** | 太大破坏进化，太小没影响 |
| **update_frequency** | 频繁更新更精细，但拖慢训练 |
| **是否启用赫布** | 默认否，启用为论文级别加分 |

---

## 答辩话术

> "拉马克主义在生物学是错的，但在算法里很有用。这两条曲线展示了：拉马克种群快速收敛，达尔文种群慢但最终更高——这正是早期人工生命研究的经典发现。我重现了这个对比。"
