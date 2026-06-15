# Protagonist Ecology · 连续时间世界重构设计

> 位置：`routes/protagonist_ecology/docx/supporting/CONTINUOUS_TIME_WORLD_DESIGN.md`
>
> 状态：设计文档；continuous-time 主链已落地到 route runtime / trace / memory（2026-05-21），但更强连续语义与部分 legacy tick 兼容仍待继续清理
>
> 作用：为 `routes/protagonist_ecology/` 以及其依赖的 `src/core/` / `src/runtime/` 提供连续时间世界的重构路线，不在这里直接实现大改，只先把方向、边界、迁移顺序和验收标准定清楚。

---

## 0. 状态同步（2026-05-21）

当前源码树里已经落地：

- route runtime 使用 `episode_duration_seconds + substep_dt`
- 事件、memory、trace 同时记录 `tick + time_seconds`
- protagonist brain 前向已按 `dt` 做泄漏积分
- 主角路线默认以 seconds 口径推进，而 `tick/step_index` 更多承担兼容、日志和可重复性角色

当前仍未完全收口的部分：

- 部分配置和兼容层仍保留 legacy tick 别名
- `chop / build / worksite credit / event queue` 的更强连续化表达还可以继续推进

下面第 1 节及之后保留的是**当时为什么必须做这次重构**的设计论证，不再等同于 2026-05-21 的字面现状。

## 0.1 设计启动时的结论

设计这次重构时，系统虽然很多地方都传了 `dt`，但**真正的主时钟语义仍然是 `Tick`**：

- 世界主循环按 step 递增
- 事件只带 `tick`
- agent 决策调度依赖 `nextDecisionTick()`
- 年龄、代谢、死亡、资源刷新、行为统计都大量绑定 step 边界

这会导致一个核心问题：

**系统看起来像连续 2D 世界，但时间语义本质上还是离散调度器。**

因此本次重构的设计结论是：

- **不继续把新机制深绑在 `Tick` 上**
- **也不一口气改成纯离散事件仿真器（pure discrete-event simulation）**
- 采用 **hybrid continuous-time** 方案：
  - 世界主时钟切到 `seconds`
  - 运行时用**固定上限子步长**积分推进
  - 事件同时记录 `time_seconds` 与 `step_index`
  - agent 决策、宏动作持续时间、reward 结算都改成按真实持续时间处理
  - 需要严格排序的异步事件走**到期事件队列**，但世界主体仍保留连续状态积分

一句话：

```text
世界是连续时间 + 子步积分推进；
事件是异步时间戳；
step 只保留为可重复性、日志和兼容层，不再是世界真时间。
```

---

## 1. 为什么这件事当时必须前置

设计这份方案时，protagonist 路线已经进入：

- Trace / Resume 已完成
- Memory V2 已开始接线
- 后面还会继续上 EventLineage、ContributionLedger、通信语义、World Model、层级目标

这些后续模块都高度依赖时间语义。

如果现在不先把时间框架理顺，后果会非常具体：

- **Memory V2 会绑错时间尺度**
  - 例如事件记忆、目标坚持、社会关系衰减到底是“每 tick 衰减”还是“每秒衰减”会越来越混乱。
- **合作信用分配会错**
  - 两个动作相隔 0.05 秒和 0.9 秒，在离散 tick 下可能被算成同一格或相邻格，信用边界不稳定。
- **通信语义会被 step 粒度污染**
  - 信号响应、集合、协同行为对“谁先发生”“相差多久”非常敏感。
- **连续动作会被伪离散化**
  - move / flee / drink / chop / build / emit_signal 本来更像强度和持续时间，而不是每步按钮。
- **CTRNN 的价值发挥不出来**
  - CTRNN 的核心就是连续神经状态演化；如果世界事件全被 step 边界切碎，brain 连续了但环境不连续，收益会被打折。

所以用户这次判断是对的：

**如果继续先堆上层模块，后面大概率要返工。**

---

## 2. 外部资料调研结论（只保留对工程真正有用的部分）

### 2.1 固定步长与半固定步长

参考：

- Glenn Fiedler, *Fix Your Timestep!*

可直接落地的结论：

