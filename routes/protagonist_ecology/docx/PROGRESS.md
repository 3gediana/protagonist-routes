# Protagonist Ecology · 路线进度追踪

> 这份文档只记录 `routes/protagonist_ecology/` 并行路线自己的问题、解决记录和下一步。
>
> 顶层 `PROGRESS.md` 继续记录整个仓库总进度；这里记录主角生态路线的长期研究工程推进。

---

## 顶部状态

| 项 | 值 |
|---|---|
| 最后更新 | 2026-05-20 |
| 当前阶段 | Phase 1 Trace/Resume 完成 + 连续时间世界 CT-0 完成 |
| 当前主线 | 继续实现 CT-1：runtime 从 `episode_ticks` 迁到 `episode_duration_seconds` |
| 最新用户新增问题 | 当前 tick 调度式世界不够像连续时间；已完成 CT-0 接口兼容层与 trace 双时间戳，下一步进入 runtime seconds 化 |

---

## 已完成

### 1. protagonist 并行路线最终愿景

- 已写：`FINAL_STATE_VISION.md`
- 目标：原始聚落沙盒世界，不是随机点生态箱。
- 关键观感：采集、搬运、堆放、回巢、围火、建造、围猎、通信、代际进步。

### 2. protagonist 模块发展计划

- 已写：`MODULE_DEVELOPMENT_PLAN.md`
- 目标：把主角路线当成同仓库内独立项目推进。

### 3. 世界理解与复杂行为长期研究工程方案

- 已写：`WORLD_UNDERSTANDING_RESEARCH_AND_ENGINEERING_PLAN.md`
- 已列出核心瓶颈：
  - 记忆结构弱
  - 缺世界模型
  - reward 偏行为计数
  - 合作信用分配粗
  - 通信没有稳定语义
  - 课程不够开放式
  - 缺 replay / trace
  - 缺完整 checkpoint resume
  - 缺层级目标
  - 缺长期大规模实验制度
 - 已定优先级：
  1. Trace / Resume
  2. Memory V2 专门记忆区
  3. EventLineage / ContributionLedger
  4. 通信语义任务
  5. Quality Diversity / MAP-Elites
  6. POET-like 开放式课程
  7. World Model
  8. Hierarchical Goal System
  9. 长训 + 3D replay panel

---

### 4. Phase 1 · Trace / Resume 基础设施

- 已实现：`runtime/TraceRecorder.h` / `runtime/TraceRecorder.cpp`
- 已打通训练 episode 级 trace 输出：
  - `event_trace.jsonl`
  - `agent_trace.csv`
  - `world_trace.csv`
  - `replay_manifest.json`
- 已实现 protagonist checkpoint manifest：`checkpoint_manifest.json`
- 已打通 `runtime.resume_checkpoint_path`，可从 `checkpoint_genN/genome_*.bin` 恢复 protagonist population
- 已完成 smoke trace 输出验证
- 已完成 smoke resume 验证
- 已知边界：背景物种完整 resume / RNG state / innovation tracker / generation index 仍未完成

### 5. 连续时间世界前置规划

- 已写：`CONTINUOUS_TIME_WORLD_DESIGN.md`
- 已完成一次面向工程落地的外部资料调研，重点覆盖：
  - 固定上限子步长 / semi-fixed timestep
  - CTRNN 连续状态积分
  - event-driven multi-agent decision process
  - SMDP / temporally extended action
- 已明确后续采用 **hybrid continuous-time** 路线：
  - 世界真实时间改为 `seconds`
  - 运行时内部做子步积分
  - 事件同时记录 `time_seconds` + `step_index`
  - 动作、reward、trace 改按持续时间表达

### 6. CT-0 · 时间接口兼容层

- 已新增 `SimTimeSeconds`
- 已为 `Event` 增加 `timeSeconds()`
- 已为 `IWorld` / `World2D` 增加 `currentTimeSeconds()`
- `World2D` 已维护 `time_seconds_`，并在 `tick(dt)` 中持续推进
- `TickStarted` / `TickEnded` 事件已同时带 tick 与秒级时间
- `TraceRecorder` 已输出双时间戳：
  - `event_trace.jsonl` 增加 `time_seconds`
  - `agent_trace.csv` 增加 `time_seconds`
  - `world_trace.csv` 增加 `time_seconds`
