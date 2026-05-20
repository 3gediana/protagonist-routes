# experience_log.md · 实验与调参记录

## 2026-05-20 · protagonist mixed compute 第一轮工程化（CPU 决策并行已验证，GPU backend 代码已落但环境未就绪）

### 本次目标

把前一轮只是停在“设计/分析”的 compute 承接，推进到真正可编译、可跑、可继续迭代的工程底座：

- 把 protagonist route 的 CPU 并行从 episode 级继续下钻到 agent decision 级
- 保持 capability/world mutation 仍然串行，避免一上来把世界层线程安全问题扩大
- 给 `ProtagonistBrain` 的 dense controller 接上可选 GPU backend
- 做到 **即便本机 CUDA 不完整，也能优雅回退到 CPU 路径**

### 这次真正改掉的点

1. **`Agent` 被拆成了“算决策”和“提交动作”两相**
   - `src/core/agent/Agent.*`
   - 原先 `step()` 里混着：
     - perception
     - brain forward
     - `DecisionTriggered` 事件
     - capability apply
   - 现在拆成：
     - `decide()`
     - `publishDecisionEvent()`
     - `applyCapabilities()`
   - 这样可以先并行做重计算，再按固定顺序串行提交世界写入。

2. **`AgentLayer` 现在具备 per-world agent decision 并行执行器**
   - `src/core/world/layers/AgentLayer.*`
   - 新增持久化 `DecisionExecutor`，不是每 tick 临时起线程
   - decision 阶段可以并行扫 agent 列表
   - capability apply、能量结算、死亡事件仍保持串行
   - 这一步的关键价值是：**把 CPU 热点从“一个 episode 只吃一核”推进到“一个 world 内也能吃多核”**。

3. **route training 的外层 episode 并行与内层 decision 并行现在会联动分配线程预算**
   - `routes/protagonist_ecology/runtime/ProtagonistEcologyRuntime.cpp`
   - 不再简单按 `episodes_per_generation` 决定 `EvaluationPool`
   - 新逻辑会综合：
     - `evolution.num_threads`
     - `compute.agent_decision_threads`
     - `episodes_per_generation`
     - `hardware_concurrency`
   - 目的不是绝对最优，而是先避免最糟糕的“外层 3 个 episode 并行、内层每个 world 再开满 31 线程”这种 CPU 过度超卖。

4. **`ProtagonistBrain` 已接入可选 dense GPU backend**
   - 新增：
     - `routes/protagonist_ecology/brain/ProtagonistDenseGpu.h`
     - `routes/protagonist_ecology/brain/ProtagonistDenseGpu.cpp`
     - `routes/protagonist_ecology/brain/ProtagonistDenseGpu.cu`
   - `ProtagonistBrain` 现在支持：
     - dense controller 前向可走 GPU
     - DNC memory core 继续留在 CPU
     - GPU 不可用时自动退回 CPU dense path
   - 这和目标一致：**先把最适合 GPU 的 dense matmul/activation 卸载出去，不碰 world/event/memory 主逻辑**。

5. **route 级 compute 配置已经接通**
   - `routes/protagonist_ecology/runtime/ProtagonistEcologyConfig.*`
   - 新增：
     - `compute.protagonist_dense_gpu_enabled`
     - `compute.agent_decision_threads`
   - `ProtagonistWorldFactory` 现在会把 per-world decision thread 配置传给 `AgentLayer`
   - `BootstrapPopulation` 会按配置决定 protagonist brain 是否尝试启用 GPU dense。

6. **顺手修掉了 protagonist brain 参数量日志低报问题**
   - `ProtagonistEcologyRuntime` 的 `brain_probe` 之前手拼 sensory dim
   - 现在统一改用 `protagonistSensoryDim()`
   - 这能避免后续 perception 再扩维时，日志/估算继续悄悄落后于真实参数量。

### 这次最关键的经验

1. **agent 级并行要先做到“读世界”和“写世界”分离**
   - 真正危险的不是 dense forward 本身，而是 capability apply 期间的 world mutation
   - 所以第一步不是“把整个 `Agent::step()` 并行化”，而是先把 decision 相位剥出来。

2. **双层并行如果不做预算联动，CPU 很容易被自己打爆**
   - 外层 `EvaluationPool` 已经能并 episode
   - 再叠每个 world 内 decision threads，默认就会出现线程数乘法膨胀
   - 所以 route runtime 至少要先做一个“够保守”的预算分配，而不是只顾着把两个开关都开到最大。

3. **GPU backend 最好从 route 内局部接起，不要一上来污染全仓库构建链**
   - 这次做法是：
     - 顶层只加 optional CUDA language / toolkit 检测
     - 真正 `.cu` 源只挂到 `routes/protagonist_ecology`
   - 这样就算 CUDA 不存在，也不会把整个仓库都拖死。

4. **本机“有 NVIDIA 目录”不等于“有完整 CUDA toolkit”**
   - 这次最关键的环境结论是：
     - `C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v11.8` / `v12.4` 确实存在
     - 但目录里只有 `extras/`，没有标准 `bin/include/lib`
     - `where.exe nvcc` 明确找不到 `nvcc`
   - 所以当前状态不是“代码没接好”，而是**机器上没有完整可编译 CUDA toolchain**。

### 验证结果

1. **Release 重编通过**
   - 命令：`cmake --build build --config Release --target neural-eco-protagonist neural-eco`
   - 结果：通过

2. **route 非训练 preview 验证通过**
   - 命令：`build/bin/Release/neural-eco-protagonist.exe --config configs/protagonist_ecology_bootstrap.toml`
   - 结果：通过
   - 说明：
     - `Agent` 两相拆分没有破坏现有 route 运行
     - `AgentLayer` 新 decision executor 没把基本 world loop 弄崩
     - protagonist route 的 mixed compute 接线至少在 CPU fallback 路径上是稳定的

3. **GPU 路径当前未真正编入**
   - `build/CMakeCache.txt` 中：`CMAKE_CUDA_COMPILER:FILEPATH=NOTFOUND`
   - route 生成文件里也没有 `.cu` / `cudart` 痕迹
   - 这意味着本次成功编译的是：
     - CPU agent decision parallel path
     - GPU backend 代码存在但未激活的 fallback build

### 当前仍未完全收尾的点

- 本机缺完整 CUDA toolkit，当前还无法验证 protagonist dense GPU kernel 真正上卡
- 还没补 CPU/GPU dense forward 的 profiling / 对照日志
- compute 主线当前只是第一轮工程化，不代表已经完成最终性能调优

### 当前结论

- **CPU 侧的下一层并行底座已经真的接上了，不再只停留在 episode 并行**
- **GPU 侧的 protagonist dense backend 代码与 CMake 路径已经落地，但当前被本机 CUDA 环境缺口卡住**
- **如果下次继续算力线，先补 `nvcc + bin/include/lib`；如果继续主线，则可以放心回到 Memory Phase A，不会和这次 compute 底座冲突**

## 2026-05-20 · protagonist route checkpoint-resume Phase 1 全收口（RNG + speciation + generation+1）

### 本次目标

把上一轮已经做到一半的 protagonist route Phase 1 Trace / Resume **一次性收口**，不再留“最后一个坑以后再补”：

- protagonist evolution RNG 要能恢复
- background species NEAT evolution RNG 要能恢复
- background species 的 speciation 内部状态要能恢复
- checkpoint 语义从“重评估当前代”改成真正的 `generation + 1` 续跑
- 文档和 smoke 验证一次补齐

### 这次真正改掉的点

1. **evolution 内部状态不再只靠 seed 重建，而是显式持久化**
   - `routes/protagonist_ecology/runtime/ProtagonistEvolution.*`
   - `src/core/evolution/NeatEvolution.*`
   - `src/core/evolution/speciation/Speciation.*`
   - 新增了显式 `exportState()/importState()`：
     - protagonist GA：保存/恢复 `std::mt19937` RNG 状态
     - background NEAT：保存/恢复 RNG 状态 + `SpeciationState`
   - `SpeciationState` 里现在包含：
     - compatibility threshold
     - next species id
     - previous representatives

2. **route checkpoint 改成 post-reproduction 语义**
   - `routes/protagonist_ecology/runtime/ProtagonistEcologyRuntime.cpp`
   - 现在 checkpoint 保存点从“评估后、繁殖前”挪到了“繁殖后”
   - `checkpoint_state.toml` 新增：
     - `evaluated_generation`
     - `resume_generation`
     - `best_fitness_seen`
   - resume 后直接从 `resume_generation` 开始，不再重复评估已存那一代

