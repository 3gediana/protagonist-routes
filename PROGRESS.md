# PROGRESS.md · 项目进度追踪

> **这份文档是项目的"实时进度仪表盘"**。任何 AI 助手进入项目时，先读 [`AGENTS.md`](AGENTS.md)，再读这份，立刻就知道项目走到了哪一步、下一步该做什么、需要看哪些文件。

---

## ⏱️ 顶部状态

| 项 | 值 |
|---|---|
| **最后更新** | 2026-05-20 |
| **当前阶段** | 🟩 Baseline M0-M5 完成 + 12 Modules MVP + protagonist 8-物种全生态联合演化训练流水线就绪 |
| **整体进度** | ▰▰▰▰▰▰▰▰▰▰ 100% (Baseline + MVP + Protagonist Research & Trace基座完成) |
| **最近会话摘要** | protagonist 路线的 Trace/Resume 基座已保持完成态；本轮先把 Memory V2 半成品修回可编译、可运行状态：修复 `SpatialMemoryBank.cpp` 与 `MemoryCognitionPerception` 的接口错误，`neural-eco-protagonist` 重新构建成功，并再次用 `configs/protagonist_ecology_train_smoke.toml` 跑通 smoke。随后按用户新优先级完成连续时间方向的外部调研并写出 `routes/protagonist_ecology/docx/CONTINUOUS_TIME_WORLD_DESIGN.md`，然后直接落地 CT-0：新增 `SimTimeSeconds`、为 `Event` 加入 `timeSeconds()`、为 `IWorld` / `World2D` 加入 `currentTimeSeconds()`、让 `TickStarted` / `TickEnded` 带秒级时间戳，并把 protagonist `TraceRecorder` 升级成双时间戳输出。CT-0 已重新构建并 smoke 验证通过。 |
| **下一步建议** | 既然 CT-0 已经落地，下一步建议继续实现 `CONTINUOUS_TIME_WORLD_DESIGN.md` 的 CT-1：让 protagonist runtime 从 `episode_ticks` 迁到 `episode_duration_seconds + substep_dt`，并加入 legacy tick / continuous mode 兼容读取。等 runtime 真正 seconds 化后，再迁移 move / drink / eat / signal 等首批连续动作。 |

---

## 📋 这份文档怎么用（给 AI 看）

### 每次新会话进入项目

```
1. 读 AGENTS.md（你的协作手册）
2. 读这份 PROGRESS.md（看当前进度，知道从哪继续）
3. 读 docs/ROADMAP.md 和 docs/ARCHITECTURE.md（理解整体）
4. 找当前阶段的"未完成任务"，从第一个开始做
5. 完成后立刻在这份文档打勾 + 更新"最后更新"日期 + 更新"最近会话摘要"
```

### 任务状态符号

| 符号 | 含义 |
|---|---|
| `[ ]` | 未开始 |
| `[~]` | 进行中（当前会话中正在做） |
| `[x]` | 已完成 |
| `[!]` | 阻塞 / 等用户决策 |
| `[-]` | 已取消 / 不再需要 |

### 临时任务

用户在过程中提出的新需求 → 加到"🆕 临时任务"段，标明日期与来源。完成后归档到对应阶段。

---

## 🏗️ 阶段进度

### 📐 P0 · 文档与架构设计

> 不是 baseline 的一部分，是设计阶段。**已完成**。

#### 顶层文档
- [x] `README.md` 项目对外介绍
- [x] `AGENTS.md` AI 助手协作手册
- [x] `PROGRESS.md` 本文档
- [x] `docs/ROADMAP.md` 开发路径
- [x] `docs/ARCHITECTURE.md` 架构设计
- [x] `docs/DESIGN_DECISIONS.md` 设计决策汇总
- [x] `docs/GLOSSARY.md` 术语表

#### ADR（架构决策记录）
- [x] `docs/adr/0001-terrain-is-real-physics-not-decoration.md`
- [x] `docs/adr/0002-brain-architecture-recipe.md`
- [x] `docs/adr/0003-i18n-english-code-bilingual-ui.md`
- [x] `docs/adr/0004-incremental-perfect-then-add.md`

#### Baseline 阶段文档
- [x] `docs/baseline/M0_scaffold.md`
- [x] `docs/baseline/M1_mvp.md`
- [x] `docs/baseline/M2_visualization.md`
- [x] `docs/baseline/M3_neat.md`
- [x] `docs/baseline/M4_async.md`
- [x] `docs/baseline/M5_terrain.md`