- 物理/动态系统不应该接收无限制变化的 `dt`
- 最稳的工程方式不是完全 variable dt，而是：
  - 定义一个可接受的最大子步长 `max_substep_dt`
  - 用多个子步来吃掉更大的 frame/episode advance
- 必须防止“spiral of death”，因此要设置：
  - 最大子步数
  - 或在负载过高时允许仿真降速/截断

对本项目的启发：

- `world.advance(dt_seconds)` 内部不能随便把大 `dt` 一次性塞给所有 layer / brain / capability
- 需要采用：

```text
advance(total_dt)
  -> split into N substeps (<= max_substep_dt)
  -> per substep integrate world / agent / capabilities / events
```

### 2.2 CTRNN 的工程实现

参考：

- Beer 相关 CTRNN 研究
- neat-python CTRNN 文档

可直接落地的结论：

- CTRNN 可视为一组连续时间 ODE
- 关键参数是每个节点的时间常数 `tau`
- 若 `dt / tau` 太大，朴素 Euler 容易不稳定
- 工程上可优先使用 ETD1 / exponential Euler 处理线性衰减项

对本项目的启发：

- protagonist brain 后续若上 CTRNN motor core，不能只“加 recurrent 边”就算连续时间
- 需要明确：
  - 神经状态变量
  - 时间常数范围
  - 每 substep 的积分方法
  - 与动作持续强度接口如何衔接

### 2.3 事件驱动多智能体决策

参考：

- Menda et al., *Deep Reinforcement Learning for Event-Driven Multi-Agent Decision Processes*

可直接落地的结论：

- 多 agent 在异步、持续时间不同的动作场景下，不适合强行塞回统一离散 step
- 固定时间步会掩盖“接近同时发生但顺序不同”的关键差异
- 当 agent 数增多时，单纯缩小 step 宽度的成本很快爆炸

对本项目的启发：

- 不能只靠“把 tick 变小”来假装连续时间
- 必须引入：
  - 真实 `time_seconds`
  - 到期事件队列或最小触发时间机制
  - agent 独立决策时间

### 2.4 SMDP / 连续时间决策

参考：

- Du et al., *Model-based Reinforcement Learning for Semi-Markov Decision Processes with Neural ODEs*

可直接落地的结论：

- 连续时间决策问题更接近 SMDP 而不是标准固定步长 MDP
- 动作持续时间本身就是状态转移的一部分

对本项目的启发：

- protagonist 的很多行为天然是 temporally extended action：
  - 追逐
  - 饮水
  - 砍树
  - 建造
  - 搬运
  - 发信号
- 所以以后 reward / trace / lineage 都要保留动作持续时间，而不是只记“某 tick 按过按钮”

---

## 3. 当前代码里最硬的时间耦合点

下面这些位置说明：问题不是某一个文件，而是**整条时间语义从接口层就写死了**。

### 3.1 类型层

- `src/core/types/Aliases.h`
  - `using Tick = std::uint64_t;`
  - 目前只有离散 step 语义，没有统一的 `SimTimeSeconds`

### 3.2 事件层

- `src/core/events/Event.h`
  - 事件只持有 `Tick tick_`
- `src/core/events/EventPayloads.h`
  - 很多 payload 也带 `Tick`

后果：

- trace、observer、reward、credit 系统天然都更容易绑到 step，而不是绑到真实时间

### 3.3 世界接口层

- `src/core/interfaces/IWorld.h`
  - `currentTick()` 是唯一时钟读取接口
  - `tick(float dt)` 虽然有 `dt`，但名字和语义仍是“step 一下”
- `src/core/world/World2D.h/.cpp`
  - `tick_` 是世界主时钟
  - `tick(float dt)` 末尾 `++tick_`
  - `TickStarted` / `TickEnded` 在 step 边界发出

### 3.4 agent 调度层

- `src/core/agent/Agent.h/.cpp`
  - `decisionInterval()`
  - `nextDecisionTick()`
  - `shouldDecide(Tick current_tick)`
  - `step()` 里用 `world.currentTick()` 触发决策
- `src/core/world/layers/AgentLayer.cpp`
  - `agent->incrementAge()` 也是按 step +1
  - 代谢/能量变化虽然乘 `dt` 的机会还在，但死亡与事件记录仍在 step 边界发生

### 3.5 runtime 主循环

