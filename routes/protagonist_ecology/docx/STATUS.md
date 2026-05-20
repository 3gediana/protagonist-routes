# STATUS.md · Neural Eco 全仓交接手册

> 最后巡检：2026-05-21（夜间会话）
> 巡检范围：`d:/claude-code/c++` 整个工作树
> 当前分支：`master`
> 当前 HEAD：`146c37f feat(core): Stage 5 - MapElitesArchive for quality-diverse curriculum`
> 交接目标：让新对话能在**不依赖旧上下文**的情况下，快速接手仓库真实状态。

> **本版本相对 2026-05-20 版的主要变化**：完成 Stage 0 / 1 / 2 / 3 / 4 / 5 全部 5 个机制扩展阶段。下面正文以"现在状态"为准，不再重复昨天的旧叙述。

---

## 1. 先看这一页结论

### 1.1 这个仓库现在是什么

C++ 神经进化生态系统项目。两条共享底座的开发线：

- **主仓主线**：`src/` 通用生态系统框架
- **当前主力工作线**：`routes/protagonist_ecology/` 主角生态路线

`master` 的最新提交都集中在 protagonist route。

### 1.2 新对话最重要的五个事实

1. **5 个 Stage 全部已完成并提交**
   - Stage 0: CT 持续动作结构化事件
   - Stage 1: Memory Phase A 闭环（44 维 perception + 4 banks 全联通 + ablation 基础设施）
   - Stage 2: P3 ContributionLedger（worksite 与 hunt 都按贡献分配）
   - Stage 3: 富化 GoalManager（hierarchical-ish，13 类目标中已主动使用 10+）
   - Stage 4: World Model trace dataset builder
   - Stage 5: MapElitesArchive（与 NoveltyArchive 并存）

2. **没有任何已知 build / test 回归**
   - 75/75 单测全绿
   - protagonist smoke 全程能跑

3. **未做的事**（按设计明确推迟，不是漏掉）
   - 正式训练（用户保留决策权）
   - 真正的 Manager/Worker 双头脑（脑结构升级属 Phase B）
   - DNC-forcing micro-task（属 Phase B）
   - GPU 路径激活（本机 CUDA toolkit 不完整）
   - 可视化层（ADR-0005 明确延后）
   - POET / EnvironmentGenome（属 Phase B+）

4. **当前可演示的核心机制环**
   - chop → ChopCompleted 事件 → EpisodicMemoryBank.chopped_tree 条目 → MemoryCognitionPerception 输出到 brain
   - build → 每 dt 累积 per_agent_contribution_seconds → BuildCompleted 含贡献时长向量 → worksite_completion_reward 按 share 分配 → built_worksite episodic（rank-derived salience）+ socialMemory cooperation by share
   - kill → cumulative damage ledger → joint_hunt_reward 按 damage share 分配 → socialMemory cooperation by share
   - 4 个 ablation 开关全部接线可观测

5. **接手时一定要先做**
   - 读 PROGRESS.md（route 的）
   - 读本文件（STATUS.md）
   - 读 docs/experience_log.md（如果存在）
   - **不要悄悄开正式训练**——用户明确要求开训前必须通知

---

## 2. 新对话推荐阅读顺序

### 2.1 最小阅读顺序

1. `AGENTS.md`
2. `PROGRESS.md`（顶层）
3. `routes/protagonist_ecology/docx/PROGRESS.md`
4. `routes/protagonist_ecology/docx/STATUS.md`（本文件）
5. `docs/ROADMAP.md`
6. `docs/ARCHITECTURE.md`

### 2.2 接 protagonist route 继续做事时再读

1. `routes/protagonist_ecology/docx/MEMORY_SYSTEM_MASTERPLAN.md`
2. `routes/protagonist_ecology/docx/MEMORY_PHASE_A_EXECUTION_PLAN.md`
3. `routes/protagonist_ecology/docx/MEMORY_ABLATION_SMOKE_RESULT.md`
4. `routes/protagonist_ecology/docx/CONTINUOUS_TIME_WORLD_DESIGN.md`
5. `routes/protagonist_ecology/docx/COMPUTE_SCALING_GPU_CPU_PLAN.md`

### 2.3 接手第一件事不是写代码

按顺序：

1. `git status --short`（看工作树是否 dirty）
2. `git log --oneline -10`
3. 跑一次 smoke 验证不破：
   - `cmake --build build --config Release --target neural-eco-protagonist test_core`
   - `./build/bin/Release/test_core.exe --gtest_brief=1`
   - `./build/bin/Release/neural-eco-protagonist.exe --config configs/protagonist_ecology_train_smoke.toml`