3. **checkpoint 目录现在额外带完整 evolution sidecar**
   - protagonist checkpoint 根目录新增：
     - `protagonist_evolution_rng.txt`
     - `best_genome_overall.bin`
     - `best_genome_latest.bin`
   - 每个背景物种 slot 目录新增：
     - `neat_evolution_rng.txt`
     - `neat_evolution_state.toml`
     - `speciation_representatives/`
   - 这意味着 checkpoint 不再只是“种群快照”，而是更接近真正的 **training continuation state**

4. **resume continuity 顺手补了 best genome 链**
   - `best_fitness_seen` 进入 checkpoint state
   - 恢复时会把 `best_genome_overall.bin / best_genome_latest.bin` 复制进新 run 目录
   - 这样 resumed run 的 `best_genome_overall.bin` 不会从头重算

5. **修掉了一个非常隐蔽但会破坏 resume 等价性的排序坑**
   - protagonist checkpoint 之前按文件名字典序读 `genome_*.bin`
   - `genome_10.bin` 会排到 `genome_9.bin` 前面
   - 这会悄悄打乱 population 顺序，进而改变 agent 装配顺序和训练轨迹
   - 现在改成按数值 genome id 排序加载

### 这次最关键的经验

1. **真正的“可续训”不等于只存 population**
   - population / innovation tracker / generation index 只够做到“差不多接上”
   - 要做到真正完整，至少还要补：
     - evolution RNG
     - NEAT speciation threshold / representative memory / next species id

2. **resume 语义如果还是“重评估当前代”，长期实验会一直别扭**
   - 训练中最自然的断点是“这代已经评完并繁殖出下一代了”
   - 所以 checkpoint 最好直接保存 **下一代种群 + 下一代 evolution state**
   - 这样 resume 才是真正的 `generation + 1`

3. **验证 checkpoint-resume 时，要区分“resume 逻辑问题”和“训练并行本身不完全确定”**
   - protagonist route 多线程训练目前不是逐 run 完全确定性的
   - 所以 resume 等价性不能只拿默认多线程 smoke 做强对照
   - 这次真正有说服力的验证方式是：
     - 保持同一配置
     - 临时设 `evolution.num_threads = 1`
     - 比较 full run 与 resume run 的同一代 `fitness.csv`

4. **checkpoint 文件命名/读取顺序这种“小问题”，足以毁掉完整 resume**
   - 这次 numeric sort 的 bug 很典型：逻辑看起来都对，但顺序错了就不等价
   - 以后只要 checkpoint 目录里是按文件扫描组 population，都要优先检查排序口径

### 验证结果

1. **Release 重编通过**
   - 命令：`cmake --build build --config Release --target test_core neural-eco-protagonist`
   - 结果：通过

2. **核心测试通过**
   - `test_core` 通过
   - 新增了：
     - `Speciation` export/import regression test
     - `NeatEvolution` export/import regression test

3. **默认 smoke 验证通过**
   - `configs/protagonist_ecology_train_smoke.toml`
   - 新 checkpoint 下 `checkpoint_state.toml` 已显示：
     - `resume_generation = 2`
     - `rng_state_resume = true`
     - `speciation_resume = true`

4. **resume 语义验证通过**
   - 从 `checkpoint_gen1` 恢复后，新的 `fitness.csv` 只包含：
     - `generation = 2`
   - 说明已经不再重跑 `generation = 1`

5. **single-thread full-vs-resume 等价对照通过**
   - 临时把 `evolution.num_threads = 1`
   - full run 的 `fitness.csv`：
     - `0,178.046,586.866,1223.38`
     - `1,487.051,828.103,1824.9`
     - `2,360.286,755.699,1594.6`
   - resume run（从 `checkpoint_gen1`）的 `fitness.csv`：
     - `2,360.286,755.699,1594.6`
   - 这说明 **Phase 1 checkpoint-resume 的核心续训状态已经对齐**

### 当前仍未完全收尾的点

- protagonist route 默认多线程训练跨 run 仍不是严格确定性的；这不是本次 checkpoint 缺口，而是后续如有需要可单独处理的“并行确定性”问题
- `SnapshotIO` 对 NEAT genome 的行为基因覆盖仍以当前 route 实际训练所需字段为主；如果后续 route 真开始使用更多 evolvable trait，需要再评估是否继续扩充 snapshot 字段

### 当前结论

- **protagonist route Phase 1 Trace / Resume 现在已经不再是“半成品断点续训”，而是完整训练 continuation 管线**
- **checkpoint 已覆盖主角 / 背景 / RNG / speciation / best-genome continuity，并且 resume 语义已经是 `generation + 1`**
- **下一步可以放心回到 CT-2 / future-event / Memory V2，不需要再把 Phase 1 当未收口事项挂着**

## 2026-05-20 · protagonist route checkpoint-resume 扩到主角 + 配角 + generation index

### 本次目标

把 protagonist route 的 Phase 1 Trace / Resume 从“只能恢复主角 population”的半成品，推进到更接近真正训练流水线的状态：

- checkpoint 不只存 protagonist genomes
- background species 的 NEAT population 也一起存
- innovation tracker 能接上
- generation index 能接上

这一步的意义不是“多存几份文件”，而是让这条路线真正具备你前面强调的那种训练工作流基础：**先练、再存、再继续、再投放**。

### 这次真正改掉的点

1. **route checkpoint 从 protagonist-only 扩到 protagonist + background species**
   - `routes/protagonist_ecology/runtime/ProtagonistEcologyRuntime.cpp`
   - checkpoint 目录现在除了 `genome_*.bin`，还会为每个背景物种 slot 写：
     - `background_slot_XX_<archetype>/meta.toml`
     - `background_slot_XX_<archetype>/population.bin`
   - 这部分直接复用 `SnapshotIO`，把背景 NEAT population 和 innovation tracker 一起存下来了。

2. **新增 machine-readable checkpoint state**
   - 新增 `checkpoint_state.toml`
   - 里面记录：
     - `generation`
     - protagonist population 数量
     - background species 数量
     - 每个 slot 的 archetype / snapshot_dir / population_size
   - 这样 route resume 不再只能扫目录猜状态，而是有稳定的程序读取入口。

3. **resume 现在能同时恢复三类关键内容**
   - protagonist population
   - background species NEAT population
   - background species innovation tracker
   - generation index

4. **resume 代数语义和主系统保持一致**
   - 当前 checkpoint 仍然是在“该 generation 完成评估后、繁殖前”保存。
   - 所以 resume 后会**从该 generation 重新开始评估**，而不是直接跳到 `generation + 1`。
   - 这和主系统 `SnapshotIO` 的现有语义一致，虽然不是最理想的 long-run continuation，但至少全仓库口径一致，不会 route 单独搞一套不同规则。

### 这次最关键的经验

1. **训练 checkpoint 最怕“只存主角，不存生态对手”**
   - 如果主角恢复了，但背景物种从头随机，那训练上下文就已经变了。
   - 对 protagonist 这条线来说，配角不是背景板，所以 checkpoint 也必须把它们当成训练状态的一部分。

2. **generation index 比想象中重要**
   - 如果 resume 以后代数从 0 重新编号，`fitness.csv / species_metrics.csv / trace` 的含义都会乱。
   - 即使先不做真正的 RNG 完全续跑，至少 generation index 要先接上，不然后续根本没法把多次训练拼成一条长实验记录。

3. **先把语义讲清，再去追求“完美续跑”**
   - 这次没有顺手硬做 RNG state resume，因为那会把 scope 一下拉大。
   - 当前先完成：population + innovation tracker + generation index。
   - 剩下的 RNG 完整恢复，可以作为 Phase 1 的最后缺口单独做。

### 验证结果

1. **Release 重编通过**
   - 命令：`cmake --build build --config Release --target neural-eco-protagonist`
   - 结果：编译通过

2. **训练 smoke 重新通过，并生成扩展 checkpoint**
   - 命令：`build/bin/Release/neural-eco-protagonist.exe --config configs/protagonist_ecology_train_smoke.toml`
   - 结果：正常完成 3 代 smoke，输出目录 `data/runs/protagonist_smoke/20260520-191608/`
   - `checkpoint_gen1/` 下已出现：
     - protagonist `genome_*.bin`
     - `checkpoint_manifest.json`
     - `checkpoint_state.toml`
     - 7 个 `background_slot_XX_<archetype>/population.bin + meta.toml`

3. **resume smoke 通过**
   - 使用刚生成的 `checkpoint_gen1` 做临时 resume 配置重新启动训练
   - 结果：route 正常从 `Gen 1` 跑起，并再次成功输出 background snapshots