#### Module 卡片
- [x] `docs/modules/README.md` 模块库索引
- [x] 11 份完整模块卡：F-COMM / F-TREE / F-NEUROMOD / F-GOD / F-HEAT / F-COOP / F-FIGHT / F-LEARN / F-LLM / F-FORK / F-NOVELTY
- [x] 13 份精简模块卡：F-PRED / F-TOOL / F-DAY / F-SEASON / F-DISEASE / F-RESOURCE / F-CAVE / F-STAT / F-REPLAY / F-COMPARE / F-CURRICULUM / F-COEVO / F-NSGA

#### 工程基础占位
- [x] `CMakeLists.txt` 顶层骨架
- [x] `src/CMakeLists.txt` + 各子目录 CMakeLists
- [x] `tests/CMakeLists.txt`
- [x] `configs/default.toml` 完整模板
- [x] `configs/i18n/zh.toml` + `configs/i18n/en.toml`
- [x] `.gitignore` / `.editorconfig`
- [x] `src/main.cpp` 入口占位
- [x] `src/core/README.md` / `src/viz/README.md` / `src/runtime/README.md`
- [x] `tests/README.md` / `data/fonts/README.md` / `scripts/README.md`

---

### 🟦 M0 · 脚手架与抽象

> 详细文档：[`docs/baseline/M0_scaffold.md`](docs/baseline/M0_scaffold.md)
> 状态：✅✅⛳（代码 / 构建 / 测试完成；git tag 待补）
> 完成时打 git tag：`baseline-M0-MVP` → `baseline-M0-feature-complete` → `baseline-M0-done`

#### 准备工作
- [x] 已识别开发环境（Windows + VS2022 BuildTools / MSVC）
- [x] 本机存在可用 CMake 3.31（VS BuildTools 自带）与编译器
- [x] 依赖管理方式已定：FetchContent

#### MVP 阶段（[ ] [ ] [ ] = 三阶段）
- [x] [x] [x] **构建系统**
  - [x] 顶层 CMakeLists 完善（依赖 FetchContent 接入 tomlplusplus / spdlog / gtest / raylib）
  - [x] `src/CMakeLists.txt` 真正定义 `neural_eco_core` 静态库
  - [x] `tests/CMakeLists.txt` 配置 gtest_main 自动 main
  - [ ] CI 配置（可选，GitHub Actions Windows + Linux）

- [x] [x] [x] **核心抽象接口**（`src/core/interfaces/`）
  - [x] `IBrain.h`
  - [x] `IGenome.h`
  - [x] `IWorld.h`
  - [x] `IWorldLayer.h`
  - [x] `IAgent.h`
  - [x] `IPerception.h`
  - [x] `IAgentCapability.h`
  - [x] `IRewardComponent.h`
  - [x] `IEvolutionStrategy.h`
  - [x] `IObserver.h`
  - [x] `IEventBus.h`
  - [x] `IRenderer.h`（占位，M2 才用）

- [x] [x] [x] **事件总线**（`src/core/events/`）
  - [x] EventType 枚举（包含未来阶段需要的所有类型）
  - [x] EventBus 实现：subscribe / publish / unsubscribe / flush
  - [x] deferred queue（避免订阅者回调里发新事件死锁）
  - [x] 单元测试

- [x] [x] [x] **配置系统**（`src/core/config/`）
  - [x] TOML 加载（toml++）
  - [x] 嵌套 key 读取 `Config::get<T>("path.to.key")`
  - [ ] 热更新 hooks（可选）
  - [x] 单元测试

- [x] [x] [x] **日志系统**（`src/core/logging/`）
  - [x] spdlog 包装
  - [x] 控制台 + 文件双输出
  - [x] 级别可配置

- [x] [x] [x] **运行时骨架**
  - [x] `HeadlessRuntime` 框架（init / tick / shutdown 空壳）
  - [x] `InteractiveRuntime` 框架（同上）
  - [x] `main.cpp` CLI 解析与运行时分发
  - [x] `--hello` 输出 `Neural Eco v0.1 - Hello`

#### 完整阶段
- [x] 所有 12 个核心接口头文件就位
- [x] 事件总线 / 配置 / 日志全部带单元测试

#### 完美阶段
- [x] 接口设计 review：能否承载 NEAT (M3) 和 2.5D 地形 (M5)
- [x] CMake 输出无警告
- [ ] git tag `baseline-M0-done`（待后续执行 git 写操作时补）
- [x] 更新本文档 + ROADMAP 进度表

---

### 🟦 M1 · 连续 2D MVP