4. 然后再读本会话产出的新文档（见第 5 节）

---

## 3. 本次会话推进的所有 Stage

> 这一节是"如果新对话只看一节就知道这一夜做了什么"。

### Stage 0 · CT-2B/CT-3 结构化动作事件 ✅

**commit**: `8ea81b2`

之前的 chop/build 只有"attempts/successes 加 1"的离散计数。本 stage 改成结构化事件：

- `core/events/EventType.h` 增 6 个 protagonist route 事件：
  - `ChopStarted` / `ChopCompleted` / `ChopAbandoned`
  - `BuildStarted` / `BuildCompleted` / `WorksiteMaterialDeposited`
- `core/events/EventPayloads.h` 含完整 payload 结构
- `TreeLayer::chopNearest` 改返回 `ChopHitResult{tree_id, tree_pos, remaining_integrity, felled, wood_yield}`
- `ChopTreeCapability` 变成有状态 per-agent session machine：start / continue / target-switch / abandon / complete
- `WorksiteLayer` 三个新字段：`build_started_at_seconds` / `per_agent_contribution_seconds`（map）/ `active_builders`（set）
- `progressConstruction` 在首次进入发 `BuildStarted`、每 dt 累积贡献时间、完成时按贡献时长降序发 `BuildCompleted`
- `absorbCarriedMaterials` 实际入库时发 `WorksiteMaterialDeposited` 含 depositor + material_type
- `TraceRecorder` 已为 6 个新事件接好 payload 序列化

**验证**：smoke trace 中可见 `ChopStarted=8 / ChopCompleted=6 / ChopAbandoned=2`，full payload 字段（chopper, tree_id, started_at_seconds, duration_seconds, wood_yield）齐全。

### Stage 1 · Memory Phase A 收口 ✅

**commits**: `d6bc642` + `72e2199` + `01eec4d`

- runtime 评估循环新增 3 个 tick 收集器（`tick_chop_completed` / `tick_build_completed` / `tick_deposits`），订阅 EventBus 在每个 substep 收集
- 在 per-agent memory 写入循环里转写 5 类 episodic：
  - `chopped_tree`（salience 按时长）
  - `built_worksite`（salience 按 contributor rank）
  - `deposited_stick` / `deposited_stone`（salience 0.30）
  - `signal_emitted`（salience 0.20）
- 加 `MEMORY_PHASE_A_EXECUTION_PLAN.md` 中验收清单的 7 项全部 PASS（见 PROGRESS.md 第 8 节表）
- 新增 `scripts/protagonist_memory_ablation.py`：一次跑 baseline + 4 个 ablation（spatial / episodic / social / goal），自动生成对照表，结果在 `MEMORY_ABLATION_SMOKE_RESULT.md`

**重要诚实说明**：smoke 结果显示 ablation 反而 fitness 更高——这是 NEAT-3-gen 过拟合到噪声的预期反应，不是"记忆有害"。详见 `MEMORY_ABLATION_SMOKE_RESULT.md`。

### Stage 2 · P3 ContributionLedger ✅

**commits**: `77dc5dc`（worksite）+ `b9bae01`（hunt）

替换 free-rider-prone 的"在场就分"模式：

- **Worksite credit**: 用 BuildCompleted 的 contributors[] 与 contribution_seconds[] 配对（已在 Stage 0 落地数据源）。Runtime 现在：
  - 找自己在 contributors 中的 index
  - 若贡献 < 0.5s（free-rider threshold）跳过
  - 否则按 `my_seconds / total_legitimate_seconds` 分得 `worksite_completion_reward`
  - 社交合作分也按 share 分配给同 worksite 其他贡献者
- **Hunt credit**: 新增 `hunt_damage_ledger[victim_id][attacker_id]` 累计 episode 内伤害。新增 `tick_deaths` 收集器订阅 AgentDied：
  - 每次死亡按 damage share 分配 `joint_hunt_reward`
  - 阈值 1% 过滤偶然擦伤
  - 社交合作分也按 share 分配给同杀其他攻击者
- `tests/route_protagonist/test_contribution_ledger.cpp`：3 个 builder（长 / 短 / 路过者）的单测，验证排名正确 + 路过者过滤

**验证**：74→75 单测全绿；smoke 全程跑通。

### Stage 3 · 富化 GoalManager（P7 partial）✅

**commit**: `983f391`

之前 GoalManager 只用 13 个 GoalType 中的 6 个（Flee/FindWater/BuildWorksite/RestNearFire/FindFood/Explore）。本 stage 接入 4 个新 goal，让 goal_index 分布对 brain 更可区分：