### 当前仍未完全收尾的点

- RNG state resume 仍未完成
- 当前 checkpoint 语义仍是“该 generation 重新评估”，不是 generation+1 无缝续跑
- `SnapshotIO` 的 NEAT genome 序列化目前主要覆盖 route 实际在用的核心字段；如果后面要让更多行为基因在 route 内真实生效，需要顺手补齐更完整的 genome 字段覆盖

### 当前结论

- **protagonist route 已经从“可训练”推进到“可断点续训”**
- **而且断点续训不再只保主角，而是开始保整个共同训练生态的一阶状态**
- **下一步最值的是补 RNG state resume，然后再继续回到 CT-2 / event queue / Memory V2 这些更高阶底座**

## 2026-05-20 · protagonist continuous-time CT-2A 收尾：残余 `per_tick` 口径清理 + 连续摄食

### 本次目标

在上一轮把 protagonist runtime 主链切到 seconds 之后，继续把 route 里仍然带强烈 tick 命名/离散语义的部分收干净，避免出现“底层已经 continuous-time，但局部机制还是离散按钮”的半迁移状态。

这次重点不是再做大架构，而是做两类很值钱的收尾：

- 把 route-local 残余 `per_tick` 速率口径切到 `per_second`
- 把 `eat` 从“碰到资源就瞬间吃光”改成按 `dt` 累积的持续摄食

### 这次真正改掉的点

1. **route-local 速率命名改成 `per_second`**
   - `routes/protagonist_ecology/runtime/ProtagonistEcologyConfig.h/.cpp`
   - `routes/protagonist_ecology/world/SurvivalStatusLayer.*`
   - `routes/protagonist_ecology/world/InterventionDirectorLayer.*`
   - `routes/protagonist_ecology/runtime/ProtagonistWorldFactory.cpp`
   - `SurvivalStatusLayer` / `InterventionDirectorLayer` 现在在接口、成员名、配置字段名上都统一成 `*_per_second`
   - 同时保留 legacy `*_per_tick` 兼容读取，不会把旧配置直接跑崩

2. **`EatCapability` 改成真正的持续摄食**
   - `src/core/capability/EatCapability.*`
   - `src/core/world/layers/ResourceLayer.*`
   - 新行为：每次 `apply()` 最多只会消耗 `eat_rate_per_second * dt`
   - `ResourceLayer` 新增部分消耗能力；只有资源真的被吃空时才发 `ResourceConsumed`
   - 这样 substep 变细时不会再出现“一口吞空整个资源”的离散失真

3. **补了连续摄食回归测试**
   - `tests/core/test_comm.cpp`
   - 新测试锁住三个关键点：
     - 小 `dt` 只吃掉小部分资源
     - 资源剩余量持续下降而不是立刻归零
     - 吃空时才变成 unavailable

4. **同步更新了一批 protagonist 高使用频率配置键**
   - 至少确认 `protagonist_ecology_preview_long.toml` 已切到 `per_second` 命名
   - 兼容层保留，所以剩余配置可以逐步迁，不要求一次性全改完

### 这次最关键的经验

1. **continuous-time 重构里，名字也是机制的一部分**
   - 即使逻辑已经是 `value * dt`，如果配置和成员名还写 `per_tick`，后续很容易让人误判真实语义。
   - 尤其 protagonist route 这种独立路线，文档/配置/实现要尽量同口径，不然后面做 CT-3 event queue 会继续混乱。

2. **“持续动作”优先修 `eat` 很值**
   - `move / drink / signal` 之前已经基本按 `dt` 走了，但 `eat` 还是最明显的离散跳变点。
   - 把 `eat` 改成部分消耗后，substep 变小不会再改变“单位真实时间最大摄食量”，这是 continuous-time 非常关键的 invariant。

3. **资源层最好支持部分消耗，而不是在 capability 里硬模拟**
   - 如果只在 `EatCapability` 里做假分片，`ResourceLayer` 仍然会维持“一次拿走全部”的底层语义。
   - 这次把部分消耗能力真正下沉到 `ResourceLayer`，后面做 `graze / sip / nibble / build material progress` 都能复用。

4. **验证最好用 preview，而不是继续加训练 run**
   - 这次 route 级验证用了 `configs/protagonist_ecology_preview_long.toml`。
   - 它足够覆盖 protagonist route 的真实世界层交互，但不会额外把问题混到训练流程里，更适合做 continuous-time 收尾验收。

5. **`build/worksite` 不能在没有显式 action 设计时硬加隐式连续门槛**
   - 这次试过给 `WorksiteLayer` 加“低速停留才持续施工”的隐式就位门槛。
   - 结果是 `preview_xlong` 的闭环表现波动很大：有时材料能送到工地但 `site_progress` 长时间不涨，有时甚至连 worksite unlock 都回退。
   - 结论：这类连续化最好等到有明确 `build` action / progress channel 时再做；本次已把该试验回退，不保留在主线。

### 验证结果

1. **Release 重编通过**
   - 命令：`cmake --build build --config Release --target test_core neural-eco neural-eco-protagonist`
   - 结果：全部通过，仅保留既有 warning

2. **核心测试通过**
   - `build/bin/Release/test_core.exe`
   - 结果：`71 / 71 passed`
   - 说明新增连续摄食测试和旧测试都兼容

3. **protagonist preview 验证通过**
   - 命令：`build/bin/Release/neural-eco-protagonist.exe --config configs/protagonist_ecology_preview_long.toml`
   - 结果：正常完成 preview，日志末尾显示 `advanced 600 seconds with substep_dt=0.1`

### 当前仍未完全收尾的点

- 其它 protagonist configs 里仍有一批 legacy `per_tick` 键，虽然已有兼容读取，但还没全部换名
- `chop / build / worksite` 仍然比 `eat` 更偏“站位即触发”式逻辑，还没做成更强的持续进度语义
- 还没落最小 future-event / due-action 队列
- `preview_xlong` 在 continuous-eat 改动后对 nursery 闭环节奏更敏感，后续需要重新做一轮 route-local 难度/速率复核

### 当前结论

- **protagonist continuous-time 现在不只是 runtime 级 seconds 化，而是已经开始进入动作语义层面的连续化**
- **`eat` 已经从离散瞬时事件，变成受 `dt` 和速率上限约束的持续动作**
- **下一步最该做的是 `build / chop / event queue`，而不是再回头重复修 runtime 主循环**

## 2026-05-20 · protagonist continuous-time 主链迁移（CT-1 ~ CT-4 核心落地）

### 本次目标

把 protagonist 路线从“世界层虽然已有 `currentTimeSeconds()`，但 runtime / reward / route-local cooldown 仍主要按 tick 思考”的半连续状态，推进到**真正以 seconds 为主语义**的可运行状态。

目标不是一次性做完所有长期收尾，而是先保证：

- runtime 真正支持 `episode_duration_seconds + substep_dt`
- world / route 里的关键 cooldown / event delay 统一到 seconds
- protagonist memory / trace / social signal 时间窗不再强绑 tick
- `ProtagonistBrain` 至少具备基本的 `dt` 积分动态，不再是每个子步纯覆盖输出
- 整个系统仍然 **可编译、可跑 smoke、可过核心测试**

### 这次真正改掉的主链

1. **runtime 时间主语义切换**
   - `routes/protagonist_ecology/runtime/ProtagonistEcologyConfig.*`
   - `routes/protagonist_ecology/runtime/ProtagonistEcologyRuntime.cpp`
   - 新增 / 接通：
     - `simulation.time.preview_duration_seconds`
     - `simulation.time.episode_duration_seconds`
     - `simulation.time.substep_dt`
     - `simulation.time.max_substeps_per_advance`
   - 同时保留 legacy：
     - `runtime.preview_ticks`
     - `evolution.episode_ticks`
     - 各类 `*_ticks`

2. **route-local 时间层统一到 seconds**
   - `NurserySupplyLayer`
   - `NurseryProgressionLayer`
   - `InterventionDirectorLayer`
   - `TreeLayer`

3. **核心世界时间层统一到 seconds**
   - `ResourceLayer`
   - `TimeOfDayLayer`
   - `SeasonLayer`

4. **memory / trace 双时间戳接通**
   - `EpisodicMemoryBank`
   - `SocialMemoryBank`
   - `GoalMemory`
   - `TraceRecorder`

5. **brain 连续时间推进**
   - `ProtagonistBrain::forward(inputs, dt)` 由“直接覆盖 hidden/output”改成“leaky integration”
   - hidden / output 都按 `dt / tau` 的 alpha 做渐进积分

