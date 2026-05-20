# Protagonist Ecology · Memory Phase A 接线实施计划

> 位置：`routes/protagonist_ecology/docx/MEMORY_PHASE_A_EXECUTION_PLAN.md`
>
> 目标：把当前已经存在但未闭环的 `Spatial / Episodic / Social / Goal` 记忆区真正接通，作为 `MEMORY_SYSTEM_MASTERPLAN.md` 的第一落地阶段。

---

## 1. Phase A 的定位

Phase A 不追求一步到位上完整 `ProtagonistBrainV3`。

Phase A 只做一件事：

**把当前 Memory V2 从“有类、有接口、部分写入”推进到“真能影响行为决策、真能做消融验证”的可用系统。**

不在本阶段做的内容：

- 不重做完整 `ProtagonistGenomeV2`
- 不上 world model
- 不做 protagonist GPU 化
- 不重写整套行为主干
- 不做最终版因果账本

本阶段要做到的是：

- memory 不是 write-only
- goal 不是默认值摆设
- spatial 不是混槽脚手架
- social 不是只记“见过谁”
- `MemoryCognitionPerception` 不再只有 12 维的极简摘要

---

## 2. 当前真实缺口

### 2.1 Spatial

当前 `SpatialMemoryBank` 只有：

- `water_seen`
- `food_seen`
- `tree_seen`
- `object_seen`
- `danger_seen`
- `base_seen`
- `last_visit_age`

但 runtime 实际写入时：

- `base / campfire / worksite` 全写到 `base_seen`
- `tree_seen` 没有系统写入
- `danger_seen` 没有系统写入
- `object_seen` 把 `stone / stick` 混一起

### 2.2 Episodic

当前 `EpisodicMemoryBank` 能记录事件，但 runtime 只写：

- `ate_food`
- `attacked_enemy`
- `killed_prey`

且 `MemoryCognitionPerception` 完全没有读取 episodic。

### 2.3 Social

当前 `SocialMemoryBank` 已有：

- `recordSighting`
- `rewardCooperation`
- `rewardCommunication`
- `recordHostility`

但真正被用到的只有：

- `recordSighting`

而且 perception 没有读取 social query。

### 2.4 Goal

当前 `GoalMemory`：

- 会 decay
- 能 `setGoal / reinforce / progress`

但 runtime 中只看到：

- 吃到食物时，如果当前 goal 恰好是 `FindFood`，才 reinforcement

没有一套真正的 goal selection/update 逻辑。

### 2.5 MemoryCognitionPerception

当前只输出：

- water/base/object/danger 四组方向，共 8 维
- 当前 goal index/commitment/elapsed/progress，共 4 维

总计 12 维。

这远远不够承载现有 Memory V2，更别说后续大脑扩容。

---

## 3. Phase A 结果图

Phase A 完成后，预期结构是：

```text
runtime events / local sensing
    -> Spatial / Episodic / Social / Goal write paths
    -> MemoryCognitionPerceptionV2
    -> ProtagonistBrain current dense trunk
    -> behavior changes visible in eval
```

核心变化：

- 现有 dense trunk 先不重构，但 memory 输入要变得真实有用
- 先让 memory 真有行为影响，再谈 V3 架构与更大参数量

---

## 4. 具体改动顺序

## 4.1 Step A1 · 重构 SpatialMemoryBank 的语义槽

### 目标

先解决当前最明显的“混槽”和“定义了没写入”。

### 建议修改文件

- `brain/SpatialMemoryBank.h`
- `brain/SpatialMemoryBank.cpp`
- `runtime/ProtagonistEcologyRuntime.cpp`
- 可能还要补对应测试

### 建议改法

把当前单函数：

```cpp
recordSighting(Vec2 world_pos, float water, float food, float tree, float object, float danger, float base)
```

升级成更清晰的结构化写入方式，例如：

```text
recordWaterSighting
recordFoodSighting
recordTreeSighting
recordStoneSighting
recordStickSighting
recordDangerSighting
recordBaseSighting
recordFireSighting
recordWorksiteSighting
recordSuccessAt
recordFailureAt
```

如果暂时不想拆多个函数，也至少要把 cell 字段扩成：