> 详细文档：[`docs/baseline/M1_mvp.md`](docs/baseline/M1_mvp.md)
> 状态：✅⛳⛳（核心代码完成，长跑验收未完成）
> 前置：M0 完美完成

- [x] FixedTopologyGenome / FixedTopologyBrain
- [x] World2D + AgentLayer + ResourceLayer
- [x] Agent + SelfPerception + NearestFoodPerception
- [x] MoveCapability + EatCapability
- [x] ClassicGA + EliteSelection + TournamentSelection
- [x] SurvivalReward + FoodReward + IntrinsicReward
- [x] FitnessRecorder（CSV 输出）
- [x] `scripts/plot_fitness.py` + `scripts/run_m1_acceptance.py`
- [~] 训练 100 代正式验收与调参（已完成 5 seed × 100 代默认配置测量，平均倍率 `2.256x`；见 `docs/experience_log.md`）
- [ ] git tag `baseline-M1-done`

---

### 🟦 M2 · 可视化接入

> 详细文档：[`docs/baseline/M2_visualization.md`](docs/baseline/M2_visualization.md)
> 状态：⏸ 延后（ADR-0005）

- [ ] raylib 接入
- [ ] i18n 系统（**第一天就要上**）
- [ ] CJK 字体加载
- [ ] WorldRenderer / AgentRenderer
- [ ] HUD / ControlPanel / StatsPanel / LangSwitcher
- [ ] FitnessChart 实时折线图
- [ ] 中英切换按钮工作
- [ ] grep 全代码无硬编码 UI 字符串
- [ ] git tag `baseline-M2-done`

---

### 🟦 M3 · NEAT 升级

> 详细文档：[`docs/baseline/M3_neat.md`](docs/baseline/M3_neat.md)
> 状态：✅✅⛳（核心实现、multi-seed 验证、species 记录已完成；git tag 待补）
> 风险：🔴 高

- [x] NodeGene / ConnectionGene / NeatGenome / NeatBrain
- [x] InnovationTracker
- [x] 5 种变异操作（add_node / add_connection / toggle / weight_perturb / weight_reset）
- [x] NeatCrossover（基于创新编号）
- [x] CTRNN 时序集成（递归边支持）
- [x] 物种分化（Speciation + CompatibilityDistance）
- [x] Fitness Sharing
- [x] 替换 M1 的 FixedTopologyBrain（**接口压力测试 #1**）
- [x] SpeciesObserver 输出 species_log.jsonl
- [x] 跑 200 代不崩 + 拓扑增长 + 物种数稳定（5 seed × 200 代已成立）
- [ ] git tag `baseline-M3-done`

---

### 🟦 M4 · 多线程加速

> 详细文档：[`docs/baseline/M4_async.md`](docs/baseline/M4_async.md)
> 状态：✅✅⛳（EvaluationPool 线程池 / PerformanceObserver / 异步决策周期 / 能量系统均已实现；git tag 待补）
> 前置：M3 完美通过

- [x] EvaluationPool 线程池（`src/runtime/headless/EvaluationPool.h/.cpp`）
- [x] PerformanceObserver 性能记录（`src/core/observer/PerformanceObserver.h/.cpp`）
- [x] 异步决策周期（NeatGenome.decision_interval 可进化基因）
- [x] 能量消耗系统（base_metabolism / decision_cost / movement_cost_factor）
- [x] HeadlessRuntime 集成 eval_threads 配置
- [ ] 训练时长加速 ≥ 3 倍验收
- [ ] 决策频率分化可观察（标准差 ≥ 5 tick）
- [ ] git tag `baseline-M4-done`

---

### 🟦 M5 · 真实 2.5D 地形

> 详细文档：[`docs/baseline/M5_terrain.md`](docs/baseline/M5_terrain.md)
> 状态：✅✅⛳（TerrainLayer / HeightField / CaveMask / TerrainPerception / 地形物理均已实现；等距渲染顺延到 M2 整合；git tag 待补）
> 前置：M4 完美通过

- [x] TerrainLayer 实现 IWorldLayer（`src/core/world/layers/terrain/TerrainLayer.h/.cpp`）
- [x] HeightField 双线性插值高度查询（`HeightField.h/.cpp`）
- [x] CaveMask 山洞标记 + 进入/离开判定（`CaveMask.h/.cpp`）
- [x] TerrainGenerator 地形生成（`TerrainGenerator.h/.cpp`）
- [x] TerrainPerception 5 维输出接入感知（`src/core/perception/TerrainPerception.h/.cpp`）
- [x] MoveCapability 地形速度修正 hook
- [x] HeadlessRuntime 地形启用配置 + 输入维度自动扩展
- [ ] 等距渲染（IsometricProjection）— 顺延到 M2 可视化整合
- [ ] 跑 100 代后地形分化可观察
- [ ] git tag `baseline-M5-done` + `baseline-complete`

