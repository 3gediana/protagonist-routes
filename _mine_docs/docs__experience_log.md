# experience_log.md · 实验与调参记录

## 通用规则（跨实验提炼）

### 调参与排查
- **调任务参数前先读所有相关常量**（`kRadius`、refresh interval、字段单位）。`kWaterSenseRadius=36` > `spawn_radius=26` 让水永远可见、`direction_requires_visibility` 失效；`SignalPerception.kSignalRadius=16` 决定 emitter 必须放多远。
- **看到 `_per_second` 字段名先回 layer tick 函数核对单位**。`hydration_decay_per_second` 是 absolute/s 对 `max_hydration=100`，不是 ratio/s。
- **continuous-time 重构里名字也是机制的一部分**：值已经是 `value * dt`，但配置/成员名还写 `per_tick`，会让人误判语义。
- **读 trace 比纯调参更值钱**：当一个事件指标看起来违反直觉（baseline=0、ablation 反而更高、跨变体完全相同），先去 trace 而不是改参数。
- **遇到崩溃先查 NaN 传播链**：CTRNN 内部状态、agent position、HeightField 索引都是 NaN 高发点。多层防护（forward / capability / layer 入口都加 NaN 检测）。

### Ablation 评估方法学
- **ablation eval 的有效性取决于训练任务覆盖**：未训过的维度 events 永远 noise，只能在 fitness 维度看到弱分离。
- **同时看 events + fitness 两个维度**：events 能 0、fitness 能差 60%，policy OOD 时只看 events 会得到错误结论。
- **fixed genome 评估泛化**：训练任务里有的能力部分泛化到 ablation task；没有的能力（task-specific）无法靠泛化补上。

### Observer / 指标设计
- **修 observer 指标时先看统计口径，再看模块是否真的坏了**：很多"指标全 0"是观察口径错位（代末快照 vs tick 累积），不是机制断了。
- **过程型机制优先 tick 累积 + 代末汇总**，代末快照很容易撒谎。
- **字段名看起来像一个意思，统计口径常常完全不同**。同名字段在不同 CSV 里可能是 events 计数 vs 状态快照 vs 跨 episode 平均，必须回代码确认。
- **DNC / 脑状态摘要是"在活动"，不等于"对任务有效"**。真正的硬证据是 fixed-task ablation 的 primary_metric 分离。

### 重构与兼容
- **Brain / Genome 第一阶段必须是"结构拆壳"，不是"参数面升级"**。先把代码从单块拆成组件，旧 buffer 和 dim 口径保持兼容；先**新结构、旧参数面**。
- **load-bearing 接口**（genome 序列化、evolution crossover/mutate、checkpoint、dense GPU mirror）一起改最容易炸 — 一次只能动一面。
- **route-local config struct 容易出现"字段定义了、配置写了、parser 漏读"的假接线**：编译期不报错，只在 runtime 某个组合配置依赖它时才炸。每加一段 `[world.xxx]` 都配一个最小 parse regression test。
- **重构内部时最容易死在编译接线上**（CMake 漏接 `.cpp`、accessor 签名不一致、测试 target 没编进新文件）—— 下一步永远先做最小 build/test 验证，再继续扩功能。

### Checkpoint / Resume
- **真正的"可续训"≠ 只存 population**：还要存 evolution RNG、NEAT speciation threshold / representatives / next species id。
- **resume 语义最好是 generation+1，不是"重评估当前代"**：checkpoint 保存点应在繁殖之后。
- **训练 checkpoint 必须连配角一起存**：主角恢复但背景物种从头随机，训练上下文已经变了。
- **checkpoint 文件命名/扫描排序这种小问题足以毁掉完整 resume**：`genome_10` 字典序排在 `genome_9` 前面会悄悄打乱 population 顺序。
- **验证 checkpoint-resume 时要区分"resume 逻辑问题"和"训练并行不完全确定"**：默认多线程不是逐 run 严格确定性，先临时设 `num_threads=1` 做 full-vs-resume 等价对照。

### Continuous-time 迁移
- **runtime 改 substep 不能不改 reward/cooldown/metrics**：持续型奖励（survival / proximity / shaping）改成乘 `dt`；事件型奖励（kill / emit / completion）保持一次性。
- **MoveCapability 能量成本必须乘 `dt`**，不然 substep 越小单位真实时间扣能越多。
- **memory / trace 不一起改会出现"runtime 连续、日志离散"的假连续时间**：必须 dual timestamp（tick + seconds）。
- **资源层最好支持部分消耗**：`EatCapability` 在 capability 内部假分片，底层 `ResourceLayer` 仍是"一次拿光"，substep 变细会失真。

