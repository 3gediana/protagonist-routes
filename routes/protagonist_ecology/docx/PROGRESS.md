# Protagonist Ecology · 路线进度追踪

> 这份文档只记录 `routes/protagonist_ecology/` 并行路线自己的问题、解决记录和下一步。
>
> 顶层 `PROGRESS.md` 继续记录整个仓库总进度；这里记录主角生态路线的长期研究工程推进。

---

## 顶部状态

| 项 | 值 |
|---|---|
| 最后更新 | 2026-05-20 |
| 当前阶段 | Phase 1 Trace/Resume 完整收口（主角+背景+RNG+speciation+generation+1 resume） + 连续时间世界 CT-1 完成 + CT-2A 首批持续动作语义落地 + 记忆系统总设计与 Phase A 接线计划完成 + mixed compute 第一轮工程化（CPU agent decision 并行已接通，protagonist dense GPU backend 已落代码） |
| 当前主线 | Memory Phase A 仍是路线主线，但 compute 承接底座已开始落地：CPU 已从 episode 级推进到 per-world agent decision 级；如继续 GPU 线，下一步先补齐本机完整 CUDA toolkit / nvcc，再验证 protagonist dense inference 真正上卡 |
| 最新用户新增问题 | 用户已明确：记忆系统不能单独做成“大头娃娃”，必须和主行为控制脑同步扩容；优先先把主角记忆系统设计完整、系统化，让 agent 真能记住世界、事件、同伴、目标与因果链，再规划 protagonist 参数上 GPU，同时把 CPU 并行利用率吃满 |

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

### 4. 记忆系统总设计与 Phase A 接线方案

- 已写：`MEMORY_SYSTEM_MASTERPLAN.md`
- 已写：`MEMORY_PHASE_A_EXECUTION_PLAN.md`
- 已完成对当前 protagonist Memory V2 的实况审计：
  - `MemoryBank` 是真闭环，但仍是无结构短期记忆
  - `SpatialMemoryBank` 只有部分写入，且存在 `base / fire / worksite` 混槽
  - `EpisodicMemoryBank` / `SocialMemoryBank` 当前仍偏 write-only
  - `GoalMemory` 类型齐全，但缺真正的 goal selection/update 闭环
  - `MemoryCognitionPerception` 当前只有 12 维，信息量明显不足
- 已形成路线级结论：
  - 不能只把 memory 做大，主行为控制脑也必须同步变大
  - 先把现有 Memory V2 真接活，再做 `ProtagonistBrainV3`
  - 后续 protagonist dense brain 走 mixed GPU，世界逻辑和事件系统继续深挖 CPU 并行
- 已写：`COMPUTE_SCALING_GPU_CPU_PLAN.md`
- 已明确后续算力承接顺序：
  - 先做记忆系统真闭环与主角脑结构化扩容
  - CPU 先从 episode 级并行升级到 agent decision / batch 级
  - GPU 首批只承接 protagonist dense cognition 与后续 world model，不做全项目 GPU 重构
- 已把 Phase A 实施顺序固定为：
  1. `SpatialMemoryBank` 语义拆槽
  2. `MemoryCognitionPerception` 扩维
  3. 最小 `GoalManager` 闭环
  4. `EpisodicMemory` 扩事件与查询
  5. `SocialMemory` 接 cooperation / hostility / communication
  6. `memory ablation eval`

### 4. Phase 1 · Trace / Resume 基础设施

- 已实现：`runtime/TraceRecorder.h` / `runtime/TraceRecorder.cpp`
- 已打通训练 episode 级 trace 输出：
  - `event_trace.jsonl`
  - `agent_trace.csv`
  - `world_trace.csv`
  - `replay_manifest.json`
- 已实现 protagonist checkpoint manifest：`checkpoint_manifest.json`
- 已新增 machine-readable checkpoint state：`checkpoint_state.toml`
- 已打通 `runtime.resume_checkpoint_path`，可从 `checkpoint_genN/` 恢复：
  - protagonist population
  - background species NEAT populations
  - background species innovation tracker
  - protagonist/background evolution RNG state
  - background species speciation state
  - resume generation index
- checkpoint 现在改为 post-reproduction 保存：
  - `checkpoint_state.toml` 记录 `evaluated_generation + resume_generation`
  - resume 直接从 `generation + 1` 继续，不再重复评估已保存那一代
- 已补 checkpoint best genome continuity：`best_genome_overall.bin / best_genome_latest.bin`
- 已完成 smoke trace 输出验证
- 已完成 smoke resume 验证
- 已完成 single-thread full-vs-resume 对照验证：`checkpoint_gen1` 恢复后只跑 `generation=2`，且 `fitness.csv` 与完整直跑结果一致
- 当前已知边界：route 多线程训练本身仍不是逐 run 严格确定性的，所以“精确轨迹对照”以单线程 smoke 为准；这不再是 checkpoint 缺口，而是训练并行确定性问题

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

### 7. mixed compute 第一轮工程化

- 已把 `Agent::step()` 拆成 `decide / publishDecisionEvent / applyCapabilities` 三相，保证重计算与世界写入分离
- 已给 `AgentLayer` 新增持久化 decision executor，支持 route 通过 `compute.agent_decision_threads` 做 per-world agent decision 并行
- 已把 protagonist route training 的外层 `EvaluationPool` 与内层 decision threads 联动，避免 episode 并行与 agent 并行无脑相乘造成 CPU 过度超卖
- 已新增 route 级 compute 配置：
  - `compute.protagonist_dense_gpu_enabled`
  - `compute.agent_decision_threads`
