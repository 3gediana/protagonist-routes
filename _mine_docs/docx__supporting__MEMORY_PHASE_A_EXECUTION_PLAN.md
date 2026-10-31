# Protagonist Ecology · Memory Phase A 接线实施计划

> 位置：`routes/protagonist_ecology/docx/supporting/MEMORY_PHASE_A_EXECUTION_PLAN.md`
>
> 目标：把当前已经存在但未闭环的 `Spatial / Episodic / Social / Goal` 记忆区真正接通，作为 `MEMORY_SYSTEM_MASTERPLAN.md` 的第一落地阶段。
>
> 状态：**A1~A5 已基本落地，A6（memory ablation）仍是 partial**（2026-05-21）
>
> 说明：本文件保留原始实施思路，但所有“当前缺口 / 推荐顺序 / 完成标准”已按当前源码树重写；不要再把这里当成 Phase A 开工前的旧现状。

---

## 1. Phase A 的定位

Phase A 不追求一步到位上完整 `ProtagonistBrainV3`。

Phase A 只做一件事：

**把当前 Memory V2 从“有类、有接口、部分写入”推进到“已进入基础决策闭环，但还需要更强 query / ablation 验证”的可用系统。**

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

## 2. 当前真实状态与剩余缺口

### 2.1 Spatial

当前已经不是旧版那种 6 槽脚手架。`SpatialMemoryBank` 现在已有：

- `water_seen`
- `food_seen`
- `tree_seen`
- `stone_seen`
- `stick_seen`
- `danger_seen`
- `base_seen`
- `fire_seen`
- `worksite_seen`
- `last_visit_age`

runtime 也已经真实写入：

- 水源
- 食物资源
- 树
- 石头 / 木棍
- 捕食者危险
- 基地
- 篝火
- 工地

所以当前剩余缺口更准确是：

- 已不再存在 `base / campfire / worksite` 混槽问题
- 但仍主要是 coarse grid + nearest-query，不是更深的 place memory
- success/failure/task-chain 这类更任务化 spatial query 还没做

### 2.2 Episodic

当前 `EpisodicMemoryBank` 已不再只写三种事件。runtime 现在已写入：

- `ate_food`
- `saw_predator`
- `drank_water`
- `attacked_enemy`
- `was_attacked`
- `killed_prey`
- `completed_worksite`
- `chopped_tree`
- `built_worksite`
- `deposited_stick`
- `deposited_stone`
- `signal_emitted`

`MemoryCognitionPerception` 也已经会读取：

- `topSalient(1)` 的位置 / salience / recency
- `recentOfType("was_attacked")`
- `recentOfType("attacked_enemy")`

所以剩余缺口是：

- query 深度仍浅
- 事件类型还没完全覆盖 `throw_hit / crafted_spear / signal_responded` 等链条
- 还没有针对建造链 / 通信链的专项 episodic eval

### 2.3 Social

当前 `SocialMemoryBank` 已有：

- `recordSighting`
- `rewardCooperation`
- `rewardCommunication`
- `recordHostility`

现在真正被用到的已经不只 `recordSighting`，而是：

- `recordSighting`
- `rewardCooperation`
- `rewardCommunication`
- `recordHostility`

`MemoryCognitionPerception` 也已经会读取 social query：

- 最佳盟友位置 + cooperation / communication 分数
- 最近捕食者位置 + hostility 分数

剩余缺口是：

- 仍没有通信语义任务
- 仍没有 social ablation / listener shift / 互信息类指标
- social 现在是“基础读回已接通”，还不是完整 communication research stack

### 2.4 Goal

当前 `GoalMemory`：

- 会 decay
- 能 `setGoal / reinforce / progress`

runtime 中现在已经有一套规则化 goal refresh 逻辑，会在低频条件下切换：

- `Flee`
- `FindWater`
- `BuildWorksite`
- `GatherStick`
- `GatherStone`
- `RespondSignal`
- `RestNearFire`
- `FindFood`
- `Explore`

同时仍保留：

- 吃到食物 / 补水 / 建造推进时的 `reinforceCommitment()`
- `completionProgress()` 更新

但这里有一个当前代码事实需要写清：

- `EmitSignal` 分支已经写进 runtime
- 但它目前被前面的 `Flee` 条件遮蔽，**还不能算稳定可达的现行 goal**

所以当前剩余缺口是：

- 已有最小 goal selection/update 闭环
- 但还不是 Manager/Worker 层级目标系统
- 也还没有 goal 分布 / 持续时间 / 长链完成度的系统验收

### 2.5 MemoryCognitionPerception

当前已经不是 12 维。现在输出共 **44 维**：

- spatial 18 维（9 组 dx/dy）
- episodic 6 维
- social 8 维
- goal 4 维
- DNC 摘要 8 维

所以当前剩余缺口已经不是“维度太小”，而是：

- query 语义仍偏保守
- 44 维虽足以承载 Phase A 基础闭环，但还不等于更高阶 cognition interface 已完成

---

## 3. Phase A 当前结构

当前结构已经是：