### 并行 / GPU
- **agent 级并行先做"读世界"和"写世界"分离**：危险的是 capability apply 期间的 world mutation，不是 dense forward。
- **双层并行不做预算联动 CPU 会被打爆**：外层 episode pool × 内层 decision threads 默认线程乘法膨胀。
- **GPU backend 最好从 route 内局部接起，不要污染全仓库构建链**：CUDA 不存在时 CMake 不应让整个仓库挂掉。
- **"build 通了" ≠ "GPU 已加速训练"**：当前 GPU 只接管 dense forward 的小段 + 每步 host↔device 拷贝；camp-building 1-gen CPU 5.6s vs CUDA 8.5s。

### 工作流
- **micro-task smoke 先裁成极短版本**（2 代 × 1 episode）就够把 parser / runtime / reward wiring 大问题抓出来。
- **smoke 通过 ≠ 机制学会**：只能证明 wiring 通、reward 在流；要证明 memory / DNC 真有效，得长跑 + ablation 对照。
- **优先级很容易跑偏**：当前关键路径不是 production world，是训练世界先把能力链学出来；不是再加新世界，是把 `curriculum_camp_building` 链做扎实。

---

## 2026-05-22 · 训练 signal_response 三次失败 — train/eval objective alignment 的鸡生蛋问题

为让 ablation eval 里 `signal_response_events` 真正分离，从把 controlled emitter 接入训练开始，连续做了 3 次 30 代训练 + ablation 验证，**全部失败（events 全 0）**，留下 3 条经验：

**第 1 次**：在 camp_building train config 加 `world.signal.controlled_emitter_enabled=true` + 拉高 `signal_co_attendance_reward` 1.5 + emitter 周围 spawn 食物。

结果：fitness 上涨（avg 2171→5365），`signal_co_attendance` 翻倍（4812→10453），但 `signal_response_events` 全 0。**根因**：`signal_co_attendance_reward` 是 **emitter-side reward**（自己 emit + peer 同伴在你半径内吃食物），不是 listener-side。controlled emitter 的 `emitter_id=kInvalidAgentId`，永远不会触发这个 reward 通路。

**第 2 次**：加 `signal_response_reward` listener-side 字段（agent 进入 emit halo + 在 window 内即给 reward）。

结果：fitness 又涨，events 仍 0。**根因**：events 计数和 listener reward 都 wrap 在 `if (currentGoal == RespondSignal)` 内。GoalManager rule-based 优先级表里 RespondSignal 排在 health/predator/hydration/carrying/site_needs 之后，训练时 agent 总在 GatherStick 或 BuildWorksite 状态，几乎从不切到 RespondSignal goal。metric 测的不是 agent 能力而是 GoalManager 优先级表。

**第 3 次**：把 events 计数和 listener reward 从 `currentGoal == RespondSignal` 解耦，直接看物理位置 + window。

结果：fitness 巨涨（avg 6519→19680，best 74833），events 仍 0。**根因（鸡生蛋）**：listener reward 触发条件是"进入 halo"，但 agent 没动力进 halo（emitter 远在 28m 外，agent 在 nest 里看不见、没 perception gradient 引导）。需要 perception-driven shaping reward 让 agent 朝 emitter **移动过程中**就拿 reward，但那是又一轮 reward shaping。

**留下的可复用基础设施**（commit `2c81ca4` + `81f763f`）：
- `controlled_emitter_enabled` flag 让 controlled emitter 可在训练时启用，randomize_position 防过拟合，emit 时通过 `ResourceLayer::addCorpse` spawn 食物
- `signal_response_reward` listener-side reward field + parser，默认 0 不影响现有训练
- `signal_response_events` metric 跟 GoalManager 解耦，直接物理位置 + window
- train_camp_signal.toml 训练 config 已 ready

**通用教训**：
- ablation eval 的 metric 设计必须跟训练 reward shaping 完全对齐，否则训出来的能力跟测的能力不是同一回事
- 训练 reward 不只是数值大小，还要看**触发条件方向**（emitter-side vs listener-side）
- rule-based GoalManager 是 metric 的隐藏依赖：metric 包 `if (currentGoal == X)` 时实际测的是 GoalManager 优先级
- listener-side reward 触发条件是"进入 halo"会有鸡生蛋问题，需要 perception-driven shaping 给 agent "朝 X 走"的连续 gradient

