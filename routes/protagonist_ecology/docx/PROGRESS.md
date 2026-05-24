# Protagonist Ecology · 路线进度追踪

> 这份文档只记录 `routes/protagonist_ecology/` 这个当前项目自己的问题、解决记录和下一步。
>
> 顶层 `PROGRESS.md` 继续记录整个仓库总进度；这里记录主角生态路线的长期研究工程推进。

---

## 顶部状态

| 项 | 值 |
|---|---|
| 最后更新 | 2026-05-25 下午（**L6 R3 stage 3 commit A-N 14 件齐全 + Stone bootstrap commit 1/2/3 全 done + Smoke 验证 (HEALTHY verdict) + 60-gen throw_penalty AB 完成 (验证为负优化) + 60-gen graduate AB 后台跑中**） |
| 当前阶段 | **Layer 1-5 完整 + Layer 6 R2 stage 3 三件齐全 + Layer 6 R3 stage 3 commit A-N 14 件 + Stone bootstrap commit 1/2/3 全 done + 端到端 smoke 验证通过 + 60-gen graduate AB 跑中**。R3 commits A-K (上次)：A API + B hook + toml gate + C 物种延续 gate + D 骨架 + E protagonist 真 mini-tick + F background 真 mini-tick + G protagonist 多 peer mini-world + H borderland_metrics.csv + I summary 脚本 + J weight mutation + K sandbox-aligned perturb/reset rates。**本次新增 commit L/M/N**：L = structural NEAT mutation (add_conn + add_node) BorderlandEvaluator::mutateStructuralPool API + toml gate + 3 gtest；M = borderland_metrics.csv 加 node_count_avg/connection_count_avg 列 + summary script 加 Structural mutation ACTIVE/OFF 诊断 + 2 toml gtest；N = 第一个 per-archetype eval flavor — predator (Apex/Pack/Ambush) mini-world 注入 1 个 LargeHerbivore prey peer 当 hunting target，让 predator 选择真信号。Total tests **218/218 PASS**。Stone bootstrap commits：1 API + 2 runtime hook + 3 跨进程持久化。**Smoke 4-gen verdict: HEALTHY**。**60-gen throw_penalty AB**：负优化（baseline 7/7 vs throw 6/7）。**60-gen graduate AB started 21:05**: 全 pipeline (curriculum + triple_world + eval/mutate + extinction_rescue=2)，没开 commit L/N (binary 20:42 commit K)。Layer 7 R3 step 1 trace 完整，stage 2 viewer 待决策。Layer 8 R1 Lamarckian 落地。 |
| 当前项目优先级 | **L6 R3 stage 3 commit A-N + Stone bootstrap commit 1/2/3 全 done + smoke verified + 60-gen graduate AB 跑中 (~28 commits, 218/218 tests)**。下一步候选：(a) **AB 跑完 (~75 min) 用 harmony_check + borderland_metrics_summary 验收**；(b) **下一轮 structural-mutation AB** — `tmp/layer1/long_run_60gen_2d_structural.toml` 配置已就位 (`8c71f51`)，启用 borderland_mutate_add_conn_rate=0.10 + add_node_rate=0.04 跑 60 代看背景物种结构是否真涨；(c) #3 main world continuous tick (largest remaining module per FINAL_BLUEPRINT)；(d) Layer 7 R3 stage 2 viewer 决策；(e) Scavenger/Omnivore/Rabbit 的 per-archetype eval flavor (commit N 只搞了 predator)。用户路线图"1→2→3→4→5"，**#2 graduation pipeline 现在真闭环 + 结构性 mutation API + 第一个 per-archetype flavor 就位**：sandbox→border→{weight mutate + add_conn/add_node mutate + flavor mini-world}→eval→cull→continuity-gate→main_world。 |
| 最新用户新增问题 | 用户要求 "跑a3实验看看效果 → 重训 → 看着办"。**Phase 1 (commit 0119e78)**：v8 retrain（seed 137、120 gen、emitter 周期 15s、jitter 16m、dense_reward 1.5、训练 12 min、peak best=229k）+ 5×7 ablation sweep。`comm_scout_worker` 强证据 fitness 58.3→9.95 ▼83%、responders 1.5→0 ▼100%；`comm_danger_warning` survived 全 8/8（task pressure 不足）；`comm_build_request` worksite=0（logistics 链断）。**Phase 2 (本 commit)**：修两个有问题的 task 设计——`comm_danger_warning` 改 safe-rallying 语义（60×60 地图、12 predators、emitter 在 nest、dense_reward +0.6）让 baseline 出现 4.5 死亡/episode + fitness ▼88%；`comm_build_request` required_sticks 2→1, stones 1→0 但 worksite_completion 仍 0（visibility=false 让 v8 看不到方向），fitness ▼47%。结论：fitness drop 链路在 4 个通信 task 上多重成立，A3 主线判定多重通过；survived / worksite_completion 这两类 primary metric 是 v8 当前能力上限，需要在 protagonist 训练阶段就接触这两个 task（推到 B 阶段）。完整对照见 `supporting/COMMUNICATION_SEMANTICS_EVIDENCE.md § 3-9`。 |
| 最新本体论决议 | 用户提出"如果出问题要回流训练，活体是不是会集体消失？"——已固化为 `supporting/PRODUCTION_WORLD_LIFECYCLE_SPEC.md`。核心三句话：主世界永不 reset / 训练永远在别处做 / 新血脉只能通过出生或迁徙进入主世界。这份 spec 是 R-002 / R-006 / R-007 / 3D viewer 四条线的共同地基。 |
| Phase 3 收口（2026-05-22 晚） | 三处真实修复 + 一处 hack 占位 + MI 分析。**真实修复**：(1) `worksite.preactivate` 让 eval 任务的 worksite 一开局就激活，`comm_build_request` 的 `worksite_completion_events` 从全 0 拉到 baseline 0.75-1.0，`spatial / goal` ablation 仍降到 0.5（链路解耦正确）；(2) `GoalActionAssistCapability` carrying 时把"最近 peer_signal_heard"作为 active worksite 的 fallback 返航 cue（实测主路径未触发，是 spatial-blind 的合法 fallback，单测正负例都锁了）；(3) **TraceRecorder 加 3 列 env-state 输入** (`nearest_food_dist / predator_nearby_count / worksite_distance`)，配合 `scripts/protagonist_communication_mi.py` 跑出**第一份定量语义涌现证据**——`comm_danger_warning / baseline` 下 channel-0 active 与 `predator_nearby` MI = **0.36 bits**，是 `food_nearby (0.14)` 的 2.6 倍，是 `comm_scout_worker / comm_build_request` 同组的 9-15 倍；信号关闭后 MI 全部归零。 |
| **B 阶段 L1 落地 + v9 retrain（2026-05-22 夜）** | **替换了 Phase 3 的 `nest_predator_damage` hack**。机制底座：(1) `WorksiteLayer::enforcePredatorObstacles()` —— 完工 worksite 形成 collidable 圆，predator 走进去被推回边缘、protagonist 不阻挡（FINAL_STATE_VISION § 3.7 L1）；(2) `BaseLayer.nest_predator_damage_per_second` 字段/配置/tick/Factory/MemoryAblationEval 全清；(3) `AgentSocialPerception` kDim 8→10，新增 `peer_count` (out[8]) + `predator_count` (out[9]) 给 brain 自己决策 fight-back / flee；(4) `ProtagonistHuntCapability` 删除 `target->isPredator() continue`，主角现在能打狼（NN 自己决策）；(5) `TrainingParams.shelter_camp_reward` 0.06 + `shelter_camp_predator_radius` 12.0 —— 主角站在完工 worksite 内 + 12m 内有 predator 才给 reward（防 idle camping）；6 个 EmergentShelterL1 unit tests 全过 (135/135 total)。**v9 retrain 完成**（120 代，seed 137，peak fitness 265k @ gen 46，**▲16% over v8**）；v9 ablation eval 关键数据：comm_danger_warning baseline survived **5.0→6.5 ▲30%**（hack 撤了反而更高），protagonist_attack **0→352.5 / episode**（反击行为涌现），但 completed_worksites=0（v9 学到的是直接打而非"建墙挡狼"，因 eval `worksite.count=0`）；memory ablation 链路保持（no_episodic 6.5→5.25 ▼1.25, no_spatial 6.5→6.0, no_social 6.5→6.0）；Peak fitness 265k vs v8 229k。**还没到的**：v9 没学会 "防微杜渐建墙再用" 的曲线流程；下一步 B 阶段 L2 = 让 comm_danger_warning eval 加 worksite preactivate + 训练 task 设计专门让 predator 浪涌前 protagonist 先建墙。 |
| **L2 G1 batched brain forward + GPU 验证（2026-05-22 夜，commit 69ea2fa）** | **三阶段 brain forward + per-frame batched dense matmul**。改 8 个文件，加 5 个新文件（含 1 个 .cu kernel），共 +1366 行 / -1 行。`IBrain.h` 加 `IBatchableBrain` 可选接口（prepareInputs / denseSlot / finalizeOutputs），`Agent` 加 `decideUntilDense / decideAfterDense`，`AgentLayer` 加 `BatchedDecisionHook` + 4 阶段 tick；`ProtagonistBrain` 实现 batchable 三段（memory bank tick / encoder / DNC step 仍 per-agent，只把 dense matmul 抽出来 batch）；`BatchedDenseGpu.cu` 是新的 batched kernel（blockIdx.y = agent，blockIdx.x = unit，CPU `forwardCpu` 作为 fallback + 测试参考）；`ProtagonistBatchedDenseRunner` 把 N 个 brain 的 slot 收成 agent-major flat buffer 走单次 GPU forward；`ProtagonistEcologyRuntime` 在 3 个 createBootstrapWorld 点装 hook（init / memory ablation / 训练 episode pool）。Config: `compute.protagonist_batched_dense_enabled` (默认 off) + `protagonist_batched_dense_force_cpu` (test 开关)。3 个新数值对比 unit test（CPU batched vs per-agent forward ε<1e-5）+ 138/138 全过。**端到端 GPU 验证通过**（RTX 5060，CUDA 12.8）：单线程 10 代 baseline gen_total~30s brain%=60% → GPU batched gen_total~26s brain%=52%，**brain time 30%↓，总训练 15%↓**；fitness 一致（GPU avg best 62804 vs CPU batched 61576，process-RNG noise 内）。 |
| **L2.6 v12 4-机制 literature-backed 替换 v10/v11 临时调参（2026-05-23 凌晨，commit 422847f + d41a0ff）** | **用户的核心反馈：v10/v11 反复"调参 / 砍 task"是错的方向，让 AI 先查文献再说**。文献调研后定位到 4 个真名症状 + 4 个对应正解：(1) **Lazy Agent Problem** (Sunehag et al.) — protagonist 学会"躺平"；(2) **Negative Transfer / Gradient Conflict** (Yu PCGrad, Cho SMT 2024) — 6 个 task_template 互相干扰；(3) **Multi-task averaging** (Schaul Ray-interference 2019) — 跨任务平均推向保守；(4) **Sparse Long-horizon Reward** — worksite/craft 链条 outcome 信号被 drive penalty 压扁。**4 个 literature-grounded 修复**全部 commit：(1) Step 1 = **Context-conditioned policy** (Rakelly PEARL 2019, Duan RL² 2016) — `TaskContextPerception` 新增 8-onehot 输入，让单一神经网络学 task-specific 行为而非"删 task"；(2) Step 2 = **Decoupled intrinsic motivation** (Cuccu/Schmidhuber 2011 "Intrinsically Motivated NeuroEvolution") — `intrinsic_weight × novelty_reward` 不再被 `outcome_weight` 缩到 0.05× 而是直接乘；(3) Step 3 = **ACCEL regret-based weighting** (Parker-Holder NeurIPS 2022) — 每代 EMA 更新 `(best - avg) / (\|avg\|+1)` 作为 template_regret，高 regret = "agent ALMOST solves it" → 自动 upweight；(4) Step 4 = **SMT Hard-Tasks-First** (Cho ICML 2024) — 早期 N 代用声明 difficulty 加权（线性 taper），强迫网络先碰难任务避免简单任务"掩盖"复杂任务。改 8 文件 +314/-10 行（新增 `TaskContextPerception.{h,cpp}` + `Agent::task_template_idx_` 字段 + Runtime `computeEffectiveTemplateWeights` + `selectTaskTemplateDynamic` + EMA update）。**166/166 tests 全过**。**v12 60-gen 验证 (seed=91，与 v10/v11 同配置)**：alive 1.9→3.8 (+100%)、worksite 0.1→0.5 (+400%)、chops 7→18.6 (+166%)、craft 0.1→0.7 (+600%)、hunts 19→31 (+61%)；best_fitness CV v10 327% → v11 103% → **v12 88%**；**v12 起步 +216 vs v11 起步 -107**（context cue + decoupled novelty 让网络从一开始就有方向）。仅 co_presence 反而 -49%（hard-tasks-first 早期把 mixed_ecology 8-protag 推到前台，是参数调而非机制问题）。仍未追上 v9 的长链条 build/craft 数字 — 那是 60 代不够 + 需要 hierarchical RL extension（不在本 commit 范围）。**结论：4 机制是后续涌现任务的稳定底座**。 |
| **v2 物种差异化 L2.1-L2.4 全部落地（2026-05-22 夜，4 commits）** | 按 SPECIES_DIFFERENTIATION_SPEC.md 的 4 层架构执行，从 v1 "30 个 reward 标签" 完全转向 v2 "机制压出来"，符合用户原话"奖励太直接 → 反涌现"。**L2.1 物质条件分化** (commit 200ea1a)：Agent 加 6 字段 (damage_threshold / threat_damage_window / stealth_state / digest_satiety / scavenge_efficiency / is_in_cave) + 4 方法（accumulateThreatDamage / clearThreatDamageWindow 等）；PredatorHuntCapability 实现 1s 滑动窗口 prey-side 累积器（**单狼 dmg 4 永远过不了 LargeHerbivore threshold 12，必须 4 狼同时打才解锁 kill burst**）+ 豹 stealth 第一击 ×2 伤害 + apex 大 prey kill 后 60s digest_satiety + scavenger 食腐 ×3 / 鲜肉 ×0.33；新增 StealthCapability（3s 静止入隐）+ CaveHideCapability（terrain 查询）；AgentSocialPerception 加 stealth + cave 隐身过滤；8 物种 capability 数值差异化（狼 dmg 10→4、scavenger 加 PredatorHuntCapability output_dim 6→7）。14 个新单测全过。**L2.2 drive state 系统** (commit 3896e90)：Agent 加 5 个 drive (hunger/thirst/fatigue/fear/loneliness) + DriveSetpoints；DriveTickCapability 自动更新 4 个 drive，thirst 由 runtime 从 SurvivalStatusLayer 读；reward = drive_weight × driveSetpointPenalty() + outcome_weight × 旧 v9 reward；3 个 toml gate (`reward.drive_weight`, `reward.outcome_weight`, `reward.drive_thirst_from_hydration`)；setpoints 按 spec § 2.1 物种特化（apex/leopard loneliness=0.6 / pack=0）。9 个新单测全过。Smoke 5 代 protagonist avg fitness -1455 → -46（drives 自动恢复至 setpoint）。**L2.3 资源 niche** (commit 06daaf0)：ResourceLayer 加 ResourceKind 枚举 (Grass/Browse/Fruit/Meat) + PlantSpawnWeights ctor + sampleKind() 抽样；Agent 加 EatEfficiencyByKind (4 floats)；EatCapability 按 kind 缩放 energy_gain；8 物种各自 efficiency 表 (LargeHerbivore grass=1/browse=0.5/fruit=0.3/meat=0; Scavenger meat=1/fruit=0.5; Omnivore fruit=1/grass=0.6/browse=0.6/meat=0.4)；toml `[world.resources.plant_weights]` 暴露三个权重。5 个新单测全过。**L2.4 task template 自适应课程** (commit 7d978b6)：ProtagonistEcologyConfig 加 TaskTemplate 结构 + `[[evolution.task_templates]]` toml 解析；selectTaskTemplate (gen, ep, seed) 决定性加权采样；applyTaskTemplate 在 episode bootstrap 前 mutate route + episode_params；6 个 spec § 4.2 模板 (solo_hunt_easy / solo_hunt_predator / group_vs_pack / proactive_build / wave_attack / mixed_ecology) 完整实例 toml 在 tmp/l2_2_smoke/l2_4_curriculum.toml。166/166 测试全过。**整体小结**：v2 全套 L2.1-L2.4 共 +2740 行 / -32 行。每个阶段都有 toml gate 默认 off，老训练（v9 / a3 / memory ablation）行为完全保留。下一步 L2.5 = 长训验证 + 文档收口。 |

