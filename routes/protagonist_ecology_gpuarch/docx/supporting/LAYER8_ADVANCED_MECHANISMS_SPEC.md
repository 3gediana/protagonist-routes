# Layer 8 · 高级机制设计规格 (FINAL_BLUEPRINT.md §8)

> 状态: 设计阶段（实现 deferred）
> 4 个独立子项目，按用户优先级展开

---

## 1. Lamarckian (Hebbian online weight update)

### 1.1 设计

NEAT 是 evolutionary，无 backprop；但 Hebbian / 神经调节可以让 agent 在
episode 内 online update 权重，episode 结束把更新后的权重传给后代。

```cpp
// 每 tick 末尾
NeatBrain::hebbianUpdate(float learning_rate) {
    for (each connection_gene) {
        if (post_synaptic.modulatory_signal > 0) {
            connection.weight += lr * pre.activation * post.activation;
        }
    }
}
```

modulatory_signal 来自 reward gradient (近似 dopamine)。

### 1.2 集成点

- ProtagonistEvolution::reproduce 调用 child 的 weights = parent_a.currentWeights() (vs
  原始 genome weights) — Lamarckian 继承
- 选项 toml: `[evolution.lamarckian] enabled, learning_rate, modulatory_threshold`

### 1.3 工作量

- NeatBrain::hebbianUpdate: 已存在 (现在没 wire 到 reward)
- ProtagonistEvolution: ~50 行新代码使用 currentWeights
- 测试: ~100 行
- 总: **~200 行**

---

## 2. World Model (DreamerV2 RSSM)

### 2.1 设计

让 protagonist brain 包含一个 transition predictor:
- input: state_t, action_t
- output: state_{t+1} prediction + reward prediction

学好的 world model 可以做"想象 rollout" — 内在动机（curiosity = prediction error 大的状态）。

### 2.2 集成点

- ProtagonistBrain 加 RSSM module (LSTM cell)
- 训练 loss = (predicted_next - actual_next)²
- 内在 reward = ‖prediction_error‖

### 2.3 工作量

巨大。需要：
- LSTM cell 实现 (200 行)
- RSSM forward + backward (300 行)
- training loop (200 行)
- intrinsic reward integration (50 行)
- 总: **~750 行 + 大量调试**

不建议作为第一优先级。

---

## 3. DIAYN (Diversity Is All You Need)

### 3.1 设计

让 protagonist 学习无监督 "skills" — N 个 latent skill 编码，agent 学会
"在 skill k 下表现 distinctly"。reward = -log p(skill_k | state)。

适合作为 protagonist 的"风格"涌现：有的 protagonist 是 "战士"，有的是 "建造者"，有的是 "侦察"。

### 3.2 集成点

- 加 SkillDiscriminator (small MLP) 输入 state，输出 skill probability
- protagonist brain 输入加 skill_one_hot
- reward = α × original_reward + β × diversity_reward

### 3.3 工作量

- SkillDiscriminator: ~100 行
- protagonist brain skill input: ~30 行
- diversity reward: ~40 行
- 训练 loop 修改: ~50 行
- 总: **~220 行**

中等难度。最直接显示 protagonist 风格分化。

---

## 4. Emergent Communication (VICE-style + Signal MI)

### 4.1 设计

VICE = Variational Information Contracts via Events。让 signal 的语义"涌现"
靠 event-conditioned reward：

reward(emit, listen) = +α 当 emit 出现在 event_X 前 N tick 内 AND listen 后跟随 event_Y。

### 4.2 已有工作

- `scripts/protagonist_communication_mi.py` 已经能算 channel_active vs predator_nearby MI
- Phase 3 (commit 之前) verify channel-0 MI = 0.36 bits

### 4.3 待做

- 把 event-conditioned reward 加入 ProtagonistEcologyConfig.reward
- 训练时 monitoring MI 作为新的 fitness 维度
- 加入 BD (behavioral descriptor) for ME-NEAT archive

### 4.4 工作量

- event-conditioned reward: ~80 行 C++
- BD signal_mi 维度: ~30 行
- python script extension: ~50 行
- 总: **~160 行**

低-中等。最接近用户 vision 的"通信涌现"。

---

## 5. 优先级建议

### 5.1 按 user value
1. **DIAYN** — protagonist 风格分化，可视化效果最直接
2. **Emergent Communication** — 已有 baseline, fast iteration
3. **Lamarckian** — 单行配置开关，快速实验
4. **World Model** — 高投入高回报但风险大

### 5.2 按工作量 vs 收益
- 短期 (1 session): Lamarckian + Emergent Communication baseline
- 中期 (2 sessions): DIAYN
- 长期 (3+ sessions): World Model

---

## 6. 与 Layer 6/7 的接口

### Layer 6 (主世界)
- Lamarckian 修改的 weights 必须随 protagonist 序列化
- DIAYN 的 skill 标签作为 lineage 元数据

### Layer 7 (Viewer)
- 可视化 protagonist DNC memory bank（attention heatmap）
- DIAYN skill colormap 给 protagonist 上色（fighter=红，builder=蓝，scout=绿）
- emergent communication 的 signal 频谱图实时显示

---

## 7. 决策建议

**推荐启动顺序**:
1. Lamarckian (toml 开关，~200 行 C++) — 最便宜验证概念
2. DIAYN baseline (~220 行 C++) — 最直观可视化效果
3. Emergent Communication 加 reward + BD (~160 行 C++)
4. World Model deferred to last

每完成一个 sub-layer 重新生成 Layer 7 viewer 给用户看效果。

---

## 8. 风险

- Lamarckian: weight drift 可能让 episode 结束时 agent 表现失控；需 regularization
- DIAYN: skill 数量 N 是超参数，太多会冲淡每个 skill 的特征
- World Model: 训练不稳定，可能让 protagonist policy 退化（well-known 问题）
- Emergent Communication: signal 频谱可能稀疏，BD MI 信号不稳

每个 sub-layer 都需要"消极结果"的诚实记录。