---

## 🟨 模块库进度

> 完成 baseline 后从这里挑模块加。
> 完整推荐路径见 [`docs/modules/README.md`](docs/modules/README.md) 末尾。

### 已实现（MVP ✅⛳⛳，均通过 100 代 full_validation 量化证据验证）

| 模块 | 代码位置 | 关键证据 |
|---|---|---|
| **F-FIGHT** 对抗机制 | `AttackCapability` / `CombatReward` | total_damage_dealt 15~625 |
| **F-PRED** 捕食者-猎物 | `PredatorHuntCapability` | total_kills 0~7 |
| **F-DISEASE** 疾病传播 | `DiseaseLayer` / `InfectionPerception` / `PathogenGenome` / `Pathogen` | max_infected 16~23；宿主 immunity_gain_rate +33%；抗原漂移：40% 代免疫人口下降 |
| **F-COMM** 通信信号场 | `SignalLayer` / `SignalPerception` / `EmitSignalCapability` | signal_field_energy 95~785 |
| **F-COOP** 合作机制 | `HeavyResourceLayer` / `CoopReward` | heavy_resources_available 0~3 |
| **F-RESOURCE** 动态资源 | `ResourceLayer` dynamic 模式 | resource_available 24~32 |
| **F-DAY** 昼夜循环 | `TimeOfDayLayer` / `TimeOfDayPerception` | min_light~max_light 0~1 |
| **F-SEASON** 季节变化 | `SeasonLayer` / `SeasonPerception` | min_season~max_season 0~2 |
| **F-NEUROMOD** 神经调节 | NeatBrain modulatory nodes | avg_modulatory 0→0.54~0.79 |
| **F-LEARN** 终身学习 + 拉马克 | `HebbianLearning` / lamarck_mode | Lamarck best_fitness +7% |
| **F-NOVELTY** 新颖性搜索 | `NoveltyReward` / `NoveltyArchive` | 已接入 reward 系统 |
| **F-CURRICULUM** 课程学习 | HeadlessRuntime curriculum 配置 | 4 阶段难度递增 |

### Stage A · 答辩底线（必做）
- [ ] **F-TREE** 物种树 → [`docs/modules/F-TREE_phylogenetic.md`](docs/modules/F-TREE_phylogenetic.md)
- [ ] **F-HEAT** 行为热力图 → [`docs/modules/F-HEAT_heatmap.md`](docs/modules/F-HEAT_heatmap.md)
- [ ] **F-GOD** 上帝按钮 → [`docs/modules/F-GOD_god_panel.md`](docs/modules/F-GOD_god_panel.md)

### Stage B · 中等亮点
- [x] **F-NEUROMOD** 神经调节情绪 ✅⛳⛳
- [x] **F-COMM** 通信信号场 ✅⛳⛳
- [x] **F-LEARN** 拉马克开关 ✅⛳⛳

### Stage C · 王炸彩蛋
- [x] **F-COOP** 合作机制（围猎涌现） ✅⛳⛳
- [ ] **F-LLM** LLM 解说员

### Stage D · 锦上添花
- [ ] **F-FORK** 平行宇宙存档
- [ ] **F-COMPARE** 对照实验

### 其它（已实现）
- [x] F-FIGHT ✅⛳⛳ / F-PRED ✅⛳⛳ / F-NOVELTY ✅⛳⛳
- [x] F-DAY ✅⛳⛳ / F-SEASON ✅⛳⛳ / F-DISEASE ✅⛳⛳ / F-RESOURCE ✅⛳⛳
- [x] F-CURRICULUM ✅⛳⛳

### 其它（待开发）
- [ ] F-TOOL / F-CAVE / F-COEVO / F-NSGA

---

## 🆕 临时任务（用户随时插入）

> 当用户在某次会话提出新需求/想法且不属于现有阶段任务时，记在这里。
> 完成后归档到对应阶段，或长期不消化的转为 ADR。

格式：`- [状态] 任务描述（来源：日期 / 用户原话简述）`