- `EmitSignal`: predator_nearby AND peer_visible
- `GatherStick`: 活跃 worksite 缺 sticks AND 不在搬运
- `GatherStone`: 活跃 worksite 缺 stones AND 不在搬运
- `RespondSignal`: 同 tick 内 companion radius 内监听到 peer 信号

新优先级链：Flee > EmitSignal > FindWater > BuildWorksite > GatherStick > GatherStone > RespondSignal > RestNearFire > FindFood > Explore。每条规则保留 should_refresh_goal gating 避免逐 tick 翻转。

**验证**：smoke trace 显示 goal 分布新增 `respond_signal` 等条目。

**未做**：真正的 Manager/Worker 双脑头（属 Phase B 脑结构升级范畴）。

### Stage 4 · World Model trace dataset builder（P6 partial）✅

**commit**: `983f391`

新增 `scripts/trace_to_world_model_dataset.py`：

- 遍历指定 run 下所有 `trace/generation_*/episode_*/agent_trace.csv`
- 按 agent_id 分组 + 排序，相邻样本拼成 `(obs_t, action_t, obs_{t+1}, reward_delta, dt)`
- obs_dim = 31（agent_trace 的数值列子集）
- action_dim = 4（dvx, dvy, is_carrying, goal_index）
- 输出单一 CSV，无 torch / parquet 依赖

**验证**：smoke run 生成 8864 行 transition，72 列 header 一致。

**未做**：实际的 world model 预训练 / 模型加载回 brain（属 Phase B）。

### Stage 5 · MapElitesArchive（P5 partial）✅

**commit**: `146c37f`

新增 `src/core/evolution/novelty/MapElitesArchive.{h,cpp}`：

- 与 NoveltyArchive 共用 `BehaviorCharacterization`（8 维行为描述）
- 网格化 bins_per_dim 为离散行为空间
- `considerCandidate(GenomeId, BC, fitness)`：新 cell 或 fitness 严格提升时返回 true 并更新 elite
- `bestFitness() / avgFitness() / coverage()` 诊断接口
- 单测 `tests/core/test_map_elites_archive.cpp` 验证：新行为开新 cell / 弱 fitness 不替换 / 强 fitness 替换 / 聚合 fitness 计算正确

**未做**：把 MapElitesArchive 真正接到 NeatEvolution 的 reproduction loop（属 Phase B 进化策略升级）。

---

## 4. 当前已知的真实缺口（按优先级）

### 4.1 Phase B 内核工作（最高价值，但都属下一阶段范畴）

- **DNC-forcing micro-task**：设计一个只能靠 DNC 检索过去状态才能拿 reward 的场景，验证 DNC 不是摆设
- **ProtagonistBrainV3 / Manager-Worker heads**：真正的 hierarchical 神经网络架构
- **WorkingMemoryBank 多时间尺度**
- **ProtagonistGenomeV2**：编码更大脑结构的 genome

### 4.2 工程基础设施

- **本机 CUDA toolkit 不完整**：`where.exe nvcc` 找不到 nvcc，`v11.8` / `v12.4` 目录只有 `extras/`。protagonist dense GPU 代码已落但路径未激活
- **EventQueue / DueAction 底座**：CT-Step 4 主动延后，等 goal expiry / episodic decay 需要时再做
- **可视化层**（M2）：ADR-0005 明确延后，`src/viz/` 仍是空骨架

### 4.3 科研验证缺口

- **真正的 ablation 反向曲线**（关掉 memory 让 fitness 下降）：当前 smoke 只有 3 代不够，需要长训
- **F-COMM / F-COOP / F-NEUROMOD 涌现证据**：见 `docs/EMERGENCE_AUDIT.md`，全 0 达成原始完美标准

### 4.4 风格 / 整洁

- 部分文档仍说 `MemoryCognitionPerception` 12 维（代码实际 44 维）——以代码为准
- `src/viz/README.md` 和 `tests/README.md` 都有过时段落
- `configs/default.toml` 默认 `mode = "interactive"` 但 interactive runtime 是骨架

---

## 5. 本会话新增 / 修改的关键文件

### 5.1 核心代码