```text
runtime events / local sensing
    -> Spatial / Episodic / Social / Goal write paths
    -> MemoryCognitionPerception(44 dims)
    -> ProtagonistBrain current dense trunk
    -> behavior changes visible in eval
```

核心变化：

- 现有 dense trunk 没有被整体重写，但 memory 输入已经变成真实可用
- `BodyStateMemory.summary()` / `WorkingMemoryBank.summary()` 也开始折回现有 learnable path
- 现阶段的主要缺口已从“有没有闭环”转成“query 深度 / 专项任务 / ablation 够不够强”

---

## 4. 具体改动顺序

> 下面各 Step 保留原始实施建议，但标题后的状态以 **2026-05-21 当前源码树** 为准。

## 4.1 Step A1 · 重构 SpatialMemoryBank 的语义槽（已落地）

### 目标

先解决当前最明显的“混槽”和“定义了没写入”。

### 当前结果

- `recordSighting(...)` 已扩到 `water / food / tree / stone / stick / danger / base / fire / worksite`
- runtime 主写入点已接通这些主要对象类型
- query 层已能分别取 `water / food / tree / danger / base / fire / worksite / stone / stick`

### 建议修改文件

- `brain/SpatialMemoryBank.h`
- `brain/SpatialMemoryBank.cpp`
- `runtime/ProtagonistEcologyRuntime.cpp`
- 可能还要补对应测试

### 建议改法

当前接口已经是：

```cpp
recordSighting(Vec2 world_pos,
               float water,
               float food,
               float tree,
               float stone,
               float stick,
               float danger,
               float base,
               float fire,
               float worksite)
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

## 4.2 Step A2 · 扩展 EpisodicMemoryBank 事件覆盖（已落地）

### 目标

让 episodic 不只是三条事件。

### 当前结果

- 基础事件覆盖已明显扩展到 `ate_food / saw_predator / drank_water / attacked_enemy / was_attacked / killed_prey / completed_worksite / chopped_tree / built_worksite / deposited_* / signal_emitted`
- `MemoryCognitionPerception` 已做基础 readback

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

## 4.3 Step A3 · 把 SocialMemory 写成真关系表（已落地，但仍初级）

### 目标

让 social memory 从“看见谁”变成“知道谁更值得靠近/回避”。

### 当前结果

- `recordSighting / rewardCooperation / rewardCommunication / recordHostility` 都有真实写入
- perception 已能读到最佳盟友 / 最近捕食者及对应分数摘要
- 但 communication semantics 与专项 social metrics 仍未做

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

## 4.4 Step A4 · 做一个最小 GoalManager（已落地，但仍规则化）

### 目标

先把 goal 从默认摆设变成低频持续变量。

### 当前结果

- runtime 已有 rule-based low-frequency goal updater
- `GoalMemory` 已不再长期停在默认 `Explore`
- 但 `EmitSignal` 分支当前仍被 `Flee` 顺序遮蔽，说明通信 goal 还没完全收口

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

## 4.5 Step A5 · 重写 MemoryCognitionPerception 为 V2（已落地，当前 44 维）

### 目标

把现有 memory 真的喂给主脑。

### 当前结果

- 当前版本已经是 44 维，不再是 12 维极简摘要
- spatial / episodic / social / goal / DNC 摘要都已进入统一 perception 向量
- `BootstrapPopulation` 的 sensory dim 也已同步变大

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

## 4.6 Step A6 · 加最小 memory ablation eval（partial）

### 目标

证明 memory 不只是装饰。

### 当前结果

- 最小 smoke harness / wiring 已有
- 但长训 ablation、专项任务解释与更强证据仍未收口

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

## 5. 当前阶段完成度

- [x] `SpatialMemoryBank` 语义拆槽
- [x] `MemoryCognitionPerception` 扩维到 Phase A 版本
- [x] `GoalManager` 最小闭环
- [x] `EpisodicMemory` 扩写入 + 基础查询
- [x] `SocialMemory` cooperation / hostility / communication 接线 + 基础查询
- [~] `memory ablation eval`（smoke / harness 已有，长训和解释仍不足）

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

## 7. 当前判定

按 2026-05-21 当前源码树，更准确的结论是：

- `Spatial / Episodic / Social / Goal` 四类 memory **已进入基础决策闭环**
- `MemoryCognitionPerception` 扩维 **已完成**
- goal 默认常驻 / social 长期全 0 / spatial 混槽 这些旧问题 **已基本解决**
- 但 **memory ablation 的长训与解释仍是 partial**

所以这份 Phase A 文档现在不该再被理解成“未开工计划”，而应理解成：

**接线已大体落地，验证与更强行为证据还没完全收口。**

---

## 8. 当前更合理的下一步

不要再默认把下一步写成“继续堆更多 BrainV3 壳层”。当前更合理的顺序是：

- 先把 `memory ablation / 专项任务验证` 真收口
- 或把 `ContributionLedger / communication semantics` 里至少一个 partial phase 收口
- 之后再继续更深的 `ProtagonistBrainV3 / GenomeV2 / mixed GPU`

一句话：

**Phase A 已经把当前这套 Memory V2 基本接活；现在缺的主要不是“再开新坑”，而是把已有闭环真正验出来。**