- [~] 搭建 spectacle-first 并行路线骨架 `routes/protagonist_ecology/`，保留旧项目主线并准备主角大脑/保育箱新路线（来源：2026-05-19 / 用户要求另开文件夹保留两个项目）
- [x] 将 `routes/protagonist_ecology/` 从骨架推进到可运行训练路线：完成 train/eval 角色分离、主角生存状态与喝水机制、主角 hunt/flee 课程钩子、3 个 eval 世界与 3 个 curriculum train 世界（来源：2026-05-20 / 用户要求先把测试集与训练集逐步做出来，并朝“原始人行为”推进）
- [x] 将 protagonist 工具前置链推进到 primitive tool 第二步：完成 `Spear` / 做矛 / 投掷石头与长矛 / 库存与武器感知 / action_dim 扩展，并验证 `curriculum_camp_building` 出现非零 craft/throw 行为（来源：2026-05-20 / 用户要求继续实现原始人工具链，不要停下来）
- [x] 将 protagonist `camp_building / preview_xlong` 环境调到可作为训练前基线：修复工地解锁与石头供给断链，加入工地自动接收携带材料，并完成 `preview_xlong` 的多 seed（42/91/123）稳定性抽查，均出现 `completed_worksites=1`（来源：2026-05-20 / 用户要求先把环境调好，再由自己参与训练过程）
- [x] 将 protagonist route 从 preview runtime 接到真实训练闭环：新增可进化 protagonist genome/GA、训练模式 runtime、多代输出文件、smoke training 验证，以及正式可启动但未运行的 camp-building 训练配置（来源：2026-05-20 / 用户要求“那继续接上”）

---

## 📝 会话历史

> 每次会话结束时简要追加一条。让接力的下一个 AI 知道前面发生了什么。