下一步（不在今晚做）：perception-driven shaping reward，比如基于 SignalPerception 强度给 dense reward，或者把 food spawn 半径放大到 SignalPerception 16m 范围让 agent 在能听信号时同时能看食物。

---

## 2026-05-21 · memory ablation 任务 tightening + peer_signal_nearby bug

修 fixed-task memory ablation 的两个分离弱（water/no_spatial 41% 残余、signal 分离几乎不存在），最后只 water 拿到干净分离，signal 暴露 genome OOD。

**根因 1 · water 任务对 spatial 无压力**：`kWaterSenseRadius=36` > 旧 `spawn_radius=26`，水永远在感知内，`direction_requires_visibility=true` 失效。改 spawn=42 后 baseline 207.5 / no_spatial 90.25（~56% drop, 7/8 干涸）。

**根因 2 · hydration_decay 是 absolute/s 不是 ratio/s**：`max_hydration=100`，初次按 ratio 设 0.025/s 让 agent 200s 都不渴。看 `SurvivalStatusLayer.cpp` 才发现单位。

**根因 3 · peer_signal_nearby 真 bug**：之前只看 `tick_emits`（本 tick）。emit 间隔 6s 时几乎不暴露；拉到 30s + GoalManager 6s refresh 后，listener 几乎永远错过 emit 那个 tick → 永远不切 RespondSignal goal，events 永远 0。修：扫 `last_signal_emit` map + window 过滤。

**根因 4 · genome OOD**：signal task 设计修好后 events 仍只 1.5 vs 1.75。原因不是任务，是 policy — 这个 fixed genome 训练时没有 signal_response 奖励，没学过"silent gap 靠记忆维持航向"。water spatial 分离干净是因为 worksite_return 训练里用了 spatial，能力部分泛化；signal 能力 task-specific，无法靠泛化补。

---

## 2026-05-21 · 指标说明单页 + 真实统计口径核对

为 protagonist route 的 CSV 输出做了一份单页说明 `routes/protagonist_ecology/docx/supporting/METRICS_FIELD_GUIDE.html`，把所有指标按"训练主指标 / 主角行为 / 记忆DNC / 生态模块 / 研究统计"分类，明确每个字段是 events 计数 / 状态快照 / 跨 episode 平均 / DNC 摘要 / proxy。

**高风险误读点**：
- `signal_emits`（被记录到的 emitter 数）≠ `signal_emit_events_avg`（fixed-task probe 到的真实 emit 事件均值）
- `completed_worksites`（episode 末快照）≠ `worksite_completion_events`（过程中"新完成工地"的事件次数）
- `active_fires` 是状态不是事件
- `dnc_usage_mean_avg / dnc_read_peak_avg` 只能说 DNC 在活动，不能下"记忆有效帮助任务"结论
- `emergence_audit.csv` 的 `modulatory_corr / species_dig_preference_var` 是 proxy，适合解释趋势不适合替代真实事件

---

## 2026-05-21 · fixed-task memory ablation worksite_return 修复 + 4-episode full run

worksite_return 1-episode traceprobe 全变体为 0。根因不是 WorksiteLayer 本身，是 `GoalActionAssistCapability`（自动 pickup）和旧 `DropCapability` / `ThrowCarriedObjectCapability` 在同一 tick 互相打架，加上 `WorksiteReturn` 任务里 nursery logistics 搅乱物流。

**修法**：assist 在 logistics 相关 goal 下优先回 remembered worksite；新增 `GoalAwareDropCapability` 在 build/logistics goal 下不让普通 drop 路径丢材料；ThrowCarriedObjectCapability 在 logistics goal 下抑制 stone throw；WorksiteReturn 任务禁用 nursery。

**4-episode full run 结果**（`data/runs/protagonist_memory_ablation_fixed/20260521-212657`）：
- water_return: baseline/no_episodic/no_social = 162, no_spatial = 67.5, no_goal = 0
- worksite_return: baseline/no_episodic/no_social = 0.5, no_spatial/no_goal = 0
- signal_response: baseline = 15.5, no_goal = 0, 其余 12.25~13（分离弱）
- danger_avoidance: no_episodic 显著变差 8 → 6.25

`no_goal` 最稳定的 ablation；`no_spatial` 在 worksite 上干净，水/信号上不够强。下一步重点应是解释和任务 tightening。

---

## 2026-05-21 · Windows 下 protagonist 构建链固定路径