- 已完成重新构建与 protagonist smoke 验证

---

## 进行中

### Phase 2 · Memory V2（基础接线已恢复，先冻结）

- 已实现：`SpatialMemoryBank` / `EpisodicMemoryBank` / `SocialMemoryBank` / `GoalMemory`
- 已接入：`MemoryCognitionPerception`
- 已完成当前编译错误修复，`neural-eco-protagonist` 重新可编译
- 已重新跑通 protagonist smoke
- 当前决策：先不继续加深 Memory V2 结构，等连续时间底座稳定后再继续推进

### Phase CT · 连续时间世界底座（当前主线）

当前准备按 `CONTINUOUS_TIME_WORLD_DESIGN.md` 推进：

- CT-0：时间类型与兼容层（已完成）
- CT-1：runtime 从 `episode_ticks` 迁到 `episode_duration_seconds`（当前）
- CT-2：首批 capability 连续持续时间化（move / drink / eat / signal）

---

## 待解决问题清单

### P1 · Trace / Replay / Resume

- [x] episode trace 输出
- [x] protagonist checkpoint resume
- [x] checkpoint manifest 完整记录
- [ ] replay manifest 与 3D 面板数据格式稳定化
- [ ] background species NEAT genome 序列化与 resume
- [ ] RNG state resume
- [ ] innovation tracker resume
- [ ] generation index resume

### P2 · Memory V2

- [ ] WorkingMemoryBank
- [x] SpatialMemoryBank
- [x] EpisodicMemoryBank
- [x] SocialMemoryBank
- [x] GoalMemory
- [x] MemoryCognitionPerception
- [ ] ProtagonistGenomeV2
- [ ] memory ablation eval

### P3 · 因果奖励与合作信用

- [ ] EventLineageTracker
- [ ] ContributionLedger
- [ ] worksite credit 重构
- [ ] hunt credit 重构
- [ ] signal credit 重构
- [ ] 路过者不吃分验证

### P4 · 通信语义

- [ ] scout-worker partial observability task
- [ ] danger warning task
- [ ] build request task
- [ ] signal mutual information metrics
- [ ] signal ablation eval

### P5 · 开放式训练

- [ ] BehaviorDescriptor
- [ ] NoveltyArchive
- [ ] MAP-Elites archive
- [ ] EnvironmentGenome
- [ ] POETCoordinator

### P6 · World Model

- [ ] trace dataset builder
- [ ] observation-action predictor
- [ ] prediction-error curiosity
- [ ] world model latent input

### P7 · 层级目标

- [ ] Manager head
- [ ] Worker head
- [ ] goal persistence
- [ ] long-chain task eval

### P8 · 连续时间世界框架

用户在 2026-05-20 提出：当前世界是 tick-by-tick 调度，agent 每固定 tick 做一次响应，看起来不像现实世界的连续时间过程。

后续需要专门研究并设计：

- [x] 连续时间世界设计文档：`CONTINUOUS_TIME_WORLD_DESIGN.md`
- [ ] `Tick currentTick()` 与 `double currentTimeSeconds()` 的关系
- [ ] `world.tick(dt)` 到 `world.advance(dt_seconds)` 的迁移策略
- [x] CT-0 时间类型与兼容层
- [ ] CT-1 runtime seconds 化
- [ ] CTRNN protagonist brain：连续神经状态 `dx/dt`
- [ ] 连续动作强度：move/drink/eat/chop/build/signal 不再只是离散按钮
- [ ] 事件异步触发：状态积分达到阈值触发，而不是只在 tick 边界触发
- [ ] reward 时间积分：按真实持续时间/强度结算
- [ ] 旧 tick runtime 与 continuous runtime 并存对比

当前决策：连续时间框架已前置为当前主线；优先落 CT-0 / CT-1，再继续深化 Memory V2 / credit / CTRNN。

---

## 下一步

1. 实现 CT-1：让 protagonist runtime 支持 `episode_duration_seconds` + `substep_dt`。
2. 为 headless / protagonist runtime 增加 legacy tick 与 continuous mode 兼容读取。
3. 迁移首批连续动作：move / drink / eat / signal。
4. 继续扩 trace / replay 字段，记录持续时间和决策时间。
5. 在连续时间底座稳定后继续深化 Memory V2 与 credit / CTRNN。