```text
water_seen
food_seen
tree_seen
stone_seen
stick_seen
danger_seen
base_seen
fire_seen
worksite_seen
success_score
failure_score
last_visit_age
```

### runtime 写入点

主要仍在：

- `runtime/ProtagonistEcologyRuntime.cpp` 约 938~1018 行这一段 memory 写入逻辑

### Phase A 验收

- trace 中可以区分 base/fire/worksite
- `tree_seen`、`danger_seen`、`stone_seen`、`stick_seen` 不再是空摆设
- query 层能够分别取这些语义点

---

## 4.2 Step A2 · 扩展 EpisodicMemoryBank 事件覆盖

### 目标

让 episodic 不只是三条事件。

### 建议修改文件

- `brain/EpisodicMemoryBank.h`
- `brain/EpisodicMemoryBank.cpp`
- `runtime/ProtagonistEcologyRuntime.cpp`

### 建议新增事件类型

先用字符串即可，不急着马上换 enum：

```text
ate_food
drank_water
took_damage
saw_predator
killed_prey
crafted_spear
throw_hit
deposited_stick
deposited_stone
completed_worksite
signal_emitted
signal_responded
reached_base_low_hydration
escaped_danger
```

### 写入策略

优先利用当前 runtime 已有的行为统计/事件线索，先补这类事件：

- 吃 / 喝
- 受伤
- 杀死猎物
- 完成工地
- 成功 craft
- 成功 throw hit
- 发出信号
- 响应信号
- 低水状态回到基地/火点

### Phase A 验收

- `topSalient()` 真能看到多样关键事件
- `recentOfType()` 对行为测试有用
- trace 中 episodic 和记忆查询能对应起来

---

## 4.3 Step A3 · 把 SocialMemory 写成真关系表

### 目标

让 social memory 从“看见谁”变成“知道谁更值得靠近/回避”。

### 建议修改文件

- `brain/SocialMemoryBank.h`
- `brain/SocialMemoryBank.cpp`
- `runtime/ProtagonistEcologyRuntime.cpp`
- 如果信号事件/工地事件有更集中落点，也需要接进去

### 必接的写入

- `recordSighting()`：保留
- `rewardCooperation()`：在共同建造/共同围猎成功时写
- `rewardCommunication()`：在信号被响应且产生收益时写
- `recordHostility()`：在遭遇伤害/敌对碰撞时写

### 最小可用 query

建议至少补：

- `mostCooperative(1)`
- `mostHostile(1)`
- 最近 seen 的 cooperative peer
- 最近 seen 的 hostile peer

### Phase A 验收

- 某些 peer 的 cooperation / hostility 分数不再长期全 0
- perception 能读到这些关系摘要
- 社交消融后，协作/避险类指标明显下降

---

## 4.4 Step A4 · 做一个最小 GoalManager

### 目标

先把 goal 从默认摆设变成低频持续变量。

### 建议修改文件

- `brain/GoalMemory.h`
- `brain/GoalMemory.cpp`
- `runtime/ProtagonistEcologyRuntime.cpp`
- 视实现方式可能新增 route-local 小工具文件

### 不需要复杂神经 goal manager

Phase A 可以先上**规则化 low-frequency goal updater**，因为目标是闭环，不是最终最优形式。

### 建议最低规则

每隔若干秒更新一次目标，基于：

- hydration 低 -> `FindWater`
- hunger 高 -> `FindFood`
- 近距离 danger -> `Flee`
- 看到 active worksite 且持物/附近有资源 -> `BuildWorksite`
- 附近 signal 强且条件满足 -> `RespondSignal`
- 夜晚且火点/base 已知 -> `RestNearFire` 或 `ReturnToBase`
- 否则 `Explore`

### 同时保留

- `reinforceCommitment()`
- `progress()`

### Phase A 验收

- goal 分布不再几乎全是 `Explore`
- goal 平均持续时间有明显分布
- goal 与行为选择能对应起来

---

## 4.5 Step A5 · 重写 MemoryCognitionPerception 为 V2

### 目标

把现有 memory 真的喂给主脑。

### 建议修改文件

