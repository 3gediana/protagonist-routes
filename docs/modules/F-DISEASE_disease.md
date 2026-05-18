# F-DISEASE · 疾病传播

> 邻接传染，逼出"社交距离基因"。

- **难度**：🟡 中
- **确定性档**：🟡 中
- **前置条件**：M5 完美完成
- **状态**：📋 待开发

---

## 一句话

随机时刻给某个 agent 打上"感染"标记。感染状态让该 agent 能量消耗加倍，并以一定概率传染给附近 agent。一段时间后自愈或死亡。

---

## 你会看到什么效果

### 完美版
种群进化出"避开密集区"行为，物种间产生不同的"社交距离"基因——某些物种紧密群居，某些物种保持距离。

### 标准版
感染传播能跑，但行为分化不一定显著。

## 实现要点

```cpp
class DiseaseLayer : public IWorldLayer {
    void tick() override {
        for (auto& agent : world.agents) {
            if (agent.infected) {
                agent.infection_progress += dt;
                if (agent.infection_progress > duration)
                    agent.recover_or_die();
                infectNearby(agent);
            }
        }
    }
};
```

可加 SusceptiblePerception：感知附近被感染 agent 的数量。

## 三档完美标准

- MVP：感染传播工作，自愈/死亡机制完成
- 完整：感知输入接入，跑 100 代有可观察的群体免疫现象
- 完美：观察到 agent 行为对密度敏感

## 🧠 补充点

- 传染概率
- 感染持续时间
- 自愈/死亡概率
- 是否有"免疫"基因（部分个体抗感染）

## 答辩话术

> "看种群密度曲线——疾病爆发时密度急速下降，恢复时回升。这是经典的 SIR 模型，但它在我的世界里不是公式驱动，是 agent 自己的避险行为驱动。"
