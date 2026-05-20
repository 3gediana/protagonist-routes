# Protagonist Ecology · 路线进度追踪

> 这份文档只记录 `routes/protagonist_ecology/` 并行路线自己的问题、解决记录和下一步。
>
> 顶层 `PROGRESS.md` 继续记录整个仓库总进度；这里记录主角生态路线的长期研究工程推进。

---

## 顶部状态

| 项 | 值 |
|---|---|
| 最后更新 | 2026-05-20 |
| 当前阶段 | 世界理解与复杂行为长期方案落地 · Phase 1 Trace/Resume |
| 当前主线 | 先把训练/eval 可追踪、可回放、可恢复，再进入 Memory V2 |
| 最新用户新增问题 | 当前 tick 调度式世界不够像连续时间；后续需要研究连续时间世界 / CTRNN / 连续动作强度框架 |

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

## 进行中

### Phase 1 · Trace / Resume 基础设施

目标：让后续所有复杂行为都能被追踪、回放、验证和恢复。

当前正在实现：

- `runtime/TraceRecorder.h`
- `runtime/TraceRecorder.cpp`
- 训练 episode 级 trace 输出：
  - `event_trace.jsonl`
  - `agent_trace.csv`
  - `world_trace.csv`
  - `replay_manifest.json`
- 主角 checkpoint resume：
  - `runtime.resume_checkpoint_path`
  - 从 `checkpoint_genN/genome_*.bin` 恢复 protagonist population
  - checkpoint manifest 记录恢复状态和限制

注意：当前先做 protagonist population 的真实恢复；背景物种 NEAT population 完整恢复还没做，不能假装已经完整 resume。

---

## 待解决问题清单

### P1 · Trace / Replay / Resume

- [~] episode trace 输出
- [~] protagonist checkpoint resume
- [ ] checkpoint manifest 完整记录
- [ ] replay manifest 与 3D 面板数据格式稳定化
- [ ] background species NEAT genome 序列化与 resume
- [ ] RNG state resume
- [ ] innovation tracker resume
- [ ] generation index resume

### P2 · Memory V2

- [ ] WorkingMemoryBank
- [ ] SpatialMemoryBank
- [ ] EpisodicMemoryBank
- [ ] SocialMemoryBank
- [ ] GoalMemory
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

- [ ] 连续时间世界设计文档：`CONTINUOUS_TIME_WORLD_DESIGN.md`
- [ ] `Tick currentTick()` 与 `double currentTimeSeconds()` 的关系
- [ ] `world.tick(dt)` 到 `world.advance(dt_seconds)` 的迁移策略
- [ ] CTRNN protagonist brain：连续神经状态 `dx/dt`
- [ ] 连续动作强度：move/drink/eat/chop/build/signal 不再只是离散按钮
- [ ] 事件异步触发：状态积分达到阈值触发，而不是只在 tick 边界触发
- [ ] reward 时间积分：按真实持续时间/强度结算
- [ ] 旧 tick runtime 与 continuous runtime 并存对比

当前决策：连续时间框架很重要，但先不插队；等 Trace/Resume、Memory V2 等前置问题解决后再推进。

---

## 下一步

1. 完成 Phase 1 的 trace/resume 编译与 smoke 验证。
2. 用 smoke run 验证 trace 文件真实输出。
3. 用 checkpoint resume 再跑一次短训练，验证主角 population 可恢复。
4. 更新本文件和顶层 `PROGRESS.md`。
5. 进入 Memory V2。