- `src/runtime/headless/HeadlessRuntime.cpp`
  - 外层按 `episode_ticks` 驱动
  - 每轮调用 `world.tick(params.tick_dt)`
  - reward 也跟着 step 循环结算
- `routes/protagonist_ecology/runtime/ProtagonistEcologyRuntime.cpp`
  - protagonist route 同样是 step 驱动训练

### 3.6 配置层

- `tick_dt`
- `episode_ticks`
- `decision_interval_min/max`

这些名字和语义都默认用户在配置“多少 tick”，而不是“多少秒 / 多少持续时间”。

---

## 4. 这次设计不做什么

为了避免把系统一次性炸掉，先明确非目标：

- **不做纯离散事件模拟器替换整个世界**
  - 因为位置、速度、信号场、疾病进展、建造进度、体力变化这些都更适合连续状态积分
- **不先改 interactive / viz**
  - 先稳住 headless 训练与 trace
- **不要求第一阶段就把所有 layer 都完全时间连续化**
  - 先搭时间底座和兼容层
- **不先把所有 brain 都换成 CTRNN**
  - 先让世界时间连续，再让 protagonist brain 吃到这个好处
- **不立刻删掉 `currentTick()`**
  - 旧日志、旧配置、旧测试、旧观察者都依赖它，必须先保留兼容期

---

## 5. 目标架构：Hybrid Continuous-Time Runtime

### 5.1 主时钟模型

引入两个时间维度：

```text
SimTimeSeconds  世界真实时间（主语义）
StepIndex       子步序号（兼容 / 日志 / 可重复性）
```

建议：

- 新增 `using SimTimeSeconds = double;`
- 初期保留 `Tick`，但把它视为 `StepIndex`
- 所有新代码优先使用 `SimTimeSeconds`

### 5.2 世界推进接口

目标语义：

```text
world.advance(total_dt_seconds)
  -> split into substeps
  -> integrate layers/agents/capabilities per substep
  -> flush due events by event_time_seconds
```

兼容期建议：

- `IWorld` 新增：
  - `currentTimeSeconds()`
  - `stepIndex()` 或继续复用 `currentTick()` 作为 step index
  - `advance(double dt_seconds)`
- `tick(float dt)` 暂时保留为 wrapper：
  - 直接调用 `advance(static_cast<double>(dt))`

### 5.3 子步策略

采用**固定上限子步长**：

```text
max_substep_dt = 0.05 ~ 0.20 seconds
```

规则：

- 外层可以要求 `advance(1.0)`
- 世界内部拆成若干 `substep_dt <= max_substep_dt`
- 每个 substep 后：
  - 推进连续状态
  - 处理到期事件
  - 更新 trace / observer 所需摘要

这是为了同时满足：

- 数值稳定性
- 事件顺序清晰
- 训练仍可 batch 高速推进

### 5.4 事件模型

事件不再只有 step，而是至少同时带：

- `time_seconds`
- `step_index`

建议把 `Event` 扩成：

```text
type
payload
time_seconds
step_index
```

这样做的收益：

- replay 可以按真实时间播放
- credit assignment 能按时间差裁剪责任窗口
- signal / hunt / cooperation 的“接近同时”有更稳定定义

### 5.5 agent 调度模型

当前：

```text
if currentTick >= nextDecisionTick -> decide
```

目标：

```text
if currentTimeSeconds >= next_decision_time_seconds -> decide
```

需要迁移为：

- `decision_interval_seconds`
- `next_decision_time_seconds`
- `age_seconds`

并保留：

- `currentTick()` / step index 仅用于兼容日志和子步序号

### 5.6 动作模型

连续时间里，能力不再被理解为“这个 step 有没有按按钮”，而是：

- 一个强度 `intensity`
- 一个持续时间 `dt`
- 一个累计进度或速率

例子：

- move：速度/加速度 × `dt`
- drink：饮水速率 × `dt`
- chop：砍树 progress += `work_rate * intensity * dt`
- build：工地 progress += `build_rate * dt`
- signal：发射强度 × `dt` 或持续时间，信号场按真实时间衰减
- flee / pursuit：连续 steering 而不是每 step 瞬时方向跳变

### 5.7 reward 模型

好消息是：