后续会话避免重复在 Windows / MSVC / WSL 路径上绕路，固定使用：

- CMake：`C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe`
- Ninja：同目录下 `Ninja\ninja.exe`
- MSBuild：`C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe`

**约定**：
- 构建工具用绝对路径，不赌 PATH
- 重编 protagonist 路线只重编 `build/routes/protagonist_ecology/neural-eco-protagonist.vcxproj`
- 运行时 config / genome / runs_dir 路径用 Windows 风格绝对路径 `D:/claude-code/c++/...`，不要混 `/mnt/d/...` 或相对路径
- 临时 TOML 放 `build/tmp/`

---

## 2026-05-21 · 文档同步 + protagonist 完成度重校准 + camp-building CPU/CUDA 基准

用户指出仓库对 protagonist 长蓝图完成度的描述失真。回到文档对齐：

- 按 `WORLD_UNDERSTANDING_RESEARCH_AND_ENGINEERING_PLAN.md` 8 个 Phase 严格算，**只有 Phase 1 Trace/Resume 真正收口**；Phase 2/3/5/6/7 都 partial；Phase 4 / 8 还没真正开始
- **GPU 不等于训练加速**：CPU build 5.6s/gen，CUDA build 8.5~9.2s/gen。当前 GPU 只接管 ProtagonistBrain 一段 dense forward，世界推进/事件/memory write/DNC step 仍在 CPU；每步 host↔device 拷贝。这是"局部前向加速尝试"，不是训练主引擎。

口径修正：
- 不再说 "protagonist 长路线已完成 / 100%"
- 不再把 "CUDA build 可运行" 说成 "训练已获得 GPU 加速"
- 默认改成"少量完整 + 多项 partial + 未开始"

---

## 2026-05-21 · ProtagonistBrainV3 最小壳层（兼容现有 genome）

不破坏 `ProtagonistGenome / Evolution / checkpoint / dense GPU` 兼容面，先把 `ProtagonistBrain` 内部从单块 dense controller 拆成组件 — `ObservationEncoder / MemoryQueryEncoder / BodyStateMemory / WorkingMemoryBank / BehaviorFusionTrunk / MotorCore / ActionHeads / InterfaceHead`。

**保持兼容**：
- `total_input_dim_ = external_input_dim + memory_cells * read_heads` 不变
- 6 组参数 buffer 不变（in_hidden / hidden_bias / hidden_action / action_bias / hidden_interface / interface_bias）
- DNC `decodeInterface()` 与 optional `ProtagonistDenseGpu` fallback 路径不变

**接 learnable path**：`BodyStateMemory.summary()` 折回 observation slice，`WorkingMemoryBank.summary()` 折回 DNC read-query slice 并继续调制 `motor_target`。这样 body/working memory 已经能通过原 `in_hidden_weights_` 影响 controller，而不只是"旁边有个状态对象"。

**护栏测试**（防止退回"有状态但不参与控制"的假接线）：
- `ExposesMinimalBrainV3ShellState`
- `FeedsBodyAndWorkingMemoryIntoLearnablePath`
- `FeedsWorkingMemoryIntoLearnablePath`：两个同 seed 的 brain 只给一个预热 working memory，要求输出有差异

DNC cue-choice 短 smoke 在 3 次重构后都跑通 `gen0 best=-6, gen1 best=24`（task wiring 没坏）。但 route 多线程不是逐 run 严格确定性。

---

## 2026-05-21 · DNC-forcing micro-task 验证 + signal config 漏读 bug

短 smoke 启动时 runtime 报 `protagonist action_dim does not match configured move/signal capability outputs`。配置写了 `world.signal.enabled = false` 但仍要求 `action_dim = 4`。

**根因**：`ProtagonistEcologyConfig::fromConfig()` 完全漏读了 `world.signal.*`，所有 `signal.enabled/channels/grid_resolution/decay/diffusion` 都退回 struct 默认值（signal 仍认为开着、channels=2）。

**修**：补 parser 读取，加 `ParsesSignalSection` 回归测试，避免再出"配置写了但 struct 没读进去"的 silent fallback。同时把 MemoryTrialLayer 作用域收口到 protagonist 个体，避免污染 background species。

裁短 smoke（2 代 × 1 episode）输出 `best=24 / min=-6`，对上 task 的 success bonus / failure penalty，说明 reward 真的在流。但这不证明 DNC memory 有效，要看长训 + ablation。

---