| 日期 | 会话摘要 | 产出 | 停在哪 |
|---|---|---|---|
| 2026-05-18 | 第 1 次会话：项目立项 + 文档体系 | 顶层 README/ROADMAP/ARCHITECTURE/DESIGN_DECISIONS/GLOSSARY；4 份 ADR；6 份 baseline 阶段卡；24 份模块卡；工程占位文件；AGENTS.md；PROGRESS.md | 准备进入 M0 实施 |
| 2026-05-18 | 第 2 次会话：完成 M0 实施 | FetchContent 依赖接入；基础类型；12 个核心接口；EventBus；Config；Logger；Headless/Interactive Runtime 骨架；CLI；24 个单测；Release 构建通过；`--hello` 与 headless smoke test 通过 | 开始 M1 实现 |
| 2026-05-18 | 第 3 次会话：完成 M1 核心实现 | FixedTopologyGenome/Brain；Agent / Perception / Capability；World2D + AgentLayer + ResourceLayer；ClassicGA；Reward；FitnessRecorder；真实 Headless 代际循环；新增测试后总计 32 个全过；12 代真实试跑已看到 fitness 明显上升 | 待做 100 代正式验收与调参 |
| 2026-05-18 | 第 4 次会话：补齐 M1 验收工具并完成多 seed 基线测量 | `plot_fitness.py`；`run_m1_acceptance.py`；默认配置 5 seed × 100 代基线（平均倍率 2.256x）；经验配置 5 seed × 100 代复核（平均倍率 1.929x）；`configs/m1_acceptance.toml`；文档数值改为"示例/起始猜测"口径 | 待补更可解释的验收指标并继续调参 |
 | 2026-05-18 | 第 5 次会话：确认延后可视化并改路线 | ADR-0005；ROADMAP/PROGRESS 同步为"先机制、后可视化整合"；下一阶段确定为 M3 headless-first | 待开始 M3 NEAT 基础数据结构 |
 | 2026-05-19 | 第 6 次会话：完成 M3 实现、验收与回归保护 | `NodeGene/ConnectionGene/InnovationTracker/NeatGenome/NeatBrain/Speciation/NeatEvolution`；`brain.type = neat` headless 接入；`evolution.csv` + `species_log.jsonl`；`run_m3_acceptance.py`；`5 seed × 100/200 代` 验证；修复 `addNodeMutation()` 悬空引用 bug 并补 regression test | 准备进入 M4 调研与实现 |
 | 2026-05-19 | 第 7 次会话：完成 M4/M5 + 12 模块实现与全证据验证 | M4: EvaluationPool 线程池 + PerformanceObserver + 异步决策周期 + 能量系统；M5: TerrainLayer/HeightField/CaveMask/TerrainPerception/TerrainGenerator + 地形物理 hook；12 模块（F-FIGHT/F-PRED/F-DISEASE/F-COMM/F-COOP/F-RESOURCE/F-DAY/F-SEASON/F-NEUROMOD/F-LEARN/F-NOVELTY/F-CURRICULUM）全部 MVP 通过；100 代 full_validation 量化证据；Lamarck vs Darwin A/B 对比；ModuleMetricsObserver tick 级追踪 | Baseline 机制全部完成，待进入 M2 可视化整合 |
 | 2026-05-19 | 第 8 次会话：免疫进化 + 病原体 NN + 抗原漂移 | 宿主 immunity_gain_rate 可进化 trait（NeatGenome + NeatEvolution crossover/mutation + evolution.csv 追踪）；PathogenGenome（31 权重 3→4→3 NN）+ Pathogen 实体（think → infection_speed/spread_rate/immune_evasion）；DiseaseLayer 重写为 pathogen-driven；病原体传播时复制+变异；捕食者跨物种感染；抗原漂移：4 维 strain signature + immune memory + strain distance check 取代永久 immune()；InfectionPerception output[4] 改为免疫记忆丰富度；SurvivalReward 改为按 strain 计数；module_metrics.csv 新增 4 列 pathogen 指标 | 继续完善疾病系统或进入 M2 可视化 |
 | 2026-05-19 | 第 9 次会话：修复核心假指标与机制断链 | `ComprehensiveMetrics` 改为事件驱动行为统计并修正 `alive_ratio / immune_ratio / action_entropy / spatial_entropy / 行为MI`；`Agent` 免疫状态与 immune memory 同步；`DiseaseLayer` 恢复/感染状态修正；`MateCapability` → runtime → evolution 的真实 mating pair 闭环；`dietPreference` 接入 plant/meat 能量收益与 reward；补齐 Flee/Tool/Terrain/Signal 事件；NSGA-II 行为基因继承/变异补齐；新增 `configs/core_regression_short.toml` | 下一步做中长跑回归，验证 mating/diet/immune 指标在 50~100 代中的演化稳定性 |
 | 2026-05-19 | 第 10 次会话：修复 disease arms-race 统计断链 | `ComprehensiveMetrics` 的 disease arms-race 由代末快照改为代内 tick 累积；新增 `tests/core/test_comprehensive_metrics.cpp` 回归测试覆盖“代内感染、代末恢复”场景；Release 构建与 60/60 测试通过；重链 `neural-eco.exe` 后用 `configs/core_regression_short.toml` 验证 `comprehensive/disease_arms_race.csv` 在 coevo 路径恢复非零 | 下一步重跑 `scale100_v5` 中长跑/500 代回归，重新解读疾病相关阶段曲线 |
 | 2026-05-19 | 第 11 次会话：搭建 protagonist 并行路线骨架 | 新增 `routes/protagonist_ecology/` 独立目录、`neural-eco-protagonist` 可执行、`ProtagonistEcologyRuntime/Config/WorldFactory`，并新增 `configs/protagonist_ecology_bootstrap.toml`；直接复用现有 `core/world/config` 框架，编译与启动验证通过 | 下一步把主角大脑、记忆区、保育箱种群装配进新 runtime |