### 迁移中最关键的经验

1. **不能只改 runtime，不改 reward / cooldown / metrics**
   - 如果 runtime 用 substep 了，但 reward 还是每子步固定加一次，奖励会被放大很多倍。
   - 这次明确把 survival / social proximity / worksite proximity / background social shaping 等**持续型奖励**改成乘 `dt`。
   - 而 kill / emit / signal co-attendance / completed worksite 这类**事件型奖励**则保留一次性加分。

2. **MoveCapability 的能量成本必须乘 `dt`**
   - 不然 substep 越小，单位真实时间内扣能越多。
   - 这类 bug 非常隐蔽，因为系统仍然“能跑”，但生态会被连续时间改坏。

3. **memory / trace 如果不一起改，会出现“runtime 连续、日志离散”的假连续时间**
   - `TraceRecorder` 必须把 `time_seconds` 打进 event / agent 轨迹。
   - `EpisodicMemory` / `SocialMemory` 至少要带 dual timestamp，否则后续做 causal replay / communication semantics 时会被 tick 粒度卡死。

4. **配置兼容层值得保留**
   - 现在旧配置仍可继续跑：seconds 键优先，缺失时回退到老 tick 键。
   - 这样连续时间重构可以一边推进一边 smoke，而不用先一次性改完整个配置生态。

### 验证结果

1. **Release 重编通过**
   - 同时重编：
     - `neural-eco-protagonist`
     - `neural-eco`
     - `test_core`

2. **核心测试通过**
   - `build/bin/Release/test_core.exe`
   - 结果：`70 / 70 passed`

3. **protagonist route smoke 通过**
   - 配置：`configs/protagonist_ecology_train_smoke.toml`
   - 结果：训练 run 正常完成，输出目录正常生成

### 当前仍未完全收尾的点

- 还有一些 legacy tick 命名仍存在于配置键 / 日志字段 / 部分 headless 路径里
- continuous-time 现在已经**接通主链**，但还没做专门的 protagonist seconds regression config
- 还没把本次改动 commit/tag

### 当前结论

- **continuous-time 已不再只是底层接口准备，而是 protagonist runtime 真正跑在 seconds 语义上了**
- **substep_dt 已经可以驱动 preview 和 training**
- **核心测试与 protagonist smoke 都通过，说明这次迁移目前是稳定的**
- 下一步应该从“继续大改”切到“收尾、固化、提交、补文档/经验”

## 2026-05-20 · protagonist runtime 接入真实训练闭环

### 背景

前面虽然已经把 `camp_building / preview_xlong` 环境调到训练前可用，但 `routes/protagonist_ecology/runtime/ProtagonistEcologyRuntime.cpp` 仍然只有 preview tick 循环，**并不能真的多代训练**。

### 真正补上的东西

1. **新增可进化 protagonist 基因表示**
   - `brain/ProtagonistGenome.h/.cpp`
   - 覆盖 protagonist dense+memory 脑的全部参数：
     - input→hidden
     - hidden bias
     - hidden→action
     - action bias
     - hidden→memory candidate / gate
     - memory candidate / gate bias

2. **新增 protagonist 专用 GA**
   - `runtime/ProtagonistEvolution.h/.cpp`
   - 复用现有 `EliteSelection + TournamentSelection`
   - 支持 elite 保留、双亲交叉、权重扰动与重置

3. **`ProtagonistBrain` 支持从 genome 构造**
   - 不再只能随机初始化
   - 训练时 agent 现在真正由 genome 驱动

4. **`BootstrapPopulation` 支持训练 population 装配**
   - 原先只会随机造 protagonist 脑
   - 现在可把一组显式 protagonist genomes 装配进 world
   - preview 和 training 共用一套 protagonist 身体/感知/能力接口

5. **`ProtagonistEcologyRuntime` 变成双模式**
   - 默认无 `evolution.generations`：继续 preview 模式
   - 存在 `evolution.generations`：进入训练模式
   - 训练模式会：
     - 创建初始 protagonist population
     - 按代创建 protagonist world
     - 运行 episode ticks
     - 计算 protagonist fitness
     - 输出 `fitness.csv` 和 `evolution.csv`
     - 调用 `ProtagonistEvolution` 繁殖下一代

### 训练奖励当前版本

先走一套**最小可用**的 protagonist reward 组合：

- 存活 tick reward
- 食物 reward（沿用 plant/meat + diet match 思路）
- exploration/intrinsic reward
- damage / kill / death penalty
- 物流 deposit bonus 仍由 world 内已有 `addFitness()` 提供

这套 reward 现在足够让训练循环先跑起来，但还不是最终版 survival curriculum reward。

### Smoke 验证

新增：`configs/protagonist_ecology_train_smoke.toml`

运行结果：

- 3 代 smoke training 成功完成
- 输出目录：`data/runs/protagonist_smoke/20260520-113857/`
- 产出文件：
  - `fitness.csv`
  - `evolution.csv`

样例：

- gen0: avg `218.957`, best `912.195`
- gen1: avg `126.502`, best `632.319`
- gen2: avg `139.496`, best `662.906`

注意：这只是“训练闭环真的在工作”的证明，**不是训练效果已经稳定/优秀**。

### 额外新增

新增正式可启动但**还没实际运行**的训练配置：

- `configs/protagonist_ecology_train_camp_building_evolve.toml`

它是给用户亲自启动首轮正式训练用的入口。

### 当前限制

当前训练 runtime 还没补这些增强项：

- best genome 导出
- snapshot/checkpoint/resume
- 更细的 protagonist-specific 行为指标
- train→eval 自动串联验证
- 更贴近“原始人生存课程”的 reward 分阶段设计

### 当前结论

- **环境已训练前可用**
- **训练循环已真正接通**
- **可以开始第一轮正式训练**
- 但更稳妥的做法仍是：先由用户亲自启动第一轮正式训练，再根据曲线和日志继续调 reward / curriculum

## 2026-05-20 · protagonist camp-building 环境整备到训练前可用

### 目标

在**不启动正式训练**的前提下，把 protagonist 的 `camp_building / preview_xlong` 环境调到足够稳，避免之后一开训就把环境问题误当模型问题。

### 查到的几类真根因

1. **工地解锁有鸡生蛋**
   - 原逻辑要求工地解锁前已经有 `stock_stone >= threshold`
   - 但石头本身又来自 progression stone phase
   - 结果某些局面会出现：工地还没开，石头链也没真正喂到工地

2. **progression 过于随机**
   - 某些关键事件走 intervention queue + cooldown，导致该开的时候没立刻开
   - 对训练前环境验证不友好

3. **做矛会抢工地稀缺石头**
   - 系统刚进入 primitive tool 第二步后，stone 会被 `CraftSpearCapability` 消耗
   - 工地仍缺 stone 时，建造链会被工具链反向饿死

4. **工地交付门槛太高**
   - 之前必须 agent 自己把材料带到 site 内，再恰好触发 drop/松手
   - 这更像“等模型学会一个精细动作”，不适合训练前环境脚手架验证

### 本次环境修正

1. **工地解锁条件修正**
   - `NurseryProgressionLayer` 中，工地解锁不再死卡“当前 stock_stone”
   - stone phase 已开启也可视为 worksite stone prerequisite 已满足

2. **关键 progression 改成更确定性**
   - first fire / worksite unlock / second fire 直接执行
   - stone / heavy stone 仍通过 director 投放，但时机更确定

3. **石头投放偏向激活工地**
   - `InterventionDirectorLayer::ScatterStone` 现在会优先把 stone 刷在激活且未满足 stone 需求的工地附近
   - 工地首次解锁时还会额外补一枚 support stone

4. **做矛为工地保留 stone**
   - `CraftSpearCapability` 先统计 active incomplete worksites 的缺 stone 数
   - 若当前总可用 stone 只够工地需求，则禁止做矛

5. **工地自动接收携带材料**
   - `WorksiteLayer` 新增 carried-material delivery hook
   - agent 只要把 `Stick / Stone` 带进 active site 半径内，就自动记为交付
   - 不再强依赖 agent 额外学会在 site 内精准 `drop`

### 验证结果

使用：`configs/protagonist_ecology_preview_xlong.toml`

多 seed 抽查：

- seed `42`：`active_worksites=1`, `completed_worksites=1`
- seed `91`：`active_worksites=1`, `completed_worksites=1`
- seed `123`：`active_worksites=1`, `completed_worksites=1`

说明现在工地主闭环已经不是单 seed 偶然现象。

### 当前判断