---

## 已完成

### Layer 1-4 (FINAL_BLUEPRINT.md 蓝图，2026-05-23 ~ 2026-05-24)

按 `docx/FINAL_BLUEPRINT.md` 的 Layer 1-8 依赖树，已落地 1-4：

| Layer | 内容 | 数据 | Commit |
|---|---|---|---|
| 1 | 体温 R-001 + 600x600 + Hunger threshold | alive 0.45→0.79 | a0669f6, 7a098a6 |
| 2 | WeatherLayer 6 type Markov + 雨/风/雷 集成 | avg_fit 990→1972 | 202186b |
| 3 | Rabbit 物种 + 9 物种生态闭环 | avg_fit 1972→6685 (gen 25 best 12390) | e3d2b8b |
| 4 | LifecycleStage + DeathCause + lineage_id + PopulationLayer | OldAge/Disease 路径 + 4 in-world births 验证 | 148b6d8 |

**Layer 4 核心设计**：
- Agent 加 `lineage_id_/parent_lineage_a/b_/birth_generation_/archetype_tag_/genome_archive_`
- `PopulationLayer.tick`: phase1 cooldown/OldAge/Elder mortality, phase3 同物种 adult 配对
- `makeChildFromParents`: clone better-fit parent NeatGenome → noise weight → makeNeatSpeciesAgent
- 性能：每次 in-world birth ~100ms（NeatBrain reconstruction），Layer 8 优化