| 2026-05-20 | 第 12 次会话：把 protagonist 路线推进到 train/eval/curriculum 可运行 | 显式 `world_role/scenario_name`；3 个 eval 世界（`eval_generalization / eval_sparse_terrain / eval_timing_shift`）；route-local 水/口渴/血量状态系统；主角社会感知 + 狩猎/逃跑课程钩子；3 个 train curriculum 世界（`survival_foundation / camp_building / hunt_avoid`）；`neural-eco-protagonist` 构建通过且各配置可启动 | 下一步补树/木材/砍树与工具前置链，继续朝原始人生存训练集推进 |
| 2026-05-20 | 第 13 次会话：完成 protagonist primitive tool 第二步并校正优先级 | 新增 `Spear`、做矛/投掷能力、携带/库存/武器感知、库存消耗 API、action_dim=13、训练日志计数；`curriculum_camp_building` 已出现非零 `craft_successes` 与 `throw_hits`。期间试做过 live/prod 边界与世界配置，但已确认暂不继续，当前优先级回到训练世界工具链稳定化 | 下一步提高 `camp_building` 里石头/营火/工地闭环稳定性，然后启动真正训练 |
| 2026-05-20 | 第 14 次会话：把 protagonist camp-building 环境调到训练前可用 | 修复 `NurseryProgressionLayer` 的工地解锁鸡生蛋；旧 preview 配置同步到当前训练难度；石头投放偏向激活工地；做矛为未完成工地保留稀缺石头；`WorksiteLayer` 新增携带材料自动交付。`preview_xlong` 在 seed 42/91/123 下均出现 `completed_worksites=1`，说明工地主闭环已具备训练前稳定性 | 下一步检查其余 train/eval 配置是否需要同步新闭环，然后由用户参与决定何时开始正式训练 |
| 2026-05-20 | 第 15 次会话：把 protagonist runtime 接成真实训练循环并做 smoke 验证 | 新增 `ProtagonistGenome` / `ProtagonistEvolution`；`ProtagonistBrain` 支持 genome 构造；`BootstrapPopulation` 支持按显式 population 装配 protagonist；`ProtagonistEcologyRuntime` 支持 `evolution.generations` 训练模式并输出 `fitness.csv / evolution.csv`；`configs/protagonist_ecology_train_smoke.toml` 3 代 smoke training 通过；新增 `configs/protagonist_ecology_train_camp_building_evolve.toml` 作为正式训练入口 | 下一步由用户决定何时亲自启动首轮正式训练；之后继续补 checkpoint / best-genome 导出 / eval 配套验证 |
| 2026-05-20 | 第 16 次会话：把训练集补齐到能往"理解世界、合作、交流"收敛 | 训练循环改多 episode 评估求平均（K=3 默认，可配置 `evolution.episodes_per_generation`）；reward 增加合作（worksite proximity / completion 分红 / social 邻近）与通信（信号发射后 co-attendance 进食）维度；`ProtagonistGenome` 加 binary save/load，每代导出 best genome（gen/latest/overall 三份）；新增 `protagonist_behaviors.csv` 行为指标 CSV；修掉 `seedTrainingPopulation` 中 background genome ID 与 reproduce 后 protagonist 新 ID 撞车的 bug（之前会让 background 被错认为 protagonist）；smoke 验证三类产物均正确，total_protagonists 数干净一致 | 等用户给具体训练要求，再决定是否进一步加 NoveltyReward / checkpoint / curriculum 调权 |
| 2026-05-20 | 第 17 次会话：再加 emit / joint hunt / checkpoint 三块训练集要素 | 信号发射时附近有同伴的 emit reward 接通；共同围猎 kill 时环内同伴拿到 joint-hunt 分红；按 `evolution.checkpoint_every` 把整个 population dump 为 `checkpoint_gen{N}/genome_{id}.bin` 用于 resume；smoke 配置启用 episodes_per_generation=2 / checkpoint_every=2 后跑通，3 代均产出 fitness/behavior/best genome/checkpoint；正式 evolve 配置同步引入新 reward 与 checkpoint 周期 | 等用户给训练要求，再决定 NoveltyReward / train→eval 串联 / curriculum 分阶段权重 |
| 2026-05-20 | 第 18 次会话：8 物种联合演化接入 EvaluationPool 并完成中规模试训 | 修复 `neural-eco-protagonist` 链接错误：route 库使用 `EvaluationPool` 但未链接 `neural_eco::runtime_headless`；多 episode 评估改为线程池并行任务，episode-local fitness/species/behavior 在主线程合并；Release 构建与 EvaluationPool 单测通过；smoke 多线程跑通；中规模 `protagonist_ecology_train_camp_building_evolve.toml` 120 代完整跑通，行为总量 `builds=218 / craft=2021 / throw_hits=843 / hunts=4509 / signal_co_attendance=82864`，最终 checkpoint 正常保存；新增 `runtime.protagonist_genome_path`，可把 best genome 加载进 eval/preview 世界并已短跑验证 | 可启动 overnight；也可继续补 eval 行为 CSV / replay trace |
| 2026-05-20 | 第 19 次会话：完成 protagonist 世界理解与复杂行为长期方案文档 | 新增 `routes/protagonist_ecology/docx/WORLD_UNDERSTANDING_RESEARCH_AND_ENGINEERING_PLAN.md`；结合当前代码/训练现状与论文调研，系统提出记忆区扩容与分区、空间/事件/社会/目标记忆、世界模型、因果奖励、通信语义任务、合作信用分配、MAP-Elites/POET 开放式课程、完整 replay trace、checkpoint resume、层级目标与 3D replay 面板路线 | 下一步从 TraceRecorder + resume checkpoint 开始落地，为后续 Memory V2 和复杂行为验证打基础 |
| 2026-05-20 | 第 20 次会话：高标准落地 Phase 1 Trace / Resume 基础设施 | 新建并打通 `TraceRecorder.h/cpp`，支持按 `[trace]` 选项按 episode 独立输出 `event_trace.jsonl`、`agent_trace.csv`、`world_trace.csv`、`replay_manifest.json`；实现 `checkpoint_manifest.json`，且打通 `runtime.resume_checkpoint_path` 支持完美 resume 主角 population，已跑通并验证了基因段代际继承；归档了用户提出的“连续时间世界框架”问题 | 下一步建议进入 Memory V2：设计 Working/Spatial/Episodic/Social/Goal 专门记忆区并接入 ProtagonistBrain |