- `perception/MemoryCognitionPerception.h`
- `perception/MemoryCognitionPerception.cpp`
- `runtime/BootstrapPopulation.cpp`

### 建议扩展到 32~48 维

建议 Phase A 先上 32 维左右，不要一步爆太大。

#### 空间记忆建议 16 维

```text
nearest_water_dx, dy
nearest_food_dx, dy
nearest_danger_dx, dy
nearest_base_dx, dy
nearest_fire_dx, dy
nearest_worksite_dx, dy
nearest_stone_dx, dy
nearest_stick_dx, dy
```

#### episodic 建议 4~8 维

```text
recent_damage_event_dx, dy
recent_water_success_dx, dy
salient_event_score
recent_signal_success_score
```

#### social 建议 4~8 维

```text
best_peer_dx, dy
hostile_peer_dx, dy
cooperation_score_max
hostility_score_max
```

#### goal 建议 4~8 维

```text
current_goal_norm
commitment
elapsed_norm
progress
interrupt_pressure
```

### Phase A 验收

- `MemoryCognitionPerception` 不再是 12 维极简输入
- `BootstrapPopulation` 感知维度同步更新
- `ProtagonistBrain` 输入维度正确增大并继续可编译运行

---

## 4.6 Step A6 · 加最小 memory ablation eval

### 目标

证明 memory 不只是装饰。

### 建议修改文件

- protagonist route eval / runtime 配置读取逻辑
- 可能新增小型 eval 场景配置
- 结果输出 csv/summary

### 最低 ablation 开关

```text
memory.enable_spatial
memory.enable_episodic
memory.enable_social
memory.enable_goal
```

### 必做对照测试

- water return
- danger avoidance
- worksite return
- signal response

### Phase A 验收

- 关掉某个 memory 后，对应指标显著下降
- 至少说明这些记忆区真的被用到了

---

## 5. 推荐实施顺序

推荐严格按这个顺序落：

1. `SpatialMemoryBank` 语义拆槽
2. `MemoryCognitionPerceptionV2` 扩维
3. `GoalManager` 最小闭环
4. `EpisodicMemory` 扩写入 + 扩查询
5. `SocialMemory` 接 cooperation / hostility / communication
6. `memory ablation eval`

原因：

- spatial + perception 是最立刻可见的收益
- goal 闭环会马上改变“行为是否持续”
- episodic/social 需要 query 通道先打通才有意义
- ablation 放最后收口

---

## 6. 风险点

### 6.1 输入维度增长会扰动现有训练

这是正常现象。

需要注意：

- `BootstrapPopulation` 的 sensory dim 要同步正确更新
- `ProtagonistGenome` 初始化/存盘兼容性要确认
- 旧 checkpoint/旧 best genome 可能不再兼容

### 6.2 规则 goal manager 可能太强或太死

Phase A 不追求最终最优。

只要它能提供：

- 低频稳定目标
- 明确可分析行为偏置

就已经比“默认 goal 摆设”前进很多。

### 6.3 social reward 接线容易误发分

在 cooperation / communication 信号接线时，必须避免：

- 路过者白拿合作分
- 纯 proximity 被当成 cooperation
- 发了信号但没人响应也拿高 communication 分

如果一开始不好完全做对，先采用**最保守记账**。

---

## 7. Phase A 完成标准

满足以下条件才算完成：

- `Spatial / Episodic / Social / Goal` 都进入真实决策闭环
- `MemoryCognitionPerception` 扩到足够承载现有 Memory V2
- goal 不再是默认 `Explore` 常驻
- social 分数不再长期全 0
- spatial 通道不再混槽
- 有一组最小 memory ablation 对照结果
- protagonist smoke / train configs 继续可编译、可运行、可 trace

---

## 8. Phase A 完成后再做什么

Phase A 完成后，才进入下一层：

- `ProtagonistBrainV3`
- `WorkingMemoryBank` 多时间尺度化
- `BodyStateMemory`
- `BehaviorFusionTrunk`
- `MotorCore`
- `ActionHeads`
- `ProtagonistGenomeV2`
- 后续 protagonist mixed GPU 路线

一句话：

**Phase A 的任务不是把记忆系统做成最终形态，而是先把当前这套 Memory V2 真正接活。**