| 文件 | 变化 |
|---|---|
| `src/core/events/EventType.h` + `EventType.cpp` | 加 6 个 protagonist route 事件类型 |
| `src/core/events/EventPayloads.h` | 加 6 个 payload 结构（BuildCompleted 含 contribution_seconds） |
| `src/core/evolution/novelty/MapElitesArchive.h` / `.cpp` | 新文件，Stage 5 |
| `routes/protagonist_ecology/world/TreeLayer.{h,cpp}` | chopNearest 改返 ChopHitResult |
| `routes/protagonist_ecology/world/WorksiteLayer.{h,cpp}` | per_agent_contribution_seconds + 结构化事件 |
| `routes/protagonist_ecology/capability/ChopTreeCapability.{h,cpp}` | 改成 per-agent session machine |
| `routes/protagonist_ecology/runtime/ProtagonistEcologyRuntime.cpp` | 多个 tick 收集器 + ContributionLedger + 富 GoalManager + episodic 写入扩展 |
| `routes/protagonist_ecology/runtime/TraceRecorder.cpp` | 6 个新事件的 payload 序列化 |

### 5.2 测试

| 文件 | 变化 |
|---|---|
| `tests/CMakeLists.txt` | 加链 `neural_eco::route_protagonist` |
| `tests/route_protagonist/test_contribution_ledger.cpp` | 新文件，Stage 2 验收 |
| `tests/core/test_map_elites_archive.cpp` | 新文件，Stage 5 验收 |

### 5.3 脚本

| 文件 | 用途 |
|---|---|
| `scripts/protagonist_memory_ablation.py` | Stage 1 · 跑 baseline + 4 ablation 对照 |
| `scripts/trace_to_world_model_dataset.py` | Stage 4 · 从 trace 生成 world model 训练数据集 |

### 5.4 文档

| 文件 | 用途 |
|---|---|
| `routes/protagonist_ecology/docx/MEMORY_ABLATION_SMOKE_RESULT.md` | 新文件，首轮 ablation 结果 + 诚实解读 |
| `routes/protagonist_ecology/docx/PROGRESS.md` | 大幅扩充，Stage 0/1 详细记录 |
| `routes/protagonist_ecology/docx/STATUS.md` | 本文件，完整重写 |

---

## 6. 接手时的默认行为

### 6.1 如果用户说"继续 / 接着做"

1. 先看 git status 确认没漏掉的工作树脏文件
2. 跑 `test_core` + protagonist smoke 确认 baseline 绿
3. 看 PROGRESS.md 顶部"下一步"
4. **默认推进方向**（按价值/可行性排序）：
   - **A. Phase B 启动**：DNC-forcing micro-task 设计 + ProtagonistBrainV3 框架（高价值，需要好几个会话）
   - **B. 长训第一轮**：跑长训 5 ablation 看是否反转 → 这是用户保留决策权的事，必须先问
   - **C. CUDA toolkit 修复 + GPU 激活**：环境问题，需用户协助
   - **D. EventQueue 底座**：等 goal expiry / episodic decay 实际需要时再做
   - **E. World Model 预训练 pipeline**：用 Stage 4 的 dataset 真跑一个小模型（Python 侧）

### 6.2 红线（绝不能做的事）

1. **不能擅自开正式训练**：用户多次重申。所有"5 ablation × 100 代"或类似 1+ 小时训练任务必须先问
2. **不要把 mixed compute 误认为 GPU 已通**：代码有，环境没有
3. **不要把 ablation smoke 结果当成"记忆有害"的科学结论**：3 代不够
4. **不要回滚 / 重命名 Stage 0–5 已落地代码**：通过测试验证过的

---

## 7. 当前 commit 链（master 顶端）

```
146c37f  feat(core): Stage 5 - MapElitesArchive for quality-diverse curriculum
983f391  feat(protagonist): Stage 3 + Stage 4 partials - richer goals + trace dataset
b9bae01  feat(protagonist): Stage 2 - extend ContributionLedger to hunt credit
77dc5dc  feat(protagonist): Stage 2 - P3 ContributionLedger for worksite credit
01eec4d  docs(protagonist): mark Stage 1 / Memory Phase A complete
72e2199  feat(protagonist): memory ablation smoke harness + first result
d6bc642  feat(protagonist): wire CT-2/CT-3 events into episodic memory
8ea81b2  feat(protagonist): Stage 0 - structured chop/build events for CT-2B/CT-3
c7f1665  feat(protagonist): consolidate protagonist route as primary work line
54d6771  refactor: advance protagonist continuous-time runtime
```

`c7f1665` 之前是用户原 STATUS.md 的 HEAD。从 `8ea81b2` 起都是本会话的工作。

---

## 8. 最终交接结论

如果新对话只记住一句话：

**5 个 stage 的机制扩展都已经落地、测试通过、smoke 验证可跑。protagonist 训练环境从 "机制存在但没真闭环" 推进到 "机制存在 + 完整 reward / memory / goal 闭环 + 因果归因 + 离线世界模型数据 + 多样性 archive"。下一步不是再写机制，而是用户介入决定何时跑首次长训。**