### 1. protagonist 当前项目最终愿景

- 已写：`supporting/FINAL_STATE_VISION.md`
- 目标：原始聚落沙盒世界，不是随机点生态箱。
- 关键观感：采集、搬运、堆放、回巢、围火、建造、围猎、通信、代际进步。

### 2. protagonist 开发执行与任务计划

- 已写：`DEVELOPMENT_EXECUTION_GUIDE.md`
- 已写：`DEVELOPMENT_TASK_PLAN.md`
- 目标：把长蓝图、用户需求和真实代码揉成一条默认执行路线，并把任务与设计分离。
- 当前文档地位：`archive/MODULE_DEVELOPMENT_PLAN.md` 降级为**历史参考**，不再作为默认主导航。

### 3. 世界理解与复杂行为长期研究工程方案

- 已写：`supporting/WORLD_UNDERSTANDING_RESEARCH_AND_ENGINEERING_PLAN.md`
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

- 已写：`supporting/MEMORY_SYSTEM_MASTERPLAN.md`
- 已写：`supporting/MEMORY_PHASE_A_EXECUTION_PLAN.md`
- 已完成对当前 protagonist Memory V2 的实况审计：
  - `MemoryBank` 是真闭环，但仍是无结构短期记忆
  - `SpatialMemoryBank` 已拆成 `water / food / tree / stone / stick / danger / base / fire / worksite` 9 个独立语义槽，runtime 写入已覆盖这些主要对象类型
  - `EpisodicMemoryBank` 已接入结构化 `chopped_tree / built_worksite / deposited_* / signal_emitted / drank_water / saw_predator` 等事件，并通过 `MemoryCognitionPerception` 做基础 readback；但 query 深度仍浅
  - `SocialMemoryBank` 已不只是 sighting：cooperation / communication / hostility 已有真实写入与 readback，但还没有通信语义任务与专门指标
  - `GoalMemory` 已由 runtime 里的 rule-based 选择逻辑持续刷新，不再长期停在默认 `Explore`；但还不是 Manager/Worker 层级系统
  - `MemoryCognitionPerception` 已扩到 44 维，并纳入 DNC 摘要