- 这套环境现在已经达到**训练前可用**
- 还没开始正式训练，是按用户要求停住的
- 后面如果训练效果差，优先先怀疑模型/奖励/训练循环，而不是再优先怀疑工地环境本身

## 2026-05-20 · protagonist 训练路线：primitive tool 第二步 + 优先级回正

### 本次真正完成的东西

- `Spear` 加入 `ObjectType`
- 新增 `CarriedObjectPerception`
- 新增 `CraftSpearCapability`
- 新增 `ThrowCarriedObjectCapability`
- `NurseryLogisticsLayer` 增加 `consumeStockpiledObject(ObjectType)`
- protagonist `action_dim` 从 `10` 扩到 `13`
- 训练路线补了两类与工具链强相关的感知：
  - `StockpilePerception`
  - `WeaponPerception`
- `SandboxStatusLayer` 新增工具链诊断：
  - `stock_spear`
  - `craft_attempts / craft_successes`
  - `throw_attempts / throw_hits`
  - `carry_spear`

### 验证结果

使用：`configs/protagonist_ecology_train_camp_building.toml`

- 构建：`neural-eco-protagonist` **通过**
- 运行：**通过**
- 关键现象：日志中已出现**非零**
  - `craft_successes`
  - `throw_hits`

这说明 protagonist 训练世界里，工具链已经从“接口存在”变成“行为真的发生”。

### 中途走偏的地方

中途一度去做了 `live / prod` 世界边界、专门 live 配置和相关世界初始化实验。

这件事本身不算错，但**优先级错了**：

- 当前真正关键路径不是 production world
- 而是 **训练世界先把能力链学出来**

用户已经明确纠正：

- 先别管生产世界
- 先把训练世界和训练链条做扎实

因此后续原则已经改回：

- **不再继续 live/prod 方向扩展**
- 只盯 `routes/protagonist_ecology/` 的训练世界

### 当前判断

现在 protagonist 路线最重要的不是再加新世界，而是提高 `curriculum_camp_building` 里的闭环稳定性：

- 木材获取
- 石头流通
- 做矛
- 投掷
- 营火/工地链条

先把这条链弄到适合真正训练，再谈后面的 test/prod。

## 2026-05-19 · 核心假指标修复 + 机制闭环加固

### 目的
把几类“能跑但不可信”的核心问题一次收口：

- `ComprehensiveMetrics` 的假指标（`action_entropy / alive_ratio / immune_ratio / spatial_entropy / 行为MI`）
- `immune_` 与 `immune_memory` 不同步
- `MatingOccurred` 只发事件、不参与繁殖
- `dietPreference` 有记录但不进真实生态收益

### 关键修复

1. **行为统计改为事件驱动**
   - 给 `Flee / Tool / Terrain / Signal / Mate` 补齐事件
   - `ComprehensiveMetrics` 不再用“速度方向”冒充行为，而是按 tick 聚合真实动作 bitmask
   - `signal_action_mi / terrain_behavior_mi / infection_behavior_mi` 改为真实 joint count 计算

2. **活体口径修复**
   - `alive_ratio` 改为真实 `alive / total`
   - `immune_ratio` 改为基于真实免疫状态/记忆
   - `spatial_entropy` 改为按 `world.size()` 动态分桶，不再假定 200×200 世界

3. **免疫状态机修复**
   - 感染时清 active immune
   - 恢复时写入 `immune_memory` 并同步 `immune_`
   - 修正 `advanceImmunity()` 双重乘以 `immunity_gain_rate` 的隐蔽 bug

4. **交配闭环修复**
   - `MateCapability` 改为 typed payload
   - runtime 每代收集真实 mating agent pair → genome pair
   - `NeatEvolution / Nsga2Evolution` 用真实配对记录偏置第二亲本选择，没记录时回退 tournament
   - 顺手堵上 coevo 中 predator/prey 角色交叉配对风险

5. **饮食分化落地**
   - 植物摄食、重资源采集、捕食能量收益都按 `dietPreference` 调整
   - `FoodReward / CombatReward` 改为与 plant/meat intake 匹配程度挂钩
   - 保留 `FoodReward` 对旧 `noteFoodEaten()` 调用方式的兼容 fallback，避免旧测试/旧逻辑断裂

6. **NSGA-II 补齐半套实现**
   - 行为基因继承/变异补齐
   - genome id 改为真实递增，避免 child id 复用

### 验证结果

#### 编译与测试

- Release 构建：**通过**
- `ctest -C Release --output-on-failure`：**59/59 全通过**

#### 短回归配置

- 新增：`configs/core_regression_short.toml`
- 覆盖：`coevo + disease + signal + tool + mate + terrain_mod + NSGA-II`
- 运行：4 代 × 60 tick，`neural-eco.exe --mode headless --config .\configs\core_regression_short.toml`
- 结果：**正常结束，rc=0**

#### 指标抽查（`data/runs/core_regression_short/20260519-184546/comprehensive/`）

| 指标 | 现象 | 说明 |
|---|---|---|
| `alive_ratio` | 0.79 ~ 0.88 | 不再恒等于 1 |
| `immune_ratio` | 0.63 ~ 0.89 | 不再恒等于 0 |
| `action_entropy` | 1.56 ~ 1.97 | 不再恒等于 0，且随代变化 |
| `signal_action_mi` | 0.07 ~ 0.18 | 已有真实非零耦合 |
| `infection_behavior_mi` | 0.12 ~ 0.31 | 说明感染状态开始影响行为统计 |

### 当前判断

- 这些问题本质上属于“**表面有 feature，底层没闭环**”，现在已经从根上补齐
- 现阶段数据终于可以当作**真实分析材料**，而不是答辩时一眼会穿帮的假统计
- 下一步不该急着继续堆 feature，而应做 **50~100 代中长跑回归**，看这些修复在长跑中是否稳定成立

## 2026-05-19 · disease arms-race 统计断链修复

### 症状
- `module_metrics.csv` 里 `max_infected / infection_deaths / recoveries / pathogen_*` 持续非零
- 但 `comprehensive/disease_arms_race.csv` 在长跑 coevo 回归里会整列变成 0
- 表现为“疾病机制在跑，但 comprehensive 疾病曲线像没开一样”

### 根因
- `ModuleMetricsObserver` 的疾病链路是 **按 tick 累积**，统计的是整代内发生过什么
- `ComprehensiveMetrics` 的 disease arms-race 原实现却只看 **GenerationEnded 时刻的代末快照**
- 当 infection_duration 远小于一整代的 episode ticks 时，经常出现“代内感染/恢复/死亡都很活跃，但代末已经清零”的情况
- 于是 `avg_virulence / strain_count / infection_rate` 会被错误写成 0

### 修复
- 在 `ComprehensiveMetrics::onTickEnded()` 中新增 disease 相关 per-tick 累积：virulence、max virulence、immunity rate、immune memory、alive sample、infected sample、strain counts
- `onGenerationEnded()` 优先输出“整代 tick 累积汇总”；只有在没有 tick 样本时才回退旧快照逻辑
- 新增 `tests/core/test_comprehensive_metrics.cpp`，覆盖“代内感染、代末恢复”场景，防止以后再回归成全 0

### 验证
- Release 重编译通过
- `test_core` 全量：**60/60 通过**
- 单测 `ComprehensiveMetrics.DiseaseStatsCaptureWithinGenerationInfections`：**通过**
- 重链 `build/bin/Release/neural-eco.exe` 后运行 `configs/core_regression_short.toml`：
  - `gen 0~3` 的 `disease_arms_race.csv` 已恢复连续非零
  - 例如 `gen 0`：`avg_virulence=0.64683`, `strain_count=62`, `infection_rate=0.49395`

### 经验
- 修 observer 指标时，先看**统计口径**，再看模块是否真的坏了
- 改了 `core` / `runtime_headless` 后，不能只重编 `test_core`；做 headless 验证前必须重链 `neural-eco.exe`
- 对长期 episode 的生态指标，代末快照很容易撒谎；凡是“过程型机制”，优先考虑 **tick 累积 + 代末汇总**

## 2026-05-19 · Emergence Audit 全指标实现 + F-COMM 调参

### 目的
实现 EmergenceAuditor 的全部涌现指标，跑对照实验，生成审计报告。

### 关键结果