## 2026-05-20 · protagonist mixed compute 第一轮工程化

把 compute 从 episode 级并行下钻到 agent decision 级，给 ProtagonistBrain dense controller 接上 optional GPU backend。

**主要落地**：
- `Agent::step()` 拆成 `decide() / publishDecisionEvent() / applyCapabilities()`：先并行 decide，再串行 apply。
- `AgentLayer` 持久化 `DecisionExecutor`，per-world agent decision 并行，capability apply / 能量结算 / 死亡事件保持串行。
- route runtime 综合 `evolution.num_threads / agent_decision_threads / episodes_per_generation / hardware_concurrency` 联动分配预算，避免外层×内层线程乘法膨胀。
- `ProtagonistBrain` 接 optional dense GPU backend（`ProtagonistDenseGpu.{h,cpp,cu}`），DNC memory core 留 CPU；GPU 不可用自动退 CPU。
- compute 开关：`compute.protagonist_dense_gpu_enabled`、`compute.agent_decision_threads`。

**当时未完成**：本机缺完整 CUDA toolkit（`C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v11.8/v12.4` 只有 extras/，没 bin/include/lib），`build/CMakeCache.txt` 里 `CMAKE_CUDA_COMPILER:FILEPATH=NOTFOUND`。后续用 portable CUDA `D:\NVIDIA\CUDA\v12.8\` 才编通 GPU 路径。

---

## 2026-05-20 · protagonist route checkpoint-resume Phase 1 收口

把 checkpoint 从"半成品断点续训"推到完整训练 continuation 管线。

**Phase 1 完整覆盖**：
- protagonist + background species NEAT population 全存（每个背景物种 slot 一份 `meta.toml + population.bin`）
- innovation tracker state（NEAT 创新历史）
- protagonist GA RNG + background NEAT RNG（`*_evolution_rng.txt`）
- `SpeciationState`（compatibility threshold + next species id + previous representatives）
- `best_fitness_seen` + `best_genome_overall.bin / best_genome_latest.bin` continuity（resumed run 不从头重算 best）
- generation index + `checkpoint_state.toml` 机器可读入口

**语义改造**：checkpoint 从"评估后、繁殖前"挪到"繁殖后"，resume 直接从 `resume_generation` 开始，不再重复评估已存那一代。

**修了一个隐蔽 bug**：protagonist checkpoint 之前按文件名字典序读 `genome_*.bin`，`genome_10` 排到 `genome_9` 前面，悄悄打乱 population 顺序。改成按数值 id 排序。

**single-thread full-vs-resume 等价对照通过**（`evolution.num_threads=1`）：full run gen2 = `360.286,755.699,1594.6` ≡ resume run gen2 同值。多线程不是严格确定性的。

---

## 2026-05-20 · protagonist continuous-time 主链迁移

从"世界层有 `currentTimeSeconds()` 但 runtime/reward/cooldown 仍按 tick"推到真正以 seconds 为主语义。

**主链落地**：
- `simulation.time.preview_duration_seconds / episode_duration_seconds / substep_dt / max_substeps_per_advance`，旧 `*_ticks` 兼容读取
- route-local 时间层（NurserySupplyLayer / NurseryProgressionLayer / InterventionDirectorLayer / TreeLayer）+ 核心层（ResourceLayer / TimeOfDayLayer / SeasonLayer）统一 seconds
- memory / trace dual timestamp：EpisodicMemoryBank / SocialMemoryBank / GoalMemory / TraceRecorder
- `ProtagonistBrain::forward(inputs, dt)` 改成 leaky integration，hidden / output 按 `dt / tau` 的 alpha 渐进积分

**CT-2A 收尾**：
- `EatCapability` 改成持续摄食：每次 `apply()` 最多消耗 `eat_rate_per_second * dt`；`ResourceLayer` 支持部分消耗，吃空才发 `ResourceConsumed`。回归测试锁住 small-dt 不会一口吞光。
- 试过给 `WorksiteLayer` 加"低速停留才持续施工"的隐式连续门槛，结果 preview_xlong 闭环退化（material 送到但 site_progress 不涨甚至 unlock 回退），回退该试验。结论：build/worksite 类需要显式 `build` action / progress channel 时再做。

---

## 2026-05-20 · protagonist runtime 接入真实训练闭环 + camp-building 环境整备

之前 runtime 只有 preview tick 循环，实际不能多代训练。补：

- `ProtagonistGenome.{h,cpp}`：覆盖 dense+memory 脑全部参数（input→hidden / hidden_bias / hidden→action / action_bias / hidden→memory candidate&gate / candidate&gate bias）
- `ProtagonistEvolution.{h,cpp}`：复用 `EliteSelection + TournamentSelection`，elite 保留 / 双亲交叉 / 权重扰动+重置
- `ProtagonistBrain` 支持从 genome 构造（不再只能随机初始化）
- `BootstrapPopulation` 支持训练 population 装配，preview / training 共用 protagonist 身体/感知/能力接口
- `ProtagonistEcologyRuntime` 双模式：无 `evolution.generations` 走 preview，有就进训练（按代创建世界、跑 episode、计算 fitness、输出 `fitness.csv / evolution.csv`、调用 `ProtagonistEvolution` 繁殖下一代）

**奖励最小可用版本**：survival tick reward + food + intrinsic + damage/kill/death penalty + 物流 deposit bonus（沿用 world.addFitness）。**还不是最终 survival curriculum reward**。

3 代 smoke：gen0 avg=219 best=912，gen1 avg=127 best=632，gen2 avg=139 best=663（"训练循环真的在工作"，不是"训练效果稳定优秀"）。

**camp-building 环境整备**（避免训练时把环境问题误当模型问题）：
- 工地解锁不再死卡 stock_stone（已开 stone phase 即视作 prerequisite 满足）
- `InterventionDirectorLayer::ScatterStone` 优先刷在激活且缺 stone 的工地附近，工地首次解锁额外补一枚 support stone
- `CraftSpearCapability` 先统计 active 工地缺 stone 数，可用 stone 不够时禁止做矛
- `WorksiteLayer` 新增 carried-material delivery hook：agent 带 stick/stone 进 active site 半径就自动交付，不强依赖学会精准 drop
- 多 seed（42/91/123）抽查 `active_worksites=1, completed_worksites=1`，闭环不再是单 seed 偶然

---

## 2026-05-20 · primitive tool 第二步 + 优先级回正

补：`Spear` ObjectType / `CarriedObjectPerception` / `CraftSpearCapability` / `ThrowCarriedObjectCapability` / `consumeStockpiledObject(ObjectType)`。protagonist `action_dim` 10 → 13；新增 `StockpilePerception / WeaponPerception`；`SandboxStatusLayer` 增工具链诊断（stock_spear / craft_attempts / craft_successes / throw_attempts / throw_hits / carry_spear）。

训练 run 出现非零 `craft_successes` / `throw_hits` —— 工具链从"接口存在"变成"行为真的发生"。

中途一度去做 live/prod 世界边界，**优先级错了**。当前关键路径不是 production world，是训练世界先把能力链学出来。回归原则：只盯 `routes/protagonist_ecology/` 训练世界。

---

## 2026-05-19 · ComprehensiveMetrics 假指标修复 + 机制闭环加固

收口几类"能跑但不可信"的核心问题：
- `ComprehensiveMetrics` 的 `action_entropy / alive_ratio / immune_ratio / spatial_entropy / 行为MI` 假指标
- `immune_` 与 `immune_memory` 不同步
- `MatingOccurred` 只发事件不参与繁殖
- `dietPreference` 有记录不进真实生态收益

**修法**：
1. 行为统计改事件驱动：补齐 `Flee/Tool/Terrain/Signal/Mate` 事件；按 tick 聚合真实动作 bitmask；MI（signal_action / terrain_behavior / infection_behavior）改真实 joint count
2. 活体口径：`alive_ratio = alive/total`、`immune_ratio` 基于真实免疫状态、`spatial_entropy` 按 `world.size()` 动态分桶
3. 免疫状态机：感染时清 active immune；恢复时写 `immune_memory` 并同步 `immune_`；修 `advanceImmunity()` 双重乘 `immunity_gain_rate` 的隐蔽 bug
4. 交配闭环：`MateCapability` 改 typed payload；runtime 每代收集真实 mating agent pair → genome pair；`NeatEvolution / Nsga2Evolution` 用真实配对偏置第二亲本，无记录回退 tournament；堵上 coevo 中 predator/prey 角色交叉配对风险
5. 饮食分化：植物摄食 / 重资源采集 / 捕食能量收益按 `dietPreference` 调整；`FoodReward / CombatReward` 与 plant/meat intake 匹配挂钩；保留旧 `noteFoodEaten()` fallback
6. NSGA-II：补行为基因继承/变异；genome id 改真实递增，避免 child id 复用

短回归 4 代×60 tick：`alive_ratio 0.79~0.88`（不再恒 1）/ `immune_ratio 0.63~0.89`（不再恒 0）/ `action_entropy 1.56~1.97`（不再恒 0）/ `signal_action_mi 0.07~0.18`（真实非零耦合）/ `infection_behavior_mi 0.12~0.31`。

---

## 2026-05-19 · disease arms-race 统计断链修复

`module_metrics.csv` 里 `max_infected/infection_deaths/recoveries/pathogen_*` 持续非零，但 `comprehensive/disease_arms_race.csv` 全 0。

**根因**：`ModuleMetricsObserver` 按 tick 累积，`ComprehensiveMetrics` 旧实现只看 `GenerationEnded` 时刻的代末快照。infection_duration 远小于 episode ticks 时，代内活跃但代末已清零 → `avg_virulence/strain_count/infection_rate` 写成 0。

**修**：`ComprehensiveMetrics::onTickEnded()` 加 disease per-tick 累积（virulence/max virulence/immunity rate/immune memory/alive sample/infected sample/strain counts）；`onGenerationEnded()` 优先输出 tick 累积汇总，无样本回退旧快照。

教训：改 `core / runtime_headless` 后不能只重编 `test_core`，做 headless 验证前必须重链 `neural-eco.exe`。

---

## 2026-05-19 · Emergence Audit + F-COMM 调参

| 实验 | 结果 |
|---|---|
| F-LEARN: Darwin vs Lamarck (200代×3seed) | Lamarck 158.1 vs Darwin 80.7，**~2x 优势** |
| F-NOVELTY: on vs off (200代×3seed) | ON 1456.6 vs OFF 79.3，**18x 巨差** |
| F-COMM 默认 (100代) | `signal_behavior_mi_avg ≈ 0.07`（远低 0.3 阈值） |
| F-COMM `backprop_factor=0.5, window=40, emit_cost=0.15` (200代) | MI ≈ 0.11（仍低于 0.3） |

F-COMM MI 始终低于 0.3。可能原因：200 代不足；signal backprop reward 太弱；4 channel 太多难分化；需更长 episode / 更大 population。建议第二轮 `population_size=40, channels=2, backprop_factor=0.8`，并加直接的"信号引导觅食"奖励。

---

## 2026-05-18 · M1 验收基线（默认 vs 经验小世界，5 seed × 100 代）

文档示例数值（种群规模、tick 数、reward 权重、倍率目标）只算起始猜测；验收以真实数据为准。`FoodReward` 修正为按"吃到的食物个数"计 reward（不是按能量值），与之前实验绝对数值不能直接比。

**默认配置（`configs/default.toml`）**：5 seed × 100 代平均倍率 **2.256x**（42:1.68 / 43:3.18 / 44:2.76 / 45:1.91 / 46:1.76），peak best 2096~3101。曲线稳定上升但 seed 间波动明显。

**经验小世界（`m1_acceptance.toml`，80×80 / 24 个体 / 80 tick / `intrinsic_weight=1.5`）**：平均倍率 **1.929x**。能稳定上升但不如默认。适合 smoke / 快速调参，不适合作为正式验收配置。

**调参单 seed 快探**（降 intrinsic_weight 提 base_metabolism / 降 initial_energy / 降 survival 提 food）都没显著优于默认 — 不是只靠一两个权重就能解决。

---

## 2026-05-19 · M3 NEAT headless-first 首轮长跑 + 多 seed 验收

**实现**：`NodeGene / ConnectionGene / InnovationTracker / NeatGenome / CompatibilityDistance / Speciation / NeatBrain / NeatEvolution`；`HeadlessRuntime` 支持 `brain.type = "fixed"|"neat"`；`NeatBrain` 用 CTRNN 双缓冲积分支持递归连接；`evolution.csv` 加 `species_count / avg_complexity`。

**首轮 200 代单 seed**：avg_fitness 1.78x，species_count 峰值 8 但 107 代后塌缩到 1。根因：固定 `compatibility_threshold=3.0` 不会动态调；spawn 没给小物种保底名额。

**修**：`Speciation` 加动态 compatibility threshold（目标区间 3~10 物种，低于下调、高于上调）；`NeatEvolution` spawn 分配加每物种至少 1 个保底（物种数 ≤ 种群规模时）。

**修后 5 seed × 100/200 代**：100 代平均倍率 1.717x / 平均最终物种 6.4 / 平均最终复杂度 28.275；200 代 1.776x / 5.0 / 37.325。复杂度和物种数都能在长跑中维持，不再塌成 1 个物种。

**长跑 bug 修**：`addNodeMutation()` 持有 `ConnectionGene& split` 后对 `genome.connections()` `push_back` → vector 重分配 → 引用悬空 → 读到脏的 `split.to/weight`。改成先按值复制再修改原连接 + 追加。新增 `seed=44` 长跑 regression test。

教训：`compatibility_threshold` 不能当固定常数盲信；没有小物种保底 spawn 长跑后期容易塌缩；只看 `fitness.csv` 容易误判，`evolution.csv` 是关键可解释指标。

---

## 2026-05-19 · 全模块验证 + ACCESS_VIOLATION NaN 修复

**发现的 bug**：
1. **DiseaseLayer 初始感染失败**：`onAttach` 在 `populateWorld` 之前调用，AgentLayer 还空。修：延迟到第一个 tick 检查 `agent_layer->size() > 0`。
2. **ModuleMetricsObserver 只记录最终 tick 快照**：DAY/SEASON 的 light_level / season_index 在 episode 末恰好回到同一值。修：加 `onTick()` 每 tick 更新 min/max 极值。
3. **PredatorHuntCapability 阈值太高**：初始 NEAT 输出很少超 0.5，kills=0。修：阈值 0.5→0.3，damage 0.5→5.0，energy_gain 0.3→3.0。
4. `coop_required_workers` 3→2，`coop_required_ticks` 5→3，`coop_collect_radius` 5→8（24 agent 在 80×80 太难凑 3 人 / 连 5 tick）。

**ACCESS_VIOLATION 崩溃**（exit code -1073741819 = 0xC0000005，gen 14~92 不固定）：
- 根因：NEAT CTRNN 内部状态产生 NaN → agent position NaN → `HeightField::heightAt(NaN)` → `static_cast<size_t>(floor(NaN))` 极大值 → 越界
- Windows Event Log fault offset 始终 0x6db22，map 文件定位到 `HeightField::heightAt`
- 修（多层防护）：NeatBrain forward 内部状态 + 输出双重 NaN 检测；FixedTopologyBrain forward 输出 NaN 检测；MoveCapability direction NaN 检测；HeightField::heightAt 入口 NaN 检测 + gx/gy clamp；CaveMask::toGridX/toGridY 入口 NaN 检测

100 代 full_validation 验证：FIGHT/PRED/DISEASE/COMM/COOP/RESOURCE/DAY/SEASON/NEUROMOD 9 模块关键指标全部出现预期非零现象，机制闭环成立。

---

## 2026-05-19 · 病原体-宿主军备竞赛调参（红皇后）

实现病原体作为带 NN 的智能体，与宿主形成红皇后军备竞赛 + 抗原漂移。

**调参历程**：

| 问题 | 原因 | 修复 |
|---|---|---|
| 0 recoveries（宿主完全无法恢复） | Pathogen NN 输出范围太大：infection_speed [0.5,2.0]，evasion [0,0.8] | 收紧到 [0.3,1.2] / [0,0.3] |
| 仍 0 recoveries | PathogenGenome 初始权重 uniform(-1,1)，tanh 输出极端 | 改 normal(0,0.3)，初始温和 |
| ~1 recovery | infection_duration=20 太短，immunity_gain_rate=0.03 太慢 | duration→30，rate 默认→0.05，进化范围→[0.03,0.20] |
| 恢复后永久免疫无再感染 | `immune()` 是永久布尔 | 改 strain-based immune memory + 抗原漂移 |

**最终参数**：strain signature 4 floats in [0,1]⁴，mutation σ=0.1，immunity distance threshold 0.5（覆盖空间 ~6%，需约 17 个不同毒株免疫才能覆盖整空间）。

**100 代结果**：recoveries 854；宿主 immunity_gain_rate Q1→Q4 `0.0785→0.1040`（+33% 进化提升）；免疫人口 Q1→Q4 `3.4→5.4`；代中免疫人口下降 40%（新毒株突破旧免疫）；pathogen avg infection_speed 0.735 < 1.0（杀宿主太快不利于传播，毒力-传播权衡）。

**关键发现**：
1. 病原体 NN 初始输出映射范围是生死线 — 太宽宿主灭绝、太窄无进化压力
2. 抗原漂移是红皇后假说核心 — 没它免疫永久、疾病变成一次性事件
3. 病原体倾向温和演化（infection_speed < 1.0）