- 已形成路线级结论：
  - 不能只把 memory 做大，主行为控制脑也必须同步变大
  - 先把现有 Memory V2 真接活，再做 `ProtagonistBrainV3`
  - 后续 protagonist dense brain 走 mixed GPU，世界逻辑和事件系统继续深挖 CPU 并行
- 已写：`supporting/COMPUTE_SCALING_GPU_CPU_PLAN.md`
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

- 已写：`supporting/CONTINUOUS_TIME_WORLD_DESIGN.md`
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

### 7. Stage 0 · CT-2B/CT-3 结构化动作事件（2026-05-21）

新对话规划下的 Stage 0 收口。目的：让 chop/build 等持续动作以**结构化 started/completed 事件对**的形式被记录，Memory Phase A 才有真正可消费的因果素材。

**已落地**：

- core EventType 新增 6 个 protagonist route 事件：`ChopStarted` / `ChopCompleted` / `ChopAbandoned` / `BuildStarted` / `BuildCompleted` / `WorksiteMaterialDeposited`，含 EventType.h + EventType.cpp + EventPayloads.h
- `TreeLayer::chopNearest` 改返回 `ChopHitResult{tree_id, tree_pos, remaining_integrity, felled, wood_yield}`，让 capability 不必二次查询就能获得归因
- `ChopTreeCapability` 改成有状态的 per-agent session 状态机：start / continue-same-tree / target-switch (abandon-old + start-new) / abandon (output drop / out-of-range / now-carrying) / complete；每个 agent 自己一份实例，串行 apply 无需锁
- `WorksiteLayer` 增加三个 CT 字段：`build_started_at_seconds` / `per_agent_contribution_seconds` (map) / `active_builders` (set)；`progressConstruction` 现在做：首次进入发 BuildStarted、每 dt 累积 per-agent 贡献时间、完成时按贡献时长降序生成 contributors 列表发 BuildCompleted
- `absorbCarriedMaterials` 每次实际入库发 `WorksiteMaterialDeposited`，带 depositor + material_type
- `TraceRecorder` 已为全部 6 个事件接好 payload 序列化，`event_trace.jsonl` 可看到完整字段（如 `{"chopper":8,"tree_id":5,"started_at_seconds":2.5,"duration_seconds":2.5,"wood_yield":3}`）
- Reward 时间积分口径审计完成：survival/proximity/social 等已是 `* dt_seconds`，combat/death/signal/joint_hunt 是 per-event one-shot，food 通过连续 EatCapability 的 plant/meatFoodThisTick 自然按 dt 累积。**没有发现任何 per_tick artifact**

**已验证**：

- Release 构建通过（`neural-eco-protagonist` + `test_core`）
- 73/73 单测通过
- protagonist smoke 跑通，event_trace.jsonl 显示 ChopStarted=8 / ChopCompleted=6 / ChopAbandoned=2 完整闭合，payload 字段齐全

**意义**：

- 之前的 chop/build 只能体现为离散计数（"chop_attempts/successes 加一"）。现在每一个完整 chop 都是一个带起止时间 + 时长 + 树木归因的结构化事件，Memory Phase A 的 EpisodicMemoryBank 可以直接 push 这种事件
- Worksite 的 `per_agent_contribution_seconds` 是 P3 ContributionLedger 的现成数据源——以前是"在场就分"，现在是"按陪伴时长分"，路过者天然吃不到分
- BuildCompleted 已经按贡献时长降序排好 contributors 列表，下游消费 0 成本

**已主动延后**：CT-Step 4（EventQueue / DueAction）当前没有真实消费者，决定推到 Stage 1（Memory Phase A 的 goal expiry / episodic decay 会真正需要它），避免过度工程

### 8. Stage 1 · Memory Phase A 收口（2026-05-21）

按 `supporting/MEMORY_PHASE_A_EXECUTION_PLAN.md` 完成 Phase A 的 6 个验收项。在大重构 commit `c7f1665` 中很多 Phase A 部件其实已经接好了，本会话主要做以下三件事：

**已落地（本会话补完）**：

- 把 Stage 0 的 6 个结构化事件接进 EpisodicMemoryBank：在 runtime 评估循环新增 3 个 tick 收集器（`tick_chop_completed` / `tick_build_completed` / `tick_deposits`），通过 EventBus 订阅在每个 substep 收集，然后在已有的 per-agent memory 写入循环里转写成 `chopped_tree / built_worksite / deposited_stick / deposited_stone / signal_emitted` 这 5 类 episodic 事件，含 salience 模型（chop 按时长，build 按贡献排名）
- 增加 TraceRecorder 对全部 6 个新事件的 payload 序列化，使 `event_trace.jsonl` 字段完整（之前只能看到 `{"type":"ChopCompleted"}` 没有 chopper / duration / wood_yield）
- 新增 memory ablation smoke harness：`scripts/protagonist_memory_ablation.py` 一次跑完 baseline + 4 个 ablation（spatial/episodic/social/goal），自动生成对照表并保留各自 trace 目录，验证 4 个 `population.protagonist.*_enabled` 开关都可读、可执行、可观测
- 写 `supporting/MEMORY_ABLATION_SMOKE_RESULT.md` 诚实记录首轮结果：4 开关都让 fitness 和行为指标显著移动（基础设施验收 PASS），但 ablation 反而 fitness 更高的反直觉现象需要长训才能反转，**不能当成"记忆有害"的科学结论**

**Phase A 验收清单（全 PASS）**：