- `IRewardComponent::evaluate(..., dt)` 这层接口已经有 `dt`

所以 reward 重构不需要推倒重来，只需要把语义定清：

- 生存奖励：按 `survival_rate * dt`
- 协作奖励：按真实共同行为持续时间或事件窗口给分
- 信号奖励：按接收者在事件时间附近的响应质量给分
- 建造/围猎信用：按事件谱系 + 时间差分配

### 5.8 Trace / Replay 模型

trace 需要新增：

- `time_seconds`
- `substep_dt`
- `event_duration_seconds`（有持续时间的动作）
- `decision_time_seconds`
- `action_hold_until_seconds`（若引入宏动作）

不然连续时间做完，回放层又会重新丢失信息。

---

## 6. 为什么不选 pure discrete-event only

纯离散事件模拟器的优点是：

- 异步事件顺序天然清楚
- 稀疏事件时很高效

但对本项目不适合作为唯一底座，原因是：

- agent 位置、速度、朝向天然连续
- 信号场、感染进度、体力、水分、建造进度、树木交互都带连续积累量
- 很多层仍需要频繁空间查询，不适合只靠“事件跳点”表达
- 纯事件驱动会让 terrain / perception / steering / collision 风格逻辑更绕

所以我们选：

```text
连续状态积分 + 到期事件队列
```

而不是：

```text
只有事件，没有连续子步
```

---

## 7. 分阶段实施路线

## Phase CT-0 · 时间类型与兼容层

目标：不改行为，只把“真实时间”引入接口面。

产出：

- `Aliases.h` 新增 `SimTimeSeconds`
- `Event` 新增 `time_seconds`
- `IWorld` 新增 `currentTimeSeconds()`
- `World2D` 维护：
  - `time_seconds_`
  - `step_index_`
- `tick(float dt)` 变成兼容 wrapper
- trace / observer 至少能读到 `time_seconds`

验收：

- 现有 smoke 行为近似不变
- 旧代码仍能编译

## Phase CT-1 · runtime 从 episode_ticks 迁到 episode_duration_seconds

目标：训练循环不再以 tick 数为主语义。

产出：

- config 新增：

```toml
[simulation.time]
mode = "hybrid_continuous"
substep_dt = 0.1
max_substeps_per_advance = 16
episode_duration_seconds = 180.0
```

- `episode_ticks` 保留兼容期
- headless / protagonist runtime 均改按 seconds 驱动

验收：

- 同一 seed 下，legacy 模式与 hybrid 模式都能完整跑完训练
- 不发生 spiral of death

## Phase CT-2 · agent 调度与动作持续时间

目标：让 agent 真正变成异步决策。

产出：

- `decision_interval_seconds`
- `next_decision_time_seconds`
- `age_seconds`
- capability 改成“按强度 × dt 生效”
- 需要阈值触发的行为改成 progress accumulator

优先迁移：

- protagonist move
- eat / drink
- chop tree
- build worksite
- emit signal
- hunt / flee

验收：

- 同类行为在更细 substep 下不出现明显语义跳变
- 行为频率不再因为 step 粒度改变而失真

## Phase CT-3 · 事件异步化与到期事件队列

目标：解决“非边界触发”的关键事件。

产出：

- world 内部到期事件容器
- 支持：
  - delayed event
  - cooldown expiry
  - duration end event
  - scheduled respawn / decay checkpoints

优先迁移：

- 资源 respawn
- 信号衰减关键节点
- campfire / worksite 阶段完成事件
- disease 阶段转移

验收：

- 事件顺序稳定，可被 trace 重建
- 临近时间事件不再受 step 边界强扭曲

## Phase CT-4 · protagonist CTRNN motor core

目标：连续时间脑和连续时间世界真正接上。

产出：

- protagonist brain 新增 CTRNN motor core 或 CTRNN variant
- 节点 `tau` / bias / response / recurrent state
- 优先使用 ETD1 / exponential Euler
- 高层 goal / memory 对 motor core 施加调制

验收：

- 追逐 / 逃跑 / 围猎 / 节律巡逻等行为更平滑
- 在相同任务下，连续 motor core 相比离散 MLP 至少不退化

## Phase CT-5 · 对比模式与实验制度