- 已给 `ProtagonistBrain` 接入可选 `ProtagonistDenseGpu` backend，并在 CMake 中加入可选 CUDA `.cu` 编译路径
- 已修正 route runtime 的 protagonist brain probe 输入维度统计，统一复用 `protagonistSensoryDim()`，避免参数量低报
- 已完成 `neural-eco-protagonist` / `neural-eco` Release 重编与 `protagonist_ecology_bootstrap.toml` 非训练 preview 验证
- 当前环境结论：本机 `C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v11.8` / `v12.4` 目录仅有 `extras/`，`where.exe nvcc` 为空；GPU backend 代码已落地，但当前 build 仍未真正激活 CUDA 路径

---

## 进行中

### Phase 2 · Memory V2（基础接线已恢复，先冻结）

- 已实现：`SpatialMemoryBank` / `EpisodicMemoryBank` / `SocialMemoryBank` / `GoalMemory`
- 已接入：`MemoryCognitionPerception`
- 已完成当前编译错误修复，`neural-eco-protagonist` 重新可编译
- 已重新跑通 protagonist smoke
- 当前决策已更新：先不直接跳到完整 `ProtagonistBrainV3`，而是按 `MEMORY_PHASE_A_EXECUTION_PLAN.md` 先把现有 Memory V2 接活，再向更大脑结构推进

### Phase CT · 连续时间世界底座（当前主线）

当前准备按 `CONTINUOUS_TIME_WORLD_DESIGN.md` 推进：

- CT-0：时间类型与兼容层（已完成）
- CT-1：runtime 从 `episode_ticks` 迁到 `episode_duration_seconds`（已完成）
- CT-2：首批 capability 连续持续时间化（进行中）
  - 已完成：`move / drink / signal` 的 `dt` 乘法语义接通
  - 已完成：`EatCapability + ResourceLayer` 改为按 `eat_rate_per_second * dt` 的部分消耗/持续摄食
  - 已完成：`SurvivalStatusLayer / InterventionDirectorLayer` 残留 `per_tick` 口径切到 `per_second`，并保留 legacy 配置兼容读取
  - 待继续：`chop / build / worksite credit / event queue` 的更强连续化表达

---

## 待解决问题清单

### P1 · Trace / Replay / Resume

- [x] episode trace 输出
- [x] protagonist checkpoint resume
- [x] checkpoint manifest 完整记录
- [ ] replay manifest 与 3D 面板数据格式稳定化
- [x] background species NEAT genome 序列化与 resume
- [x] RNG state resume
- [x] innovation tracker resume
- [x] generation index resume
- [x] generation+1 resume semantics
- [x] background speciation state resume

### P2 · Memory V2

- [x] 记忆系统总设计文档：`MEMORY_SYSTEM_MASTERPLAN.md`
- [x] Memory Phase A 接线计划：`MEMORY_PHASE_A_EXECUTION_PLAN.md`
- [ ] WorkingMemoryBank
- [x] SpatialMemoryBank
- [x] EpisodicMemoryBank
- [x] SocialMemoryBank
- [x] GoalMemory
- [x] MemoryCognitionPerception
- [ ] ProtagonistGenomeV2
- [ ] Spatial 语义拆槽与写入补全
- [ ] MemoryCognitionPerception 扩维到 Phase A 版本
- [ ] GoalManager 最小闭环
- [ ] episodic/social query 真接回 brain
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
- [x] CT-1 runtime seconds 化
- [ ] CTRNN protagonist brain：连续神经状态 `dx/dt`
- [~] 连续动作强度：move/drink/eat/chop/build/signal 不再只是离散按钮
- [ ] 事件异步触发：状态积分达到阈值触发，而不是只在 tick 边界触发
- [ ] reward 时间积分：按真实持续时间/强度结算
- [ ] 旧 tick runtime 与 continuous runtime 并存对比

当前决策：连续时间框架已前置为当前主线；优先落 CT-0 / CT-1，再继续深化 Memory V2 / credit / CTRNN。

### P9 · Compute Scaling

- [x] `COMPUTE_SCALING_GPU_CPU_PLAN.md`
- [x] `Agent` decision / apply 两相拆分
- [x] `AgentLayer` per-world agent decision 并行
- [x] protagonist route outer episode pool 与 inner decision threads 联动调度
- [x] `ProtagonistBrain` 可选 dense GPU backend + route 级 CUDA CMake 接线
- [ ] 本机完整 CUDA toolkit / `nvcc` 可用性接通
- [ ] protagonist dense GPU 路径真正编入并完成 smoke profiling
- [ ] CPU/GPU dense forward 对照 profile 与日志

---

## 下一步

1. 按 `MEMORY_PHASE_A_EXECUTION_PLAN.md` 开始第一步：先拆 `SpatialMemoryBank` 语义槽，补全 `tree / danger / fire / worksite / stone / stick` 的分离写入。
2. 随后扩 `MemoryCognitionPerception`，把当前 12 维 memory 输入升级到能承载 Spatial / Episodic / Social / Goal 的 Phase A 版本。
3. 做一个最小 `GoalManager` 闭环，让 goal 不再长期停在默认 `Explore`。
4. CT 线继续推进 `chop / build / worksite` 的持续进度语义，避免记忆系统建立在残余离散动作之上。
5. 以 `COMPUTE_SCALING_GPU_CPU_PLAN.md` 为后续承接文档：记忆 Phase A 稳定后先补 profiling 与 CPU 并行升级，再启动 protagonist mixed GPU。
6. 如果继续当前算力线，先在本机补齐完整 CUDA toolkit（至少 `nvcc + bin/include/lib`），再重新配置 CMake 验证 protagonist dense GPU 路径是否真正上卡。