| 验收项 | 状态 | 证据 |
|---|---|---|
| Spatial / Episodic / Social / Goal 都进入真实决策闭环 | ✅ | `MemoryCognitionPerception` 44 维全部从 4 个 banks 读出 |
| `MemoryCognitionPerception` 扩到能承载 Memory V2 | ✅ | 9 个 spatial 槽 16 维 + episodic 6 维 + social 8 维 + goal 4 维 + DNC 8 维 |
| goal 不再是默认 `Explore` 常驻 | ✅ | rule-based GoalManager: Flee/FindWater/BuildWorksite/RestNearFire/FindFood/Explore |
| social 分数不再长期全 0 | ✅ | smoke trace: social_best_coop 1595/9000 行非零；comm 1597 行；hostility 162 行 |
| spatial 通道不再混槽 | ✅ | 9 个独立 cell 字段（water/food/tree/stone/stick/danger/base/fire/worksite）独立写入与查询 |
| 一组最小 memory ablation 对照结果 | ✅ | 5 变体 smoke 表 + smoke 结果文档 |
| smoke / train continues to work | ✅ | 73/73 tests + protagonist smoke 全程绿 |

**已主动延后到 Phase B**（不是 Phase A 必需）：

- DNC-forcing micro-task（设计一个只有用 DNC 检索才能拿 reward 的环境）
- `WorkingMemoryBank` 多时间尺度化
- `ProtagonistGenomeV2` 编码更大脑结构

### 9. mixed compute 第一轮工程化

- 已把 `Agent::step()` 拆成 `decide / publishDecisionEvent / applyCapabilities` 三相，保证重计算与世界写入分离
- 已给 `AgentLayer` 新增持久化 decision executor，支持 route 通过 `compute.agent_decision_threads` 做 per-world agent decision 并行
- 已把 protagonist route training 的外层 `EvaluationPool` 与内层 decision threads 联动，避免 episode 并行与 agent 并行无脑相乘造成 CPU 过度超卖
- 已新增 route 级 compute 配置：
  - `compute.protagonist_dense_gpu_enabled`
  - `compute.agent_decision_threads`
- 已给 `ProtagonistBrain` 接入可选 `ProtagonistDenseGpu` backend，并在 CMake 中加入可选 CUDA `.cu` 编译路径
- 已修正 route runtime 的 protagonist brain probe 输入维度统计，统一复用 `protagonistSensoryDim()`，避免参数量低报
- 已完成 `neural-eco-protagonist` / `neural-eco` Release 重编与 `protagonist_ecology_bootstrap.toml` 非训练 preview 验证
- 当前环境结论（已按最新基准重标定）：
  - 普通 `build/` 仍是 CPU fallback，`CMAKE_CUDA_COMPILER=NOTFOUND`
  - `build_cuda/` 由 `D:\NVIDIA\CUDA\v12.8\bin\nvcc.exe` 构建，可执行文件在补上 CUDA bin 到 PATH 后可运行
  - 但 camp-building 1 generation 基准里，当前 CUDA build **没有带来训练总耗时优势**，所以现阶段只能说“CUDA build 路径可运行”，不能说“GPU 已经显著加速训练”

### 10. fixed-task memory ablation runtime 已修好并完成 full run（2026-05-21）

- 已确认当前 fixed-task memory ablation 不再只是“入口可运行”，而是已经完成：
  - 真实 genome/checkpoint 加载
  - 1-episode traceprobe
  - 4-episode fixed run
- 第一次 traceprobe 发现：
  - `water_return` 已修好
  - `worksite_return` 仍为 0
- 根因不是 `WorksiteLayer` 本体坏掉，而是：
  - `GoalActionAssistCapability` 帮 agent 直接 pickup stick/stone
  - 旧 `DropCapability` 还能在同 tick 把刚拿到的材料立刻丢掉
  - `ThrowCarriedObjectCapability` 也会打断 stone logistics
  - `WorksiteReturn` 任务还允许 nursery logistics 干扰材料流向
- 已落地修法：
  - `GoalActionAssistCapability` 在 `GatherStick / GatherStone / BuildWorksite` 下携带材料时优先回 remembered worksite，没有 worksite 再退回 remembered base
  - 新增 `GoalAwareDropCapability`，在 logistics/build goal 下不让普通 drop 路径把 stick/stone 立刻放掉
  - `ThrowCarriedObjectCapability` 在 logistics/build goal 下抑制 stone throw
  - `MemoryAblationEval.cpp` 为 `WorksiteReturn` 关闭 `nursery.enabled`
- 已完成 focused 回归测试、重新编译、traceprobe 与 full run 验证
- 4-episode full run 关键结果（`data/runs/protagonist_memory_ablation_fixed/20260521-212657`）：
  - `water_return`：baseline / `no_episodic` / `no_social` = `162`，`no_spatial` = `67.5`，`no_goal` = `0`
  - `worksite_return`：baseline / `no_episodic` / `no_social` = `0.5`，`no_spatial` / `no_goal` = `0`
  - `signal_response`：baseline = `15.5`，`no_goal` = `0`，其余变体仍有 `12.25~13`
  - `danger_avoidance`：`no_episodic` 明显变差（`survived 8 -> 6.25`）
- 当前判断：
  - recent worksite bug 已真实修好
  - `no_goal` 仍是最稳定的强 ablation
  - `no_spatial` 在 `worksite_return` 上很干净，但在 `water_return` / `signal_response` 上还不够强
  - 后续重点应转向解释与任务 tightening，而不是再证明入口能不能跑

### 11. 指标说明单页已补齐（2026-05-21）

- 已逐个核对当前真实存在的指标输出与口径：
  - `fitness.csv`
  - `evolution.csv`
  - `protagonist_behaviors.csv`
  - `memory_ablation_results.csv`
  - `module_metrics.csv`
  - `advanced_stats.csv`
  - `emergence_audit.csv`
  - `comprehensive/*.csv`
- 已新增可直接双击打开的单页说明：`supporting/METRICS_FIELD_GUIDE.html`
- 页面内容已按模块整理：
  - 训练主指标
  - 主角行为指标
  - 记忆 / DNC / Ablation
  - 生态模块指标
  - 研究型统计
  - 关键参数与实战判读模板
- 这次特别澄清了几类最容易误读的字段：
  - `signal_emits` vs `signal_emit_events_avg`
  - `completed_worksites` vs `worksite_completion_events`
  - DNC 摘要字段“只能辅助解释，不等于记忆已有效帮助任务”