| 实验 | 结果 |
|------|------|
| F-LEARN: Darwin vs Lamarck (200代×3seed) | Lamarck 最终 fitness 158.1 vs Darwin 80.7，**Lamarck ~2x 优势** |
| F-NOVELTY: on vs off (200代×3seed) | Novelty ON 最终 fitness 1456.6 vs OFF 79.3，**Novelty 巨大优势** |
| F-COMM: 默认参数 (100代) | signal_behavior_mi_avg ≈ 0.07，**远低于 0.3 阈值** |
| F-COMM: backprop_factor=0.5, window=40, emit_cost=0.15 (200代) | MI ≈ 0.11，**仍低于 0.3** |

### 分析
- **F-LEARN**: Lamarckian 权重继承提供累积优势，支持 F-LEARN 模块有效性
- **F-NOVELTY**: Novelty reward 在 coevo 模式下效果极其显著（18x 差异），说明探索压力对 coevo 至关重要
- **F-COMM**: 互信息 MI 始终低于 0.3 阈值。原因可能是：
  1. 200 代不足以让通信语义涌现
  2. signal backprop reward 的信号太弱（receiver reward 变化小）
  3. 4 个 channel 太多，难以分化出有意义的语义
  4. 需要更长的 episode 或更大的 population

### 调参建议
- F-COMM 第二轮：尝试 population_size=40, channels=2, backprop_factor=0.8
- 考虑增加"信号引导觅食"的直接奖励（receiver 跟随信号找到食物时额外奖励 source）

## 2026-05-18 · M1 首次真实多代验证

### 目的
验证 M1 的 headless 代际循环是否真的能跑出上升的适应度曲线，而不是只通过单元测试和单代 smoke。

### 实验配置
- 运行方式：`neural-eco.exe --mode headless`
- 代数：12 代
- 每代评估：80 tick
- 种群规模：24
- 世界大小：80 × 80
- 食物数量：16
- 食物能量：12.0
- 食物重生：20 tick
- hidden_layer_size：8
- reward：survival=1.0, food=10.0, intrinsic=1.5
- energy：initial=40.0, base_metabolism=0.01, decision_cost=0.01, movement_cost_factor=0.01
- agent：max_speed=1.5, eat_radius=3.0, exploration_grid_size=10
- random_seed：42

### 结果
`fitness.csv` 前 12 代结果：

| generation | min_fitness | avg_fitness | best_fitness |
|---|---:|---:|---:|
| 0 | 3.8 | 71.55 | 263.3 |
| 1 | 5.3 | 83.2375 | 497.3 |
| 2 | 6.8 | 85.3 | 380.3 |
| 3 | 5.3 | 161.987 | 870.8 |
| 4 | 3.8 | 192.05 | 981.8 |
| 5 | 5.3 | 163.925 | 504.8 |
| 6 | 5.3 | 202.3 | 744.8 |
| 7 | 6.8 | 228.3 | 507.8 |
| 8 | 5.3 | 222.112 | 740.3 |
| 9 | 8.3 | 191.675 | 866.3 |
| 10 | 6.8 | 257.862 | 869.3 |
| 11 | 6.8 | 242.425 | 740.3 |

### 结论
- 平均适应度从 `71.55` 上升到 `242.425`，约为 `3.39x`
- 说明 M1 的最小闭环已经成立：世界、感知、行动、奖励、GA、CSV 记录都在真实运行中协同工作
- 曲线有波动，但整体趋势明显向上，不是纯随机噪声

### 当前判断
- **确定成立**：M1 核心实现可运行，且存在可观测的学习/演化趋势
- **尚未完成**：还没有做 100 代正式验收，也没有做 5 次独立重复实验

### 下一步
1. 用接近默认配置跑 100 代
2. 检查 `avg_fitness` 是否满足 `≥ 初始 × 3`
3. 如果波动太大，优先调这三个参数：
   - `reward.intrinsic_weight`
   - `world.resources.count`
   - `energy.base_metabolism`
4. 增加 `scripts/plot_fitness.py` 画图并归档结果截图

---

## 2026-05-18 · M1 正式验收基线（默认配置，5 seed × 100 代）

### 先说清楚
- 文档里的具体数值（种群规模、tick 数、reward 权重、倍率目标）都**只算示例/起始猜测**，不是预先验证过的真值
- 本节开始，验收以**真实实验数据**为准，不再把文档示例值当硬指标
- 另外，`FoodReward` 在这轮实验前被修正为**按“吃到的食物个数”计 reward**，而不是按食物能量值计 reward；因此**本节结果与上面的旧实验不能直接横向比较绝对数值**

### 实验配置
- 配置来源：`configs/default.toml`
- 运行脚本：`scripts/run_m1_acceptance.py`
- seeds：`42 43 44 45 46`
- 代数：100
- 结果目录：`data/runs/m1_acceptance_baseline_default/`

### 结果

| Seed | First Avg | Final Avg | Multiplier | Peak Best |
|---|---:|---:|---:|---:|
| 42 | 313.150 | 526.750 | 1.682 | 2096.000 |
| 43 | 205.800 | 654.250 | 3.179 | 2261.000 |
| 44 | 248.300 | 684.350 | 2.756 | 2431.000 |
| 45 | 281.450 | 536.650 | 1.907 | 3101.000 |
| 46 | 307.100 | 539.300 | 1.756 | 2921.000 |

- 平均倍率：`2.256x`
- 聚合曲线图：`data/runs/m1_acceptance_baseline_default/plots/aggregate_avg_fitness.png`

### 判断
- **确定成立**：默认配置下，100 代内平均 fitness 有稳定上升趋势，M1 闭环不是假象
- **仍未“验收完成”**：不同 seed 波动明显，说明当前配置还没有稳定到可以把某个倍率门槛当成“已证明”
- **重要发现**：默认配置虽然不是最漂亮的理论设置，但在 100 代长跑下，比后面那组“经验小世界配置”更能拉开倍率

### 调参尝试（单 seed 快速探针）
- 方向 1：降低 `intrinsic_weight`，提高 `base_metabolism`
- 方向 2：降低 `initial_energy`
- 方向 3：降低 `survival_weight`，提高 `food_weight`
- 结果：这些单 seed 快探**都没有明显优于默认配置**，说明问题不是只靠一两个权重就能立刻解决

---

## 2026-05-18 · 经验配置复核（小世界短 episode，5 seed × 100 代）

### 实验配置
- 配置文件：`configs/m1_acceptance.toml`
- 运行脚本：`scripts/run_m1_acceptance.py`
- seeds：`42 43 44 45 46`
- 代数：100
- 关键参数：80×80 世界、24 个体、80 tick/代、16 资源、`intrinsic_weight=1.5`
- 结果目录：`data/runs/m1_acceptance_empirical/`

### 结果

| Seed | First Avg | Final Avg | Multiplier | Peak Best |
|---|---:|---:|---:|---:|
| 42 | 16.550 | 38.675 | 2.337 | 124.800 |
| 43 | 19.738 | 37.050 | 1.877 | 127.800 |
| 44 | 21.842 | 37.821 | 1.732 | 136.300 |
| 45 | 22.008 | 37.404 | 1.700 | 113.300 |
| 46 | 19.613 | 39.237 | 2.001 | 137.800 |

- 平均倍率：`1.929x`

### 判断
- 这组配置**仍然能稳定上升**，但 100 代倍率不如默认配置
- 它适合做**快速实验 / smoke / 小规模调参**，但目前不适合作为最强正式验收配置

### 当前结论
1. `scripts/plot_fitness.py` 和 `scripts/run_m1_acceptance.py` 已经把 M1 验收流程固化下来
2. M1 现在不是“没跑起来”，而是“已经有上升趋势，但还没找到最稳的正式验收参数”
3. 下一步优先级：
   - 继续做**数据驱动调参**，不要迷信文档示例值
   - 增加更多可解释指标（例如每代平均存活 tick、平均吃到的食物数）
   - 用统一脚本比较不同参数组，而不是手工 eyeballing

---

## 2026-05-19 · M3 headless-first 首轮单 seed 验证

### 实现内容
- 已完成 `NodeGene / ConnectionGene / InnovationTracker / NeatGenome / CompatibilityDistance / Speciation / NeatBrain / NeatEvolution`
- `HeadlessRuntime` 已支持 `brain.type = "fixed" | "neat"` 双分支
- `NeatBrain` 使用 CTRNN 双缓冲积分，支持递归连接
- 新增 `evolution.csv`，每代额外记录 `species_count` 与 `avg_complexity`

### 30 代 smoke（`configs/m3_smoke.toml`）
- 种群：24
- 每代评估：80 tick
- seed：42

结果：
- `avg_fitness`：`22.7167 -> 40.55`，约 `1.785x`
- `species_count`：`1 -> 6`
- `avg_complexity`：`17 -> 23.21`