目标：防止“重构后只是看起来高级，实际更差”。

产出：

- legacy tick runtime 与 hybrid continuous runtime 并存
- compare config
- trace 字段统一
- 指标对比：
  - fitness
  - 行为平滑度
  - 决策频率分布
  - build / hunt / signal 成功率
  - replay 事件顺序稳定性

验收：

- 能跑 A/B 对照
- 能给出是否值得全面切换的证据

---

## 8. 首批需要动到的代码面

### 核心接口

- `src/core/types/Aliases.h`
- `src/core/events/Event.h`
- `src/core/events/EventPayloads.h`
- `src/core/interfaces/IWorld.h`
- `src/core/interfaces/IWorldLayer.h`
- `src/core/interfaces/IAgent.h`

### 世界与 agent

- `src/core/world/World2D.h`
- `src/core/world/World2D.cpp`
- `src/core/agent/Agent.h`
- `src/core/agent/Agent.cpp`
- `src/core/world/layers/AgentLayer.cpp`

### runtime

- `src/runtime/headless/HeadlessRuntime.cpp`
- `routes/protagonist_ecology/runtime/ProtagonistEcologyRuntime.cpp`
- protagonist route 的 config 读取与 smoke config

### 能力 / world layers（第二批）

- movement / eat / drink / chop / build / signal / hunt
- `ResourceLayer`
- `SignalLayer`
- `DiseaseLayer`
- protagonist route 自定义 layer

### 观察 / trace

- `runtime/TraceRecorder.*`
- 所有 observers / CSV / JSONL 输出点

---

## 9. 配置迁移建议

建议新增而不是直接替换，避免一次性炸所有配置：

```toml
[simulation.time]
mode = "legacy_tick"            # legacy_tick | hybrid_continuous
substep_dt = 0.1
max_substeps_per_advance = 16
episode_duration_seconds = 180.0
trace_time_seconds = true

[brain]
decision_interval_seconds_min = 0.1
decision_interval_seconds_max = 2.0

[brain.ctrnn]
enabled = false
integration = "etd1"            # euler | etd1
```

兼容期映射：

- `episode_ticks * tick_dt -> episode_duration_seconds`
- `decision_interval (ticks) -> decision_interval_seconds`

---

## 10. 风险与规避

### 风险 1：重构范围过大，导致现有训练全坏

规避：

- 先加兼容层，不先删旧接口
- 保持 `legacy_tick` 模式可跑

### 风险 2：数值稳定性差

规避：

- 所有连续状态都经 `substep_dt` 上限控制
- CTRNN 优先 ETD1

### 风险 3：trace / replay 失真

规避：

- 从第一阶段起就在 trace 里加入 `time_seconds`
- 任何连续动作都要有 duration 字段或 progress 字段

### 风险 4：配置爆炸

规避：

- 时间参数统一放到 `[simulation.time]`
- 不分散到多个模块里乱起名

### 风险 5：性能退化

规避：

- 子步数上限
- 只对真正需要的系统启用更细积分
- 对比 legacy / hybrid 两种运行模式

---

## 11. 下一步的直接执行顺序

按当前用户优先级，后续不应该再先堆更多上层机制，而应该按下面顺序推进：

1. **先实现 CT-0**：把 `time_seconds` 正式接进 core/event/world 接口
2. **再实现 CT-1**：让 protagonist runtime 能按 `episode_duration_seconds` 跑
3. **然后做 CT-2 的首批能力迁移**：move / drink / eat / signal
4. **确认 trace / replay 没丢时间信息**
5. **最后再继续深挖 Memory V2 / EventLineage / CTRNN protagonist core**

这样顺序最稳，因为：

- Memory V2 需要正确的时间衰减基准
- EventLineage / credit 更依赖事件时间精度
- CTRNN 只有在连续时间世界里才真正值回票价

---

## 12. 最终目标图

```text
legacy tick world
  -> hybrid continuous-time world + substeps
  -> async event timestamps + due-event queue
  -> time-based decisions and action durations
  -> protagonist CTRNN motor core
  -> memory / credit / communication / world model 全部共享统一时间语义
```

到这一步以后，主角路线的“世界理解”才真的有了物理时间基底，而不是只是在连续 2D 平面里跑一个 step-based scheduler。