### 12. memory ablation 任务 tightening + peer_signal_nearby bug（2026-05-21）

commit `02d5a08`，针对前一轮 full run 留下的两个分离弱：

- **water_return**：旧 `spawn_radius=26` 在 `kWaterSenseRadius=36` 内永远可见 + `hydration_decay` 单位是 absolute/s 不是 ratio/s。改 spawn=42、initial=0.65、decay=0.6 后 baseline drank=207.5 / no_spatial=90.25（~56% drop, 7/8 干涸）。SpatialMemory 因果证据干净。
- **signal_response**：改 emit 间隔 6s→30s、emitter 拉近到 nest 边缘、response 半径 10→8 / window 30s→15s。同时修了一个真 bug — `peer_signal_nearby` 只看本 tick emit，30s 间隔下 GoalManager 几乎永远错过，listener 不会切 RespondSignal goal。改成扫 `last_signal_emit` + window 过滤后 events 从全 0 → baseline 1.5 / no_spatial 1.75。fitness 维度 baseline 7.95 / no_spatial 4.37（~55% 残余）。
- **遗留**：signal_response events 维度仍弱，**根因不是任务，是 fixed genome 训练时没有 signal_response 奖励 — policy OOD**。water spatial 分离干净是因为 worksite_return 训练里用了 spatial，能力部分泛化；signal 能力 task-specific，无法泛化。

下一步：要么重训含 signal_response 奖励的 genome，要么在 ablation summary 把 signal_response 标记为 "OOD evaluation, fitness-level separation only"。

### 13. Production World Lifecycle Spec 落地（2026-05-22）

用户提出关键的本体论质疑：如果主世界要支持 R-007 自动晋升闭环，那"出问题"是不是会导致活体集体消失、训练完再集体回来？这与 R-002 持续运行 + R-006 个体级生命周期天然冲突。

**结论已固化为 spec**：`supporting/PRODUCTION_WORLD_LIFECYCLE_SPEC.md`

核心立场：

- **主世界永不 reset**
- **训练永远在别处做**
- **新血脉只能通过出生或迁徙进入主世界**

物理图景：

- 三层世界并存：训练沙盒（用户不可见、episode 化）/ 边境试运行世界（次要可见、连续运行）/ 主生产世界（主要可见、永不 reset）
- 个体进入主世界只有两条合法路径：父母在主世界繁殖（主路径） + 从边境迁徙过来（辅路径，单次 1-3 个、地图边缘出现）
- 个体离开主世界只有四条合法路径：老死 / 病死 / 战死 / 长期生理压力死
- incident → challenge → 训练 → 边境 → 迁徙 → 主世界路径 A 复用，构成完整闭环
- 任何"集体消失 / 集体出现 / 主世界 reset"都是被工程上禁止的非法路径

工程意义：

- 这份 spec 是 R-002 / R-006 / R-007 / 3D viewer 四条需求的共同地基
- 任何与本 spec 不变量冲突的设计不允许进入实现阶段
- 当前代码距离 spec 目标态很远，主世界 / 边境世界 / 迁徙入口 / incident 录制 / 谱系持久化都还没开始
- 已在 spec § 11 写出后续 W1~W6 工作顺序，对接 `DEVELOPMENT_EXECUTION_GUIDE.md` Stage C / D / E

**灭绝边界补丁（同日扩展）**：用户追问"种群没来得及繁衍就灭绝怎么办"，已在 spec 新增 § 4.4 灭绝场景处理 + § 7 新增 7 条灭绝相关不变量（共 17 条）。覆盖：

- 4.4.1 迁徙者灭绝（最常见，设计上接受，可重试，连续 N 次失败升级 incident）
- 4.4.2 主世界某 lineage 灭绝（真实事件，可从 archive fork 重训，但重训版是带 `ancestral_archive_id` 的新 lineage，必须走 frontier → migration 重新引入）
- 4.4.3 整个物种灭绝（关键 incident，主世界不立即填空，留一个 day-night + 季节窗口的生态反应时间）
- 4.4.4 主角物种全灭绝（戏剧高光，主世界仍然连续运行，不黑屏，不"训练中请等待"，archive 里的旧 protagonist 通过 frontier 重新孵化）

补丁后的核心立场：**灭绝是合法的世界叙事，不是失败状态；archive 永久保留但不能直接复活；任何灭绝都不会让主世界 reset。**

### 14. R-008 自长机制精确化 spec 落地（2026-05-22）

用户追问"如果不给喝水 capability，agent 能不能进化出喝水？文献支持到哪？" 经查 Baker 2019 hide-and-seek / Wang 2023 Voyager / Sims 1994 / DIAYN / Option-Critic / Lazaridou-Foerster emergent communication 等核心文献，结论：**任何文献都没有证明 agent 能在没有底层 primitive 情况下凭空创造新 capability**。

**结论已固化为 spec**：`supporting/SELF_GROW_CAPABILITY_LIMITS_SPEC.md`

7 层定义：

- L0 参数微调 / L1 结构生长 / L2 策略组合：默认可达，不算"自长"
- L3 策略涌现（hide-and-seek 6 阶段那种）：可达，最低门槛"真自长"
- L4 技能分化（DIAYN / Option-Critic）：需 brain 架构留 modular 空间
- L5 通信协议涌现（emergent communication）：需通信任务立起来
- L6 组合性能力（Voyager / Sims）：需 capability 改原子化
- L7 凭空新 capability / 新物理：**物理不可达**，必须人写

R-008 期待逐项落层：

- "自己长出新技能" → L3 + L4，可达
- "行为分化 / 局部模块化" → L4，可达
- "通信 / 协作模式" → L5，可达
- 隐含期待"自己长出新 capability" → L7，**不可达**

硬规则：

- 任何"自长 X"表述必须指明 Layer 编号
- L7 必须人写，不在任何文档承诺
- 任何机制证据链必须包含 `target_layer: L0~L7` 字段

三条路线：

- A · capability 通用化（选 1-2 个改原子，L6 实验候选）✅ 推荐
- B · LLM-as-author（Voyager 风格）❌ 违背神经进化主线
- C · 修正 R-008 期待写进 spec ✅ 必须做

---

## 进行中

### Phase 2 · Memory V2（基础接线已恢复，先冻结）

- 已实现：`SpatialMemoryBank` / `EpisodicMemoryBank` / `SocialMemoryBank` / `GoalMemory`
- 已接入：`MemoryCognitionPerception`
- 已完成当前编译错误修复，`neural-eco-protagonist` 重新可编译
- 已重新跑通 protagonist smoke
- 当前决策已更新：先不直接跳到完整 `ProtagonistBrainV3`，而是按 `supporting/MEMORY_PHASE_A_EXECUTION_PLAN.md` 先把现有 Memory V2 接活，再向更大脑结构推进