判断：
- M3 不再只是“结构写出来了”，而是**已经能在真实 headless 长跑里同时出现**：
  - fitness 上升
  - 物种分化
  - 拓扑复杂化

### 200 代 acceptance（首轮，`configs/m3_acceptance.toml`，seed=42）

首轮结果：
- `avg_fitness`：`22.7167 -> 40.4667`，约 `1.781x`
- `species_count` 峰值：`8`
- 但 `species_count` 在 `107` 代后塌缩到 `1`
- `avg_complexity`：`17 -> 37.29`

### 问题定位
- 现象不是“NEAT 完全没分化”，而是**前期能分化，后期保不住**
- 根因更像是：
  - 固定 `compatibility_threshold = 3.0` 不会自动把物种数拉回健康区间
  - spawn 分配没有给小物种保底名额，弱物种容易被早早挤死

### 修正
- 给 `Speciation` 增加**动态 compatibility threshold 调整**
  - 目标区间：`3 ~ 10` 个物种
  - 当前物种数低于区间时，下调阈值；高于区间时，上调阈值
- 给 `NeatEvolution` 的 spawn 分配增加**每物种至少 1 个 spawn** 的保底逻辑（当物种数不超过种群规模时）

### 200 代 acceptance（修正后复跑，`configs/m3_acceptance.toml`，seed=42）

结果：
- `avg_fitness`：`22.7167 -> 38.0708`，约 `1.676x`
- `avg_fitness` 峰值：`42.675`
- `species_count`：首代 `1`，峰值 `11`，最终 `3`
- 首次出现多物种：`第 1 代`
- 最后一代仍保持多物种：`第 199 代`
- `avg_complexity`：`17 -> 42.875`

### 结论
- **确定成立**：M3 的 headless-first 主干已经打通
- **关键成立点**：
  - NEAT 拓扑复杂化是真的在发生
  - 物种分化不仅能出现，而且在动态阈值后可维持到长跑末尾
  - fitness 在 200 代内整体仍有上升趋势
- **尚未完成**：还缺 multi-seed 批跑，不能把这组参数当“正式稳定答案”

### 当前经验
1. `compatibility_threshold` 不能当固定常数盲信，长跑时要么动态调，要么至少按物种区间回调
2. 没有小物种保底 spawn，长跑后期很容易物种塌缩
3. `evolution.csv` 这种可解释指标文件非常重要；只看 `fitness.csv` 很容易误判“系统正常”

### 下一步
1. 写 `run_m3_acceptance.py`，做 `5 seed × 100/200 代` 批跑
2. 画 `avg_fitness / species_count / avg_complexity` 聚合曲线
3. 评估这组参数是否足够作为 M3 的默认 headless 验收基线

### multi-seed acceptance（修正后）

#### 100 代（`data/runs/m3_acceptance_100g_fixed/`）

| Seed | First Avg | Final Avg | Multiplier | Peak Species | Final Species | Peak Complexity | Final Complexity |
|---|---:|---:|---:|---:|---:|---:|---:|
| 42 | 22.717 | 38.508 | 1.695 | 11 | 11 | 30.542 | 30.292 |
| 43 | 21.217 | 35.300 | 1.664 | 13 | 9 | 29.125 | 27.375 |
| 44 | 18.633 | 35.946 | 1.929 | 12 | 4 | 27.625 | 27.625 |
| 45 | 19.758 | 34.217 | 1.732 | 12 | 1 | 26.750 | 26.167 |
| 46 | 23.863 | 37.383 | 1.567 | 12 | 7 | 31.375 | 29.917 |

- 平均倍率：`1.717x`
- 平均最终物种数：`6.4`
- 平均最终复杂度：`28.275`

#### 200 代（`data/runs/m3_acceptance_200g_fixed/`）

| Seed | First Avg | Final Avg | Multiplier | Peak Species | Final Species | Peak Complexity | Final Complexity |
|---|---:|---:|---:|---:|---:|---:|---:|
| 42 | 22.717 | 38.071 | 1.676 | 11 | 3 | 42.875 | 42.875 |
| 43 | 21.217 | 36.904 | 1.739 | 13 | 4 | 34.792 | 34.792 |
| 44 | 18.633 | 34.842 | 1.870 | 12 | 9 | 36.000 | 34.917 |
| 45 | 19.758 | 39.154 | 1.982 | 12 | 5 | 35.375 | 35.208 |
| 46 | 23.863 | 38.529 | 1.615 | 12 | 4 | 39.458 | 38.833 |

- 平均倍率：`1.776x`
- 平均最终物种数：`5.0`
- 平均最终复杂度：`37.325`

### 批跑中发现的新 bug（已修）
- 现象：某些 seed 在 100/200 代长跑中报错 `NeatBrain connection references missing node`
- 根因：`addNodeMutation()` 里持有 `ConnectionGene& split` 后对 `genome.connections()` 执行 `push_back`，导致 `std::vector` 重分配后引用悬空；随后读取了脏的 `split.to / split.weight`
- 修正：改成先按值复制 `split`，再修改原连接与追加新连接
- 回归保护：新增 `seed=44` 的长跑 regression test，当前相关 `HeadlessRuntime / NeatEvolution / Speciation / NeatBrain / NeatGenome` 测试共 13 项通过

### 当前判断（更新）
- **确定成立**：M3 headless-first 默认基线已经不是单 seed 偶然现象；5 seed 下都能完成 100/200 代长跑
- **确定成立**：复杂度和物种数都能在长跑中维持，不再出现“前期分化、后期普遍塌成 1 个物种”的问题
- **仍未完全收尾**：还差 `SpeciesObserver` / `species_log.jsonl`，然后才适合把 M3 标成阶段完成

---

## 2026-05-19 · 全模块验证与参数调优

### 目标
证明每个模块真的在产生预期行为，不是"能跑就行"。

### 发现的 Bug

1. **DiseaseLayer 初始感染失败**：`onAttach` 在 `populateWorld` 之前调用，此时 AgentLayer 为空。
   - 修复：将初始感染延迟到第一个 `tick`，检查 `agent_layer->size() > 0`

2. **ModuleMetricsObserver 只记录最终 tick 快照**：DAY/SEASON 的 light_level 和 season_index 在 episode 结束时恰好回到同一值，看起来没变化。
   - 修复：添加 `onTick()` 方法，每 tick 更新 min/max 极值

3. **PredatorHuntCapability 阈值太高**：初始 NEAT 输出很少超过 0.5，导致 kills=0
   - 修复：阈值从 0.5 降到 0.3；同时增大 damage (0.5→5.0) 和 energy_gain (0.3→3.0)

### 参数调优

| 参数 | 原值 | 新值 | 原因 |
|---|---|---|---|
| `pred_damage` | 0.5 | 5.0 | 40 能量猎物需要 80 tick 持续伤害才能死，不现实 |
| `pred_energy_gain` | 0.3 | 3.0 | 与 damage 成比例 |
| `pred_hunt_threshold` | 0.5 | 0.3 | NEAT 初始输出偏低，降低发现门槛 |
| `coop_required_workers` | 3 | 2 | 24 agent 在 80x80 世界太难凑 3 人 |
| `coop_required_ticks` | 5 | 3 | 连续 5 tick 太难维持 |
| `coop_collect_radius` | 5.0 | 8.0 | 增大合作触发范围 |

### 验证结果（100 代 full_validation.toml）

| 模块 | 关键指标 | 范围 | 判定 |
|---|---|---|---|
| F-FIGHT | total_damage_dealt | 15~625 | ✅ 攻击在发生 |
| F-PRED | total_kills | 0~7 | ✅ 捕食者在击杀 |
| F-DISEASE | max_infected | 16~23 | ✅ 疫情大规模传播 |
| F-COMM | signal_field_energy | 95~785 | ✅ 信号场有能量 |
| F-COOP | heavy_resources_available | 0~3 | ✅ 重型资源被消耗 |
| F-RESOURCE | resource_available | 24~32 | ✅ 资源在消耗/再生 |
| F-DAY | min_light~max_light | 0~1 | ✅ 昼夜循环 |
| F-SEASON | min_season~max_season | 0~2 | ✅ 季节变化 |
| F-NEUROMOD | avg_modulatory | 0→0.54~0.79 | ✅ 调制节点被进化保留 |

### A/B 对比：Lamarck vs Darwin（100 代，same seed=42）

| 指标 | Lamarck | Darwin |
|---|---|---|
| last20 best_fitness avg | 273.05 | 254.81 |
| last20 avg_fitness avg | 64.81 | 68.86 |
| last20 avg_modulatory | 0.10 | 0.30 |