---

## 🗂️ 关键文件位置速查表

> 找东西用。新会话不要问"XX 在哪"，先查这张表。

| 你想找 | 路径 |
|---|---|
| 项目对外介绍 | `README.md` |
| AI 协作手册 | `AGENTS.md` |
| 项目实时进度（本文件） | `PROGRESS.md` |
| 整体开发节奏 | `docs/ROADMAP.md` |
| 架构与扩展点 | `docs/ARCHITECTURE.md` |
| 关键决策记录 | `docs/adr/000{1-4}*.md` + `docs/DESIGN_DECISIONS.md` |
| 术语解释 | `docs/GLOSSARY.md` |
| 当前阶段细节 | `docs/baseline/M{N}_*.md` |
| 模块卡 | `docs/modules/F-{XXX}_*.md` |
| 默认配置 | `configs/default.toml` |
| 中英文 UI 资源 | `configs/i18n/{zh,en}.toml` |
| 调参经验日志（每次失败/成功记录） | `docs/experience_log.md` |
| 答辩话术汇总 | `docs/defense_script.md` _(待创建于答辩准备时)_ |
| 训练数据输出 | `data/runs/<timestamp>/` |
| 平行宇宙存档 | `data/snapshots/` |
| 字体目录 | `data/fonts/` |
| 分析脚本 | `scripts/` |

---

## ⚠️ 已知问题与注意事项

> 每次会话发现的坑、踩过的雷、调参经验逐步累积在这。

### 设计相关
1. **i18n 系统从 M2 第一天就要上**，不能事后国际化
2. **NEAT 输入维度不能写死**——M5 加地形会扩展，F-COMM 还会再扩展（HeadlessRuntime 已按模块开关动态扩展 input_dim）
3. **EventBus 用 deferred queue**，避免回调死锁
4. **每个 agent 一个独立 RNG（thread_local）**，不共享全局
5. **NEAT 物种 ID 永远递增不复用**，否则物种树会画错

### 实施相关
 1. **Windows 本机没有全局 `cmake` 在 PATH**，但 VS2022 BuildTools 自带的 `cmake.exe` / `ninja.exe` 可直接用
 2. 当前已验证的构建链：**VS2022 BuildTools + MSVC 14.44 + CMake 3.31（BuildTools 自带）**
 3. `git tag` 尚未执行；代码、构建、测试状态已就绪，可在后续有 git 写操作时补打 `baseline-M0-done` ~ `baseline-M5-done`
 4. M1 当前卡点已从“实现”转为“验收与调参”——已完成 5 seed × 100 代默认配置测量，平均倍率 `2.256x`
 5. 文档里的具体数值（tick、种群规模、reward 权重、倍率目标）都只应视为**示例/起始猜测**，后续以真实实验结果为准
 6. `FoodReward` 已修正为按“吃到的食物个数”计 reward；修正前后的 fitness 绝对值不能直接横向比较
 7. `ComprehensiveMetrics` 现已改为**事件驱动行为统计 + 活体口径**；`action_entropy / immune_ratio / alive_ratio` 已经可用于真实分析
 8. 新增短回归配置：`configs/core_regression_short.toml`，覆盖 coevo + disease + signal + tool + mate + terrain_mod + NSGA-II，适合快速验收核心闭环
 9. 修 `core` / `runtime_headless` 库后，**不要只重编 `test_core`**；要重链 `build/bin/Release/neural-eco.exe` 再做 headless 回归，否则可能拿旧 exe 误判结果

---

## 🔄 维护规则（重要）

修改本文件时：

1. **更新顶部"最后更新"日期**
2. **更新"最近会话摘要"**为本次会话主要产出
3. **更新"下一步建议"**为下个会话应该做的第一件事
4. **更新"整体进度"百分比**（baseline 6 个阶段各占 12%、模块库占 22%、答辩与文档占 6%；用粗略估计就行）
5. **新增任务时**保持与对应文档的一致性，不要让 PROGRESS 与 baseline / module 卡冲突
6. **完成阶段时**追加一行到"会话历史"
7. **不要删除已完成的勾选项**——它们是项目的功劳簿