### Phase CT · 连续时间世界底座（当前项目重点）

当前准备按 `supporting/CONTINUOUS_TIME_WORLD_DESIGN.md` 推进：

- CT-0：时间类型与兼容层（已完成）
- CT-1：runtime 从 `episode_ticks` 迁到 `episode_duration_seconds`（已完成）
- CT-2：首批 capability 连续持续时间化（进行中）
  - 已完成：`move / drink / signal` 的 `dt` 乘法语义接通
  - 已完成：`EatCapability + ResourceLayer` 改为按 `eat_rate_per_second * dt` 的部分消耗/持续摄食
  - 已完成：`SurvivalStatusLayer / InterventionDirectorLayer` 残留 `per_tick` 口径切到 `per_second`，并保留 legacy 配置兼容读取
  - 待继续：`chop / build / worksite credit / event queue` 的更强连续化表达

### Phase B · DNC-forcing micro-task（已完成最小验证，2026-05-21）

- 已新增 route-local `MemoryTrialLayer`：episode 开头短暂给出 one-hot cue，随后把 protagonist 送到 choice hub，必须记住 cue 才能在 left/right 两个对称 target 中选对并拿到 fitness bonus
- 已新增 `MemoryTrialPerception`：只在 cue phase 暴露 2 维提示，不复用 `SignalLayer`，避免 agent 自己发信号把任务污染
- 已新增训练配置：`configs/protagonist_ecology_train_dnc_cue_choice.toml`
- 已新增最小单测：`tests/route_protagonist/test_memory_trial_layer.cpp`
- 已补 route config 回归测试：`tests/route_protagonist/test_protagonist_ecology_config.cpp`
- 当前刻意保持：`memory_cognition_enabled = false`、background = 0、其它生态层基本关闭，先把任务收敛成“DNC 记住瞬时 cue → 延迟选边”的纯记忆基准
- 已完成最小验证：Release 下 `test_core` / `neural-eco-protagonist` 重编通过；`MemoryTrialLayer.*` 3 个单测通过；短 smoke（2 代 × 1 episode）可运行并产出 `fitness.csv / evolution.csv`
- 验证中顺手修复一处真实接线 bug：`ProtagonistEcologyConfig::fromConfig()` 之前漏读 `world.signal.*`，导致 `world.signal.enabled = false` 不生效、micro-task 配置会在 runtime 里触发 `action_dim` mismatch；现已补 parser 并加回归测试
- 当前结果：短 smoke 的 `best_fitness = 24`、`min_fitness = -6`，与 `MemoryTrialLayer` 的 success/failure reward 对上，说明任务 reward 路径已真正接通；但这还不是“DNC 已学会任务”的结论
- 尚未做：trace 指标专项解读，以及更长训练/ablation 去确认 DNC 是否真的在被用

### Phase B+ · `ProtagonistBrainV3` 最小壳层（进行中，2026-05-21）

当前约束已经明确：`total_input_dim_`、现有 6 组参数 buffer、`ProtagonistDenseGpu` 的矩阵布局、`ProtagonistEvolution` 的显式 crossover/mutate 接线，暂时都属于 **load-bearing compatibility surface**，所以这一步先做**结构拆壳**，不做 `GenomeV2/V3` 升级。

**已落地**：

- 新增 `brain/ProtagonistBrainV3Components.h` / `brain/ProtagonistBrainV3Components.cpp`
- `ProtagonistBrain` 现在开始持有并经由这些组件组织前向：
  - `ObservationEncoder`
  - `MemoryQueryEncoder`
  - `BodyStateMemory`
  - `WorkingMemoryBank`
  - `BehaviorFusionTrunk`
  - `MotorCore`
  - `ActionHeads`
  - `InterfaceHead`
- `forward()` / `reset()` 已切到新壳层组件，但仍复用：
  - 原 `total_input_dim_ = external_input + dnc_read_vectors`
  - 原 6 组权重 / bias buffer
  - 原 DNC `decodeInterface()` 路径
  - 原 optional dense GPU fallback 路径
- 已开始把壳层记忆折叠回现有 learnable path：
  - `BodyStateMemory.summary()` 现在会加到 observation slice
  - `WorkingMemoryBank.summary()` 现在会加到 memory-query slice
  - 因此两者已经能通过原 `in_hidden_weights_` 影响 controller 前向，而不需要先升级 genome buffer
- 已继续把 working memory 往下游 action path 再接一层：
  - `WorkingMemoryBank.summary()` 现在还会调制 `motor_target`
  - 因此 working memory 不只影响 trunk input，也会继续影响 `MotorCore -> ActionHeads / InterfaceHead`
- `routes/protagonist_ecology/CMakeLists.txt` 已把 `ProtagonistBrainV3Components.cpp` 接入 route 库
- 已补最小壳层测试：`tests/route_protagonist/test_protagonist_brain_v3_shell.cpp`
  - 只验证 `forward()` 能产出动作，并且 `WorkingMemoryBank / BodyStateMemory` 壳层对象存在
  - 不对学习效果或行为结论做任何判断
- 已补 learnable path 护栏测试：
  - `ProtagonistBrain.FeedsBodyAndWorkingMemoryIntoLearnablePath`
  - 用同 seed 的两份 brain 对照，只预热 body/working memory，要求动作输出发生变化
  - `ProtagonistBrain.FeedsWorkingMemoryIntoLearnablePath`
  - 只预热 working memory，要求动作输出也发生变化

**当前状态**：

- 这批 BrainV3 改动已完成**最小 build/test 验证**：
  - Release 下 `test_core` / `neural-eco-protagonist` 重编通过
  - `ProtagonistBrain.ExposesMinimalBrainV3ShellState` 通过
  - `ProtagonistBrain.FeedsBodyAndWorkingMemoryIntoLearnablePath` 通过
  - `ProtagonistBrain.FeedsWorkingMemoryIntoLearnablePath` 通过
  - `MemoryTrialLayer.*` 与 `ProtagonistEcologyConfig.ParsesSignalSection` 关键回归保持通过
