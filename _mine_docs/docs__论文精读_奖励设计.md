# 论文精读 · 奖励设计调研（为 D-043 服务）

> 4 篇文献精读，提取关键公式与设计原则。给未来 AI 看，不是给用户看。

## 速记结论（D-043 该怎么改）

| 改动 | 来源 | 优先级 |
|---|---|---|
| first_X +20 → +1 | Crafter | 🔴 必改 |
| 删 first_hunt | Crafter（无 attack_wolf） | 🔴 必改 |
| 死亡 -100 → -10 | Crafter | 🔴 必改 |
| 加 reward clip [-5, +5] | PPO bag of tricks | 🟡 建议 |
| 加 PBRS Φ 状态势 | Ng 1999 | 🟡 建议 |
| satiation_scale → drive(s) | Keramati 2014 | 🟢 后续 |
| 评估指标看几何均值 | Crafter | 🟢 后续 |
| (远期) auxiliary next-achievement | Moon 2023 | ⚪ 远期 |

---

## 1. Crafter — Hafner ICLR 2022

[arxiv:2109.06780](https://arxiv.org/abs/2109.06780) · [GitHub: danijar/crafter](https://github.com/danijar/crafter)

### 核心定位
deep_protagonist 的学术对应物。2D Minecraft 简化版，开放生存，PPO + 17 离散动作，22 achievements。

### 17 动作（vs 你的 17）

```
Noop, Move{L,R,U,D}, Do, Sleep,
Place{Stone, Table, Furnace, Plant},
Make{Wood,Stone,Iron}×{Pickaxe,Sword}
```

关键差异：**Crafter 用上下文动作"Do"**（对树砍/对水喝/对僵尸打），没有 attack_wolf / hunt_animal 这类。你的 disc_action 是显式分类，**容易设计成具体语义错误**。

### 22 Achievements（你 first_X 的参考模板）

```
collect_wood   collect_stone   collect_coal    collect_iron   collect_diamond
collect_sapling  collect_drink
eat_cow        eat_plant
place_table    place_furnace   place_stone     place_plant
make_wood_pickaxe   make_stone_pickaxe   make_iron_pickaxe
make_wood_sword     make_stone_sword     make_iron_sword
defeat_zombie   defeat_skeleton
wake_up
```

**最关键观察**：
- 没有 `hunt_wolf` / `kill_animal`：只奖励 `eat_cow`（食物）+ `defeat_zombie/skeleton`（明确敌对）
- **中立动物不奖励攻击** —— 你的 first_hunt 直接违反这条原则

### Reward 设计

```
+1   解锁新 achievement (per-episode 首次)
+0.1 恢复 1 点生命
-0.1 损失 1 点生命
```

**总 reward 量级 ~10-15**。死亡只是失去 max ~ 10。**没有 -100 死亡惩罚**。

### 评估指标（不看 reward）

```
score = exp(mean(log(1 + success_rate_i))) - 1
```

**几何均值**：单个 achievement 100% 主导不了总分，必须广泛解锁才有高分。

### Baseline (5M steps)

| Method | Score% | Return |
|---|---:|---:|
| Human Expert | 50.5 | 14.3 |
| DreamerV2 | 10.0 | 9.0 |
| **PPO** | **4.6** | **4.2** |
| Random | 1.6 | 2.1 |

**纯 PPO + sparse reward 上限 ~4.6%**。学术界普遍现象，**不是你实现问题**。

### 你能直接借鉴的

1. **reward 量级压回 +1 级**（你现在 first_X +20 是 20 倍 off）
2. **删除中立动物的击杀奖励**
3. **死亡 -10 而不是 -100**
4. **评估用 success rate 几何均值**，不看 mean return

---

## 2. PBRS — Ng/Harada/Russell ICML 1999

[原始论文](https://people.eecs.berkeley.edu/~russell/papers/icml99-shaping.pdf) · [综合页](https://www.emergentmind.com/topics/potential-based-reward-shaping)

### 核心定理（数学保证）

定义势函数 Φ: S → ℝ，**唯一**不改变最优策略的 shaping 形式：

```
F(s, a, s') = γ·Φ(s') - Φ(s)
R'(s,a,s') = R(s,a,s') + F(s,a,s')
```

**定理**：对任意 bounded Φ，R 与 R' 有相同最优策略。

```
Q_R'(s,a) = Q_R(s,a) - Φ(s)     # 仅常数偏移
```

**进一步（Wiewiora 2003）**：任何 reward shaping 若不改最优策略，**必然**等价于某个 PBRS。

### 你"持续状态价值"的数学化

```python
Φ(state) = w1·has_house + w2·has_clothes 
         + w3·n_followers + w4·inventory_diversity

# 每步附加 reward：
F = γ·Φ(s') - Φ(s)

# 行为：
建好房子那一步     → F ≈ γ·w1            （一次性正）
保持有房子状态     → F ≈ -(1-γ)·Φ        （持续小正，因为 γ<1）
房子被毁那一步     → F ≈ -w1             （一次性负）
```

**这就是"有家持续感"的精确实现**。

### 实战数据（Yang 2021 Sokoban）

| 任务 | Baseline | PBRS | 加速 |
|---|---:|---:|---|
| 1-box | 60k ep | 15k ep | 4× |
| 2/3-box | 卡在 <20% | 40-50k 达 100% | 质变 |

### 设计 Φ 的方法

1. **手工**：负欧氏距离到目标
2. **A\* 启发**：负规划路径长度
3. **学到的**：用 GCN 在 transition graph 上传播
4. **bootstrap**：Φ = 当前 V 估值（自举）

### 你能直接借鉴的

```cpp
// RewardSystem 加一项：
float potential(EnvState s) {
    return 0.5f * (s.has_house ? 1.0f : 0.0f)
         + 0.3f * (s.has_clothes ? 1.0f : 0.0f)
         + 0.2f * s.n_followers
         + 0.1f * inventory_diversity(s);
}

// 每 step:
float F = gamma * potential(s_next) - potential(s);
reward += F;
```

**理论保证不会跑偏**。Φ 权重不准也只是减速学习，不改最优策略。

---

## 3. Achievement Distillation — Moon NeurIPS 2023

[arxiv:2307.03486](https://arxiv.org/abs/2307.03486) · [GitHub: snu-mllab/Achievement-Distillation](https://github.com/snu-mllab/Achievement-Distillation)

### 核心定位
Crafter 当前 SOTA。**不改 reward，改 architecture**。比 DreamerV3 强，参数更少。

### 关键观察

> "The PPO agent **can predict the next achievement** to be unlocked to some extent, albeit with limited confidence."

PPO 的 hidden state 里已经隐含"下一步该做什么"，只是没显式监督。

### 方法

```
auxiliary phase (PPO update 之后):
  1. 从 off-policy buffer 采 trajectory
  2. 对每个 (state, achievement_just_unlocked)：
     - positive: (s_t, g_j)            # 真实 achievement
     - negative: (s_t', g')            # 随机其他 achievement
  3. contrastive loss: encoder(s) 距离 g 比距离 g' 近
  4. 同时正则化 policy / value 输出不变（防止破坏 PPO）
```

整体架构：
```
[CNN encoder] → [policy head]
              → [value head]
              → [achievement prediction head]  ← 新加，contrastive 训练
```

### 性能

```
Crafter 1M steps:
  PPO 标准:       ~9%
  PPO + AD (本文): ~21%
  DreamerV3:       ~14%
```

### 集成成本

需要：
1. 记录 trajectory 中 achievement 解锁时刻
2. 加 contrastive loss 训练循环
3. 加 auxiliary phase（每 N 个 PPO update 后跑一次）
4. 模型新增 prediction head（~10k 参数）

**对你的项目**：架构改动大（PPO.cpp 改），但 reward 不动。**远期目标**，不是 D-043 范围。

### 你能直接借鉴的

短期不用全套，但有 1 个轻量启发：
- **在 jsonl log 中记录每个 milestone 的 unlock 时刻 + observation snapshot**
- 后续可以离线训练一个"next milestone" classifier 验证 PPO encoder 是否学到了序列
- 验证后再决定要不要全量 AD

---

## 4. Homeostatic RL — Keramati & Gutkin 2014

[原始论文 (eLife)](https://elifesciences.org/articles/04811) · 后续 [CTCS-HRRL 2024](https://arxiv.org/abs/2401.08999)

### 核心定位
drive reduction 理论的 RL 数学化。你 `satiation_scale` 的完整版。

### Drive 函数（关键公式）

```
H_t = (h_1, h_2, ..., h_N)     # 内部状态向量（你的 thirst, hunger, fatigue）
H*  = (h_1*, ..., h_N*)         # 设定点 setpoint（理想值）

D(H_t) = (Σ_i |h_i* - h_i,t|^n) ^ (1/m)     # 多维偏离

reward(t→t+1) = D(H_t) - D(H_{t+1})        # drive 减少量
```

**核心定理**：最大化 Σ γ^t·reward = 最小化 Σ γ^t·D(H)。reward maximization ↔ homeostasis maintenance。

### 行为预测（很匹配你项目）

| 内部状态 | 该论文预测 | 你的现状 |
|---|---|---|
| thirst > 80 | 喝水 reward 巨大，其他几乎无效 | satiation_scale 硬截断（对的方向但粗糙） |
| thirst = 50 | 喝水 reward 中等 | 你给固定值（不区分） |
| thirst = 10 (饱) | 喝水 reward → 0 | 你强制为 0（对） |
| thirst = 0 (满) | 继续喝甚至 punish | 你没建模 |

### "反向 drive 抑制"

> "deprivation level for one need has an **inhibitory effect** on the reinforcing value of outcomes that satisfy irrelevant needs."

**翻译**：渴极了的时候，盖房子的 reward 该被压低（先求生）。
**翻译**：饱腹但缺保暖时，穿衣 reward 该正常起作用。

实现方式：
```python
def reward_scale_for_milestone(state):
    # 当任一 vital drive > 阈值，抑制非生存类 reward
    max_drive = max(drive(thirst), drive(hunger), drive(fatigue))
    if max_drive > 0.7:
        return 0.3        # 危机模式
    elif max_drive > 0.4:
        return 0.7        # 警觉模式
    else:
        return 1.0        # 发展模式
```

### 你能直接借鉴的

1. **VitalSystem 加 drive function 计算**（每帧一次）：
   ```cpp
   float drive_thirst = std::pow(std::abs(thirst_setpoint - thirst) / 100.f, 2.0f);
   ```
2. **eat/drink reward 改成 drive_reduction**（而不是固定值）：
   ```cpp
   float reward_drink = drive_thirst_before - drive_thirst_after;
   ```
3. **milestone reward 乘以"非危机系数"**（参考上面 max_drive 抑制）

这条**比 PBRS 更具体**：直接告诉你 satiation_scale 该怎么改。

---

## D-043 综合方案（精读完成后）

### 改动清单（按优先级）

```
P0 (必改):
  1. first_X 全部 +20 → +1                           [Crafter]
  2. 删 first_hunt（中立动物不奖励攻击）              [Crafter]
  3. 死亡 -100 → -10                                  [Crafter]
  4. 评估改用 success rate 几何均值                    [Crafter]

P1 (建议):
  5. 加 reward clip [-5, +5]                          [PPO bag of tricks]
  6. eat/drink 改成 drive_reduction 公式               [Keramati 2014]
  7. milestone 乘以非危机系数（max_drive < 0.7 时全额） [Keramati 2014]

P2 (锦上添花):
  8. 加 Φ 状态势：has_house + has_clothes + n_followers
     + 0.1·inventory_diversity                        [Ng 1999]
  9. 每 step 加 F = γ·Φ(s') - Φ(s)                   [Ng 1999]

P3 (远期):
  10. 在 jsonl 记录 milestone unlock 时刻 + obs       [Moon 2023 验证基础]
  11. (远期) auxiliary head + contrastive loss        [Moon 2023]
```

### 预期效果

| 维度 | 现状 (run7) | P0 后 | P0+P1 后 | P0+P1+P2 后 |
|---|---|---|---|---|
| 撩狼率 | 高 | 0（删 first_hunt） | 0 | 0 |
| 死亡率 | 24% | ~10%（reward 平衡） | ~5%（drive 引导） | ~5% |
| last 20 mean | -3.49 | +5～+15 | +15～+30 | +30～+50 |
| 成就广度 | 中 | 中 | 中高 | 高 |

数字是个人估计，需要训练验证。

### 工作量

- P0：30 分钟编码 + 2h 训练验证
- P0+P1：3h 编码 + 4h 训练
- +P2：另加 4h 编码 + 4h 训练
- P3 不在 D-043 范围

### 关键设计原则（贯穿）

1. **一次只改一类维度**（PPO 对 reward 极敏感，需要归因）
2. **小步训练验证**（每改一类用 200k step smoke 看趋势）
3. **始终保留死亡惩罚**（Crafter 也有 -0.1/health；不要全部 PBRS 化）
4. **Φ 函数选择 bounded 函数**（Ng 定理要求 bounded，否则保证失效）
