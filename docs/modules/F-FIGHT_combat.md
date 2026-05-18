# F-FIGHT · 对抗机制

> 让个体能互相伤害，能量在物种间转移。

- **难度**：🟡 中
- **确定性档**：🟡 中（对抗涌现稳定但策略复杂度有限）
- **前置条件**：M5 完美完成
- **状态**：📋 待开发

---

## 一句话

新增 AttackCapability：网络可输出"攻击意图 + 攻击方向"。被攻击者能量减少，攻击者能量增加（部分转移）。

---

## 你会看到什么效果

### 完美版
进化出"军备竞赛"：
- 攻击型基因：网络输出强烈攻击信号，主动靠近其它 agent
- 逃跑型基因：感知到附近 agent 立刻反向移动
- 防御型基因：在山洞内屯兵，让攻击者无法接近

### 标准版
攻击行为存在但策略单一（要么全员攻击要么全员逃跑）。

### 降级版
不太可能完全失败 —— 至少能看到能量转移和适应度差异。

---

## 实现要点

### AttackCapability

```cpp
struct AttackOutput {
    float intensity;     // 0-1，是否要攻击
    Vec2 direction;      // 攻击方向
    float range;         // 默认固定 5 个单位
};

class AttackCapability : public IAgentCapability {
    void apply(Agent& self, const World& world, const NeuralOutput& nn_out) override {
        if (nn_out.attack_intensity > threshold) {
            auto victims = world.queryAgentsInCone(self.pos, nn_out.direction, range);
            for (auto& v : victims) {
                float damage = nn_out.attack_intensity * config.damage_factor;
                v.energy -= damage;
                self.energy += damage * config.energy_transfer_rate;
                eventBus.publish(AgentAttacked{self.id, v.id, damage});
            }
            self.energy -= config.attack_cost;
        }
    }
};
```

### 输出维度扩展

NEAT 输出维度新增 3 维（attack_intensity, attack_dx, attack_dy）。

### 视觉表现

- 攻击者出手时画一道短闪光线
- 被攻击者掉血动画（红色闪烁）
- HUD 显示当前总伤害量统计

### 奖励

```
src/core/reward/
└── CombatReward.cpp
```

- 攻击成功（造成伤害）：+x
- 杀死目标：+y（y >> x）
- **被攻击致死**：fitness 大幅扣分（变相奖励防御 / 逃跑）

---

## 三档完美标准

### MVP ✅⛳⛳
- [ ] AttackCapability 接入，能量转移工作
- [ ] 视觉表现有反馈

### 完整 ✅✅⛳
- [ ] 攻击 / 逃跑两种策略都能在种群中观察到
- [ ] 攻击事件统计可用

### 完美 ✅✅✅
- [ ] 物种内或物种间出现稳定的"军备竞赛"
- [ ] git tag `module-F-FIGHT-done`

---

## 🧠 你的补充点

| 项 | 决定什么 |
|---|---|
| **damage_factor** | 单次攻击的伤害强度 |
| **energy_transfer_rate** | 多少能量从受害者转给攻击者（吸血效率） |
| **attack_cost** | 攻击的能量消耗 |
| **是否可识别敌我** | 输入是否包含"对方物种 ID" |

---

## ⚠️ 易踩坑

1. **过度暴力**：damage_factor 太高 → 全员速死，进化没机会发挥
2. **同物种内卷**：默认敌我不分，可能进化出同物种攻击。**可选**：相同物种攻击不获得能量转移
3. **攻击边界检测性能**：用 spatial hashing 加速

---

## 答辩话术

> "我没有定义'天敌'或'食物链'。是进化自己分化出了攻击型和防御型——它们在同一片土地上演化出对立的策略。这层是 F-PRED 多物种共进化的基础。"