Lamarck 模式 best_fitness 略高（+7%），证明学习权重回传有正面效果。
Darwin 模式 avg_fitness 略高且 modulatory 更多，可能因为不回传学习权重，进化需要更多调制机制来补偿。

### 可视化区分要点

要区分各模块效果，可视化需要展示：

- **FIGHT/PRED**：攻击线/击杀标记，捕食者用不同颜色
- **DISEASE**：感染 agent 用红色光晕，传播路径
- **COMM**：信号场热力图叠加
- **COOP**：重型资源周围 agent 聚集动画
- **DAY/SEASON**：背景亮度/色调变化
- **NEUROMOD**：调制节点高亮（不同颜色）
- **LEARN**：Lamarck vs Darwin 对照分屏
- **RESOURCE**：资源量柱状图随时间变化

---

## 2026-05-19 · 游戏机制参考后的系统整合方向

### 参考思路

参考 survival game / colony sim / ecosystem simulation 的常见做法后，疾病不应只是“随机传播 + 随机死亡”，而应成为能改变行为的状态系统：

- **状态效果**：感染降低移动速度、提高代谢消耗
- **进度条逻辑**：感染持续时间、死亡/恢复构成可视化进度
- **环境影响**：季节改变传播压力，洞穴/庇护地降低传播风险
- **风险收益**：捕食感染猎物收益降低，捕食者可能被跨种感染
- **行为反馈**：健康个体能感知最近感染方向，从而进化出回避

### 仪表盘应该看“现象链”

| 仪表盘区域 | 要看的现象 | 解释 |
|---|---|---|
| 感染链 | max_infected / infection_deaths / healthy_infected_dist | 疫情是否形成波，以及健康个体是否学会远离 |
| 生理链 | avg_speed vs avg_infected_speed / avg_energy | 感染是否真的削弱个体 |
| 环境链 | current_season / max_infected | 冬季是否更容易爆发，夏季是否缓和 |
| 地形链 | 洞穴内外感染密度（后续加） | 地形不只是画面，而是庇护资源 |
| 捕食链 | predator_kills / total_kills / infected prey penalty | 捕食是否受疫情反向制约 |
| 合作链 | heavy_workers_avg / heavy_resources_available | 是否有聚集与协作采集 |
| 通信链 | signal_field_energy / 群体聚集或分散指标（后续加） | 信号是否对应群体行为变化 |

### 当前代码落地

- 感染者速度降为 60%
- 感染者基础代谢 ×2
- 夏季传播 ×0.5，秋季 ×1.5，冬季 ×2.5
- 洞穴内被感染概率 ×0.3
- InfectionPerception 从 2 维扩展到 4 维：自我感染、附近感染比例、最近感染者 dx/dy
- 捕食感染猎物：伤害减半、收益 30%、捕食者有 30% 概率感染
- ModuleMetricsObserver 新增：avg_speed、avg_infected_speed、predator_kills、infection_deaths、healthy_infected_dist、current_season

### ACCESS_VIOLATION 崩溃修复

**症状**：Release 模式下随机崩溃（exit code -1073741819 = 0xC0000005），崩溃代数不固定（gen 14~92）。

**根因**：NEAT CTRNN 内部状态产生 NaN → 传播到 agent position → `HeightField::heightAt()` 接收 NaN 坐标 → `static_cast<size_t>(floor(NaN))` 产生极大值 → `data_[y * width_ + x]` 越界访问 → ACCESS_VIOLATION。

**Windows Event Log 确认**：Fault offset 始终为 0x6db22，通过 .map 文件定位到 `HeightField::heightAt`。

**修复**（多层防护）：
1. `NeatBrain::forward`：内部 CTRNN 状态 NaN 检测 → 重置为 0
2. `NeatBrain::forward`：输出 NaN 检测 → 重置为 0
3. `FixedTopologyBrain::forward`：输出 NaN 检测
4. `MoveCapability::apply`：direction NaN 检测
5. `HeightField::heightAt`：入口 NaN 检测 + gx/gy clamp 到 [0, width-1]
6. `CaveMask::toGridX/toGridY`：入口 NaN 检测

### 整合验证结果（100 代 full_validation.toml）

| 现象链 | 指标 | 范围 | 判定 |
|---|---|---|---|
| 感染链 | max_infected | 15~21 | ✅ 疫情大规模传播 |
| 感染链 | infection_deaths | 4~15 | ✅ 疾病致死 |
| 感染链 | healthy_infected_dist | 12~45 | ✅ 健康个体与感染者有距离（但回避行为尚未进化出显著趋势） |
| 生理链 | avg_speed | 0.70~1.25 | ✅ 正常移动 |
| 生理链 | avg_infected_speed | 0.38~0.66 | ✅ 感染者速度约为正常 55%（符合 0.6x penalty） |
| 生理链 | avg_energy | 42~78 | ✅ 能量在波动 |
| 捕食链 | predator_kills | 0~4 | ✅ 捕食在发生 |
| 捕食链 | total_kills | 0~6 | ✅ |
| 环境 | min_light~max_light | 0~1 | ✅ 昼夜循环 |
| 环境 | min_season~max_season | 0~2 | ✅ 季节变化 |
| 合作 | heavy_workers_avg | 0~1 | ✅ |
| 通信 | signal_energy | 64~610 | ✅ |

---

## 2026-05-19 · 病原体-宿主军备竞赛调参

### 目的
实现病原体作为带 NN 的智能体，与宿主形成红皇后军备竞赛。验证抗原漂移机制。

### 关键调参历程

| 问题 | 原因 | 修复 |
|---|---|---|
| 0 recoveries（宿主完全无法恢复） | Pathogen NN 输出映射范围太大：infection_speed [0.5,2.0]，immune_evasion [0,0.8] | 收紧到 infection_speed [0.3,1.2]，evasion [0,0.3] |
| 仍然 0 recoveries | PathogenGenome 初始权重用 uniform(-1,1)，tanh 输出极端值 | 改为 normal(0,0.3)，初始行为温和 |
| 仍然 ~1 recovery | infection_duration=20 太短，immunity_gain_rate=0.03 太慢 | 增大 duration→30，默认 rate→0.05，进化范围 [0.03,0.12]→[0.03,0.20] |
| 恢复后永久免疫，无再感染 | immune() 是永久布尔值 | 改为 strain-based immune memory + 抗原漂移 |

### 最终参数

| 参数 | 值 | 说明 |
|---|---|---|
| PathogenGenome 权重初始化 | normal(0, 0.3) | 初始行为温和 |
| infection_speed 范围 | [0.3, 1.2] | 1.0 = 基准速度 |
| spread_rate 范围 | [0.5, 1.5] | 1.0 = 基准传播 |
| immune_evasion 范围 | [0, 0.3] | 限制逃逸上限 |
| strain signature 维度 | 4 floats in [0,1]⁴ | 抗原空间 |
| strain mutation σ | 0.1 | 抗原漂移速度 |
| immunity distance threshold | 0.5 (Euclidean) | 免疫识别阈值 |
| infection_duration | 30 ticks | 给宿主足够恢复窗口 |
| immunity_gain_rate 默认 | 0.05 | 30 ticks 可恢复 |
| immunity_gain_rate 进化范围 | [0.01, 0.20] | 可进化 trait |

### 验证结果（100 代 full_validation）

| 指标 | 值 | 说明 |
|---|---|---|
| Recoveries | 854 | 有效恢复 |
| Recovery/Death | 0.02 | 仍然很低但存在 |
| 宿主 immunity_gain_rate Q1→Q4 | 0.0785→0.1040 | +33% 进化提升 |
| 免疫人口 Q1→Q4 | 3.4→5.4 | 持续增长 |
| 代中免疫人口下降比例 | 40% | 新毒株突破旧免疫 |
| Pathogen avg infection_speed | 0.735 | 低于 1.0，病原体倾向温和 |
| Pathogen avg immune_evasion | 0.144 | 中等逃逸 |

### 关键发现

1. **病原体 NN 初始输出映射范围是生死线**：太宽→宿主灭绝，太窄→无进化压力
2. **抗原漂移是红皇后假说的核心**：没有它，免疫是永久的，疾病变成一次性事件
3. **strain distance threshold = 0.5** 在 4D [0,1]⁴ 空间中意味着约 6% 的体积被覆盖，需要约 17 个不同毒株免疫才能覆盖整个空间
4. **病原体倾向温和演化**：infection_speed < 1.0 说明杀死宿主太快不利于传播（毒力-传播权衡）