- 此外已补一轮 **DNC cue-choice 短 smoke**：
  - 使用裁短后的 `protagonist_ecology_train_dnc_cue_choice_brainv3_smoke.toml`
  - 从 repo 根启动，产物落在 `data/runs/protagonist_dnc_cue_choice_brainv3_smoke/20260521-110925`
  - `fitness.csv` 继续得到 `best_fitness = 24`、`min_fitness = -6`
  - `protagonist_behaviors.csv` 中 DNC 指标继续非零
- 在接入 learnable path 后又补跑一轮同配置短 smoke：
  - 产物：`data/runs/protagonist_dnc_cue_choice_brainv3_smoke/20260521-111856`
  - `Gen 1` 再次出现 `best_fitness = 24`
  - `fitness.csv` 这次为 `gen0=-6, gen1 best=24`，说明短 smoke 仍通，但 route 当前并非严格逐 run 确定性
- 在继续把 working memory 接到 `motor_target` 之后，又重跑一轮同配置短 smoke：
  - 产物：`data/runs/protagonist_dnc_cue_choice_brainv3_smoke/20260521-113010`
  - 结果仍是 `gen0=-6, gen1 best=24`
  - 说明更深一层的兼容接线没有把任务链跑崩
- 现阶段 `BodyStateMemory / WorkingMemoryBank` 虽已进入现有 learnable path，但仍不是独立参数化的新主路径
- 当前可以认为“结构拆壳成立”，但还**不能**认为 `BrainV3` 已完成更深层行为升级

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

- [x] 记忆系统总设计文档：`supporting/MEMORY_SYSTEM_MASTERPLAN.md`
- [x] Memory Phase A 接线计划：`supporting/MEMORY_PHASE_A_EXECUTION_PLAN.md`
- [~] WorkingMemoryBank（BrainV3 壳层与最小验证已完成，且已通过输入折叠方式进入现有 learnable path；仍未独立参数化）
- [x] SpatialMemoryBank
- [x] EpisodicMemoryBank
- [x] SocialMemoryBank
- [x] GoalMemory
- [x] MemoryCognitionPerception
- [ ] ProtagonistGenomeV2
- [x] Spatial 语义拆槽与写入补全
- [x] MemoryCognitionPerception 扩维到 Phase A 版本
- [x] GoalMemory 最小闭环（runtime rule-based goal refresh）
- [~] episodic/social query 真接回 brain（已通过 `MemoryCognitionPerception` 做基础 readback，但 query 深度和专项任务仍弱）
- [~] memory ablation eval（smoke harness、fixed-task C++ runtime、1-episode traceprobe 与 4-episode full run 都已完成，并确认 recent `worksite_return` fix 生效；当前剩余的是更强解释、trace 证据与任务分离收口）

### P3 · 因果奖励与合作信用

- [ ] EventLineageTracker
- [~] ContributionLedger（worksite/hunt 两条已落地，signal credit 与 lineage 仍缺）
- [x] worksite credit 重构
- [x] hunt credit 重构
- [ ] signal credit 重构
- [x] 路过者不吃分验证

### P4 · 通信语义

- [ ] scout-worker partial observability task
- [ ] danger warning task
- [ ] build request task
- [ ] signal mutual information metrics
- [ ] signal ablation eval

### P5 · 开放式训练

- [~] BehaviorDescriptor（现以 `BehaviorCharacterization` 形式存在）
- [x] NoveltyArchive
- [~] MAP-Elites archive（离线 archive / report 已有，尚未接入 reproduction）
- [ ] EnvironmentGenome
- [ ] POETCoordinator

### P6 · World Model

- [~] trace dataset builder
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

- [x] 连续时间世界设计文档：`supporting/CONTINUOUS_TIME_WORLD_DESIGN.md`
- [ ] `Tick currentTick()` 与 `double currentTimeSeconds()` 的关系
- [ ] `world.tick(dt)` 到 `world.advance(dt_seconds)` 的迁移策略
- [x] CT-0 时间类型与兼容层
- [x] CT-1 runtime seconds 化
- [ ] CTRNN protagonist brain：连续神经状态 `dx/dt`
- [~] 连续动作强度：move/drink/eat/chop/build/signal 不再只是离散按钮
- [ ] 事件异步触发：状态积分达到阈值触发，而不是只在 tick 边界触发
- [ ] reward 时间积分：按真实持续时间/强度结算
- [ ] 旧 tick runtime 与 continuous runtime 并存对比

当前决策：连续时间框架已前置为当前项目重点；优先落 CT-0 / CT-1，再继续深化 Memory V2 / credit / CTRNN。

### P9 · Compute Scaling

- [x] `supporting/COMPUTE_SCALING_GPU_CPU_PLAN.md`
- [x] `Agent` decision / apply 两相拆分
- [x] `AgentLayer` per-world agent decision 并行
- [x] protagonist route outer episode pool 与 inner decision threads 联动调度
- [x] `ProtagonistBrain` 可选 dense GPU backend + route 级 CUDA CMake 接线
- [x] 本机完整 CUDA toolkit / `nvcc` 可用性接通（portable 路径在 `D:\NVIDIA\CUDA\v12.8\`）
- [~] protagonist dense GPU 路径真正编入并完成 smoke profiling（CUDA build 可运行，但 camp-building benchmark 未显示训练侧收益）
- [~] CPU/GPU dense forward 对照 profile 与日志（已有 1-generation wall-clock 基准；仍缺更细粒度 dense-forward profiling）

---

## 下一步

1. `signal_response` 收口下一轮（如重启）：按 audit § 7.5 列出的方向之一推进——大幅放大 listener reward 量级（`signal_response_dense_reward 1→5` + `signal_response_reward 1.5→8`）、或在 SignalResponse ablation task 内把 `food_weight 4→0.5` 让 food 不再霸占 fitness、或加 food spawn near emitter 让进 halo 与活下来不冲突、或软化 halo 12→16m + window 15→25s。当前 5 轮 v3→v7 数据强烈指向：不是 perception/reward channel 缺口，是 task 全局比例失衡 + NEAT 30/60 代探索效率不足。
2. 把 `Build / Hunt / Signal` 三条 credit 统一成一套 route-local `EventLineage / Contribution` 口径，避免 communication 继续缺因果归因。
3. 基于新的 communication 指标与 ablation，优先完成一条真的成立的通信任务（`scout-worker` / `danger warning` / `build request` 三选一先收口）。
4. 把 `supporting/METRICS_FIELD_GUIDE.html` 继续作为解释层基座维护，后续 communication / lineage / lifecycle 字段都要同步进去。
5. 在 cognition-social 主线没有收口前，不默认继续扩 `BrainV3`、GPU profiling、正式长训、3D replay 或 live world。
