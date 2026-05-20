# STATUS.md · Neural Eco 全仓交接手册

> 最后巡检：2026-05-20
> 巡检范围：`d:/claude-code/c++` 整个工作树（不是只看已提交 HEAD）
> 当前分支：`master`
> 当前 HEAD：`54d6771 refactor: advance protagonist continuous-time runtime`
> 交接目标：让新的对话能够在**不依赖旧上下文**的情况下，快速接手当前仓库真实状态。

---

## 1. 先看这一页结论

### 1.1 这个仓库现在到底是什么

这是一个 **C++ 神经进化生态系统项目**，已经形成了两条并行但共享底座的开发线：

- **主仓主线**：`src/` 下的通用生态系统框架
- **当前主要工作线**：`routes/protagonist_ecology/` 下的 protagonist 平行路线

当前真实开发重心已经明显转向 **protagonist route**。

### 1.2 新对话最重要的五个事实

1. **代码现实比 git log 更新**
   - `git log` 的 HEAD 还停在 continuous-time 相关提交。
   - 但工作树里已经有大量**未提交**的 protagonist 记忆 / DNC / mixed compute / 文档更新。
   - **接手时必须先看工作树，不要只看提交历史。**

2. **主线 baseline 机制基本已成，M2 可视化仍然明确延后**
   - `docs/adr/0005-defer-visualization-until-mechanics-stabilize.md` 已把执行顺序改成机制优先。
   - `src/viz/` 仍然基本是空骨架，不应误判为“UI 已完成”。

3. **protagonist route 已经不是空壳实验，而是当前主力工程**
   - 已具备：训练/preview/eval 入口、background species 共同训练、trace/resume、continuous-time 主链、DNC 记忆主链、mixed compute 第一轮底座。

4. **protagonist 当前默认主线不是开训，而是 Memory Phase A 真闭环**
   - 目前最合理默认任务：
     - `SpatialMemoryBank` 语义拆槽与写入补全
     - `MemoryCognitionPerception` / episodic / social query 真接回 brain
     - `GoalManager` 最小闭环
   - **不要在未通知用户的情况下直接开始正式训练。**

5. **GPU 路线代码已落，但本机 CUDA 工具链不完整**
   - protagonist dense GPU backend 已有代码和 CMake 路径。
   - 但本机当前 `nvcc` 不可用，GPU 路径尚未真正编入构建。

---

## 2. 新对话接手时推荐阅读顺序

### 2.1 最小阅读顺序

1. `AGENTS.md`
2. `PROGRESS.md`
3. `docs/ROADMAP.md`
4. `docs/ARCHITECTURE.md`
5. `cb/STATUS.md`（本文件）

### 2.2 如果接的是 protagonist route

继续读：

1. `routes/protagonist_ecology/docx/PROGRESS.md`
2. `routes/protagonist_ecology/docx/MEMORY_SYSTEM_MASTERPLAN.md`
3. `routes/protagonist_ecology/docx/MEMORY_PHASE_A_EXECUTION_PLAN.md`
4. `routes/protagonist_ecology/docx/COMPUTE_SCALING_GPU_CPU_PLAN.md`
5. `routes/protagonist_ecology/docx/CONTINUOUS_TIME_WORLD_DESIGN.md`
6. `docs/experience_log.md`

### 2.3 接手第一件事不是写代码，而是确认现实状态

建议先做：

1. 看 `git status --short`
2. 看 `PROGRESS.md` 顶部摘要
3. 看 `routes/protagonist_ecology/docx/PROGRESS.md` 顶部摘要
4. 确认当前要继续的是：
   - protagonist Memory Phase A
   - protagonist CT-2/CT-3 持续动作语义
   - 或 protagonist mixed compute 后续验证

---

## 3. 仓库顶层结构总览

### 3.1 根目录重要项

| 路径 | 作用 | 当前状态 |
|---|---|---|
| `AGENTS.md` | AI 助手协作手册 | **最高优先级入口** |
| `PROGRESS.md` | 全仓总进度仪表盘 | 持续更新中 |
| `README.md` | 项目对外介绍 | 可读，但不如 `PROGRESS.md` 权威 |
| `CMakeLists.txt` | 顶层构建入口 | 已接 protagonist route + optional CUDA |
| `cmake/Dependencies.cmake` | 第三方依赖拉取 | FetchContent 管理 toml++ / spdlog / gtest / raylib |
| `src/` | 主仓核心代码 | 主系统底座 |
| `routes/` | 并行路线 | 当前只有 `protagonist_ecology/` |
| `configs/` | TOML 配置库 | 很丰富，含主线与 protagonist route |
| `docs/` | 文档系统 | 很完整，且对接手极重要 |
| `tests/` | 单测与 runtime smoke | 当前以 `test_core` 聚合 |
| `scripts/` | Python 离线分析脚本 | 非主代码 |
| `data/` | 运行输出目录 | 当前多为空目录占位 |
| `build/` | 本机构建产物 | **非权威来源** |
| `cb/` | 本次 survey 生成的交接目录 | 本次新建 |
| `.windsurf/` | IDE / workflow 相关 | 非主逻辑 |
| `extern/` | 预留第三方目录 | 当前基本空 |
| `.tavily-state.json` | 工具状态文件 | 非项目主逻辑 |
| `$testDir/` | 环境残留目录 | 非主逻辑，接手时可忽略 |

### 3.2 仓库性质判断

- **语言**：C++20 为主，少量 Python 脚本
- **构建系统**：CMake + FetchContent
- **测试框架**：Google Test
- **渲染路线**：raylib（但当前实际可视化层未做完）
- **项目形态**：
  - 一个主可执行：`neural-eco`
  - 一个 route 可执行：`neural-eco-protagonist`
  - 一个聚合测试可执行：`test_core`

---

## 4. 文档系统：哪些是“权威”，哪些是“理想图”

### 4.1 当前最权威的文档

| 文档 | 权威级别 | 用途 |
|---|---|---|
| `AGENTS.md` | 最高 | AI 协作规则、用户偏好、默认决策方式 |
| `PROGRESS.md` | 最高 | 全仓当前状态、会话历史、下一步建议 |
| `routes/protagonist_ecology/docx/PROGRESS.md` | 最高（route 内） | protagonist 路线当前真实主线 |
| `docs/experience_log.md` | 很高 | 记录真实踩坑、验证、实验结果 |
| `docs/ROADMAP.md` | 高 | 设计节奏与阶段哲学 |
| `docs/ARCHITECTURE.md` | 高 | 代码结构和扩展点 |

### 4.2 “设计目标 / 研究方向”文档

这些文档很重要，但它们描述的是**目标架构或中长期路线**，不应直接当成“已经实现”：

- `docs/modules/*.md`
- `docs/EMERGENT_BEHAVIORS.md`
- `docs/SYSTEM_IMPROVEMENT_PLAN.md`
- `routes/protagonist_ecology/docx/WORLD_UNDERSTANDING_RESEARCH_AND_ENGINEERING_PLAN.md`
- `routes/protagonist_ecology/docx/MEMORY_SYSTEM_MASTERPLAN.md`
- `routes/protagonist_ecology/docx/COMPUTE_SCALING_GPU_CPU_PLAN.md`

### 4.3 非常重要的“现实校正文档”

| 文档 | 作用 |
|---|---|
| `docs/EMERGENCE_AUDIT.md` | 明确指出“模块文档里声称的完成度”与“真实涌现证据”之间的差距 |
| `docs/SYSTEM_IMPROVEMENT_PLAN.md` | 记录主仓机制之间还缺哪些联动和连续跟踪 |

### 4.4 文档与代码存在的已知漂移

1. `src/viz/README.md` 写了 `i18n/renderer/ui/plot`，但当前 `src/viz/` 实际只有空骨架目录。
2. `tests/README.md` 写了 `viz/integration/fixtures`，但当前 `tests/` 实际只有 `core/` 与 `runtime/`。
3. `routes/protagonist_ecology/docx/PROGRESS.md` 的旧审计段里仍有“`MemoryCognitionPerception` 当前只有 12 维”的描述，但**代码现实已经是 44 维**：
   - `routes/protagonist_ecology/perception/MemoryCognitionPerception.h`
4. 模块库 `docs/modules/README.md` 与 `docs/EMERGENCE_AUDIT.md` 的口径不同：
   - 前者偏“机制已接线 / MVP”
   - 后者偏“是否有真实涌现证据”

接手时应默认：

**代码 > route PROGRESS > PROGRESS > 审计/设计文档 > git tag / git log**

---

## 5. 构建、测试与运行现实

### 5.1 构建系统

### 顶层入口

- `CMakeLists.txt`
- `src/CMakeLists.txt`
- `routes/protagonist_ecology/CMakeLists.txt`
- `tests/CMakeLists.txt`

### 依赖

来自 `cmake/Dependencies.cmake`：

- `tomlplusplus`
- `spdlog`
- `googletest`
- `raylib`（仅在 `NEURAL_ECO_BUILD_VIZ=ON` 时）

### 当前构建开关

在顶层 `CMakeLists.txt`：

- `NEURAL_ECO_BUILD_TESTS`
- `NEURAL_ECO_BUILD_VIZ`
- `NEURAL_ECO_ENABLE_PROTAGONIST_CUDA`

### 5.2 可执行与库

| 目标 | 位置 | 作用 |
|---|---|---|
| `neural-eco` | `src/main.cpp` | 主仓 CLI，`--mode headless|interactive` |
| `neural-eco-protagonist` | `routes/protagonist_ecology/main.cpp` | protagonist 路线单独入口 |
| `test_core` | `tests/CMakeLists.txt` | 当前所有单测和 runtime smoke 聚合可执行 |
| `neural_eco::core` | `src/core/` | 主系统纯逻辑库 |
| `neural_eco::runtime_headless` | `src/runtime/headless/` | headless 训练运行时库 |
| `neural_eco::runtime_interactive` | `src/runtime/interactive/` | interactive 运行时骨架 |
| `neural_eco::route_protagonist` | `routes/protagonist_ecology/` | protagonist route 静态库 |

### 5.3 测试现实

本次 survey 期间用 `ctest -N -C Release --test-dir build` 发现：

- 当前已注册 **73 个测试**
- 测试覆盖主要集中在：
  - Agent 决策周期
  - EventBus
  - Config
  - Fixed / NEAT brain
  - NEAT evolution / speciation
  - Communication / continuous eat
  - Terrain
  - EvaluationPool
  - HeadlessRuntime smoke

### 5.4 运行入口现实

#### 主仓

- 入口：`src/main.cpp`
- CLI：`--hello` / `--mode` / `--config`
- 支持模式：
  - `headless`
  - `interactive`

#### protagonist route

- 入口：`routes/protagonist_ecology/main.cpp`
- CLI：仅 `--config`
- 默认配置：`configs/protagonist_ecology_bootstrap.toml`

### 5.5 一个容易误判的点

`configs/default.toml` 默认：

- `[runtime].mode = "interactive"`

但当前 `interactive` / `viz` 仍然是**骨架状态**，所以：

- 新接手时不要把“默认 interactive”误认为“可视化已经完成”
- 主仓实际稳定工作方式仍然是 **headless-first**
- protagonist route 也是 **headless / preview / smoke** 为主

---

## 6. 主仓主线：真实架构与机制到文件映射

## 6.1 顶层三层结构

文档设计与代码现实基本一致：

- `core/`：纯逻辑层，不依赖 viz/runtime
- `runtime/`：组装层
- `viz/`：理论上为可视化层，当前仍处于延后状态

关键文件：

- `docs/ARCHITECTURE.md`
- `src/core/README.md`
- `src/runtime/README.md`
- `src/viz/README.md`

## 6.2 主仓入口与调度

### 机制：CLI 分发

- **作用**：解析命令行并启动 headless 或 interactive runtime
- **主文件**：`src/main.cpp`
- **依赖**：`core/config`、`core/logging`、`runtime/headless`、可选 `runtime/interactive`
- **被依赖方**：最终可执行 `neural-eco`

### 机制：headless runtime

- **作用**：执行训练循环、评估、记录 metrics、快照、繁殖
- **主文件**：
  - `src/runtime/headless/HeadlessRuntime.h`
  - `src/runtime/headless/HeadlessRuntime.cpp`
- **相关文件**：`src/runtime/headless/EvaluationPool.*`
- **依赖**：几乎整个 `core`
- **被依赖方**：`src/main.cpp`、route 也复用 `EvaluationPool`

### 机制：interactive runtime（当前骨架）

- **作用**：理论上承接 UI / viz 演示
- **主文件**：`src/runtime/interactive/InteractiveRuntime.*`
- **现实状态**：骨架，不应当成完整演示入口

## 6.3 配置系统

### 机制：TOML 配置读取

- **作用**：统一加载嵌套 key 配置
- **主文件**：`src/core/config/Config.*`
- **配置库目录**：`configs/`
- **依赖**：`tomlplusplus`
- **被依赖方**：主仓 runtime、protagonist route、测试

说明：

- 主仓默认模板：`configs/default.toml`
- protagonist route 有自己一整套配置文件，不完全复用 `default.toml`

## 6.4 日志系统

### 机制：spdlog 封装

- **主文件**：`src/core/logging/Logger.*`
- **作用**：统一 console/file 输出
- **被依赖方**：主仓与 route 几乎所有入口

## 6.5 事件系统

### 机制：Deferred EventBus

- **作用**：发布-订阅跨模块通信，`publish()` 入队，`flush()` 真正派发
- **主文件**：
  - `src/core/events/EventBus.h`
  - `src/core/events/EventBus.cpp`
  - `src/core/events/Event.h`
  - `src/core/events/EventType.*`
  - `src/core/events/EventPayloads.h`
- **关键约束**：避免订阅者回调中再次发布造成重入问题
- **被依赖方**：world、observers、trace、metrics、runtime

## 6.6 世界模型

### 机制：World2D + 多层 Layer

- **作用**：作为世界容器，按 tick / dt 推进各层
- **主文件**：
  - `src/core/world/World2D.*`
  - `src/core/world/layers/*`
- **关键行为**：`World2D::addLayer()` 按 `tickOrder()` 稳定排序；`World2D::tick()` 推进 layers 并驱动 event bus

### 核心世界层

| 机制 | 主文件 | 作用 |
|---|---|---|
| `AgentLayer` | `src/core/world/layers/AgentLayer.*` | 管理 agent 生命周期与决策推进 |
| `ResourceLayer` | `src/core/world/layers/ResourceLayer.*` | 食物资源与再生 |
| `SignalLayer` | `src/core/world/layers/signal/*` | 信号场扩散/衰减 |
| `DiseaseLayer` | `src/core/world/layers/DiseaseLayer.*` | 疾病与病原体传播 |
| `ObjectLayer` | `src/core/world/layers/object/*` | 可移动物体 |
| `TerrainLayer` | `src/core/world/layers/terrain/*` | 高度图、坡度、山洞 |
| `TimeOfDayLayer` | `src/core/world/layers/TimeOfDayLayer.*` | 昼夜循环 |
| `SeasonLayer` | `src/core/world/layers/SeasonLayer.*` | 季节循环 |
| `HeavyResourceLayer` | `src/core/world/layers/HeavyResourceLayer.*` | 合作资源机制 |

### 当前现实补充

`AgentLayer` 已不再只是串行 step：

- 新增 **decision parallelism** 底座
- 文件：`src/core/world/layers/AgentLayer.*`
- 作用：并行 `decide()`，串行 `publish/apply`

## 6.7 Agent / Perception / Capability

### 机制：Agent 基本体

- **主文件**：`src/core/agent/Agent.*`
- **当前现实**：`step()` 已被拆为：
  - `decide()`
  - `publishDecisionEvent()`
  - `applyCapabilities()`
- **原因**：为 protagonist mixed compute / agent decision 并行铺路

### 核心感知组件目录

- `src/core/perception/`

当前已有代表性组件：

- `SelfPerception`
- `NearestFoodPerception`
- `FoodScentPerception`
- `SignalPerception`
- `TerrainPerception`
- `InfectionPerception`
- `NearbyObjectPerception`
- `TimeOfDayPerception`
- `SeasonPerception`

### 核心能力组件目录

- `src/core/capability/`

当前已有代表性组件：

- `MoveCapability`
- `EatCapability`
- `AttackCapability`
- `EmitSignalCapability`
- `PickUpCapability`
- `DropCapability`
- `PredatorHuntCapability`
- `FleeCapability`
- `MateCapability`
- `TerrainModifier`
- `HebbianLearning`

## 6.8 大脑与进化

### 固定拓扑脑

- **目录**：`src/core/brain/fixed/`
- **作用**：M1 的基础 brain 路线

### NEAT / CTRNN 脑

- **目录**：`src/core/brain/neat/`
- **核心文件**：
  - `NeatGenome.*`
  - `NeatBrain.*`
  - `InnovationTracker.*`
- **作用**：主仓可进化拓扑核心

### 进化策略

- **目录**：`src/core/evolution/`
- **核心文件**：
  - `ClassicGA.*`
  - `NeatEvolution.*`
  - `Nsga2Evolution.*`
  - `speciation/Speciation.*`
  - `novelty/*`
  - `selection/*`

### 当前现实补充

`NeatEvolution` / `Speciation` 已支持状态导出导入：

- `NeatEvolutionState`
- `SpeciationState`

这为主仓和 protagonist route 的 resume 能力提供底座。

## 6.9 观测与指标

### 目录

- `src/core/observer/`

### 核心 observer

| 机制 | 主文件 | 输出 |
|---|---|---|
| FitnessRecorder | `FitnessRecorder.*` | `fitness.csv` |
| SpeciesObserver | `SpeciesObserver.*` | `species_log.jsonl` 等 |
| PerformanceObserver | `PerformanceObserver.*` | `perf.csv` |
| ModuleMetricsObserver | `ModuleMetricsObserver.*` | `module_metrics.csv` |
| HeatmapObserver | `HeatmapObserver.*` | 热力图输出目录 |
| AdvancedStatsObserver | `AdvancedStatsObserver.*` | 高级统计 CSV |
| EmergenceAuditor | `EmergenceAuditor.*` | 涌现审计输出 |
| ComprehensiveMetrics | `ComprehensiveMetrics.*` | OEE / info / coevo / disease 统计 |

其中 `ComprehensiveMetrics` 是主仓目前较重的多维统计总观察器。

## 6.10 快照与恢复

### 机制：SnapshotIO

- **主文件**：`src/core/snapshot/SnapshotIO.*`
- **作用**：NEAT population + innovation tracker 快照存取
- **被依赖方**：主仓 headless runtime；protagonist route 也在部分 resume 语义上复用思路

---

## 7. protagonist route：当前真实主线与机制映射

## 7.1 route 总定位

`routes/protagonist_ecology/` 已经发展成一个**仓内独立项目**：

- 有自己的 `main.cpp`
- 有自己的 config schema
- 有自己的 runtime / evolution / world factory / trace / docx
- 但继续复用主仓 `core/` 作为基础设施

### route 目录结构

| 目录 | 作用 |
|---|---|
| `brain/` | protagonist brain、DNC、显式记忆、genome、GPU dense backend |
| `capability/` | protagonist 专有动作能力 |
| `perception/` | protagonist 专有感知 |
| `runtime/` | route config / world factory / evolution / training runtime / trace |
| `world/` | route 专有世界层（基地、营火、worksite、补给、干预等） |
| `docx/` | route 规划、研究、阶段进度文档 |

## 7.2 route 入口与配置

### 机制：route CLI

- **主文件**：`routes/protagonist_ecology/main.cpp`
- **作用**：读取指定 config，构造 `ProtagonistEcologyRuntime`

### 机制：route 配置

- **主文件**：`routes/protagonist_ecology/runtime/ProtagonistEcologyConfig.*`
- **作用**：把 protagonist route 的训练/preview/eval/live 配置统一解析成结构化 config

### 当前 route config 覆盖范围

- world role / scenario name
- simulation time（seconds + substep）
- compute（GPU 开关 / agent decision threads）
- terrain / resources / signal / objects
- base / nursery / supply / progression / campfire / water / survival / tree / worksite
- day / season / disease
- protagonist brain topology
- background species slots

## 7.3 route 世界装配

### 机制：ProtagonistWorldFactory

- **主文件**：`routes/protagonist_ecology/runtime/ProtagonistWorldFactory.cpp`
- **作用**：按 route config 装出完整世界

### 当前实际装配的 route layers

从 `createBootstrapWorld()` 可见，当前会按配置挂载：

- `ResourceLayer`
- `AgentLayer`
- `TerrainLayer`
- `BaseLayer`
- `ObjectLayer`
- `TreeLayer`
- `SignalLayer`
- `CampfireLayer`
- `WaterLayer`
- `SurvivalStatusLayer`
- `WorksiteLayer`
- `InterventionDirectorLayer`
- `NurserySupplyLayer`
- `NurseryLogisticsLayer`
- `NurseryProgressionLayer`
- `TimeOfDayLayer`
- `SeasonLayer`
- `DiseaseLayer`
- `SandboxStatusLayer`

这说明 protagonist route 已经不是“一个脑 + 一个空世界”，而是带训练环境机制链的完整 sandbox。

## 7.4 route 人口与装配

### 机制：BootstrapPopulation

- **主文件**：`routes/protagonist_ecology/runtime/BootstrapPopulation.cpp`
- **作用**：
  - 计算 protagonist 感知维度
  - 构造 protagonist brain / agent
  - 装配 background species
  - 支持 bootstrap / training population seed

### 当前重要事实

- `protagonistSensoryDim()` 是 protagonist 输入维度的统一口径
- `makeProtagonistAgent()` 支持从 genome 构造 protagonist agent
- `seedBootstrapPopulation()` 与 `seedTrainingPopulation()` 已分开
- GPU dense 开关会从这里传进 `ProtagonistBrain`

## 7.5 route runtime 主流程

### 机制：ProtagonistEcologyRuntime

- **主文件**：`routes/protagonist_ecology/runtime/ProtagonistEcologyRuntime.cpp`
- **作用**：route 的真正核心 orchestration

其职责已经包括：

- preview / training 分支
- 多 episode per generation 评估
- background species 共同训练
- reward 汇总
- behavior 统计
- trace 记录
- checkpoint / resume
- best genome continuity
- continuous-time 子步推进
- route 级线程预算协调

### 当前 runtime 参数非常丰富

`TrainingParams` 已经覆盖：

- generations / episode duration / substep dt
- episodes_per_generation / num_threads
- survival / food / intrinsic / combat / cooperation / communication reward 权重
- signal 与 joint hunt 参数
- checkpoint_every
- protagonist memory 开关
- background NEAT 进化参数

## 7.6 protagonist brain 与记忆系统

### 机制：ProtagonistBrain

- **主文件**：
  - `routes/protagonist_ecology/brain/ProtagonistBrain.h`
  - `routes/protagonist_ecology/brain/ProtagonistBrain.cpp`
- **作用**：protagonist 行为控制核心

### 当前结构现实

`ProtagonistBrain` 现在已经不是单纯 dense MLP，而是：

- 外部感知输入
- DNC external memory read vectors
- hidden trunk
- action outputs
- DNC interface controller
- 显式 memory banks 的辅助接口

### DNC 主链

- **主文件**：`routes/protagonist_ecology/brain/DNCMemory.*`
- **能力**：
  - content lookup
  - allocation weighting
  - erase-write
  - temporal links
  - multi-read-heads
  - usage tracking

### 显式记忆层

- `MemoryBank.*`
- `SpatialMemoryBank.*`
- `EpisodicMemoryBank.*`
- `SocialMemoryBank.*`
- `GoalMemory.*`

### 重要现实状态

- `MemoryCognitionPerception` 当前**代码维度是 44**，不是旧文档里的 12
- `ProtagonistBrain::dncSummary()` 暴露：
  - `usage_mean`
  - `usage_max`
  - `write_peak`
  - `read_peak`
  - `read_norm`
  - `precedence_peak`
  - `write_focus_slot`
  - `read_focus_slot`

### 当前真正的缺口

- Spatial 语义槽仍需进一步拆细 / 补全写入
- episodic / social 查询尚未完全闭回行为决策主干
- goal manager 仍未成为强闭环
- DNC 目前已被观测，但训练任务还没有真正“强迫它去用检索”

## 7.7 protagonist 专有感知

### 目录

- `routes/protagonist_ecology/perception/`

### 当前主要组件

- `AgentSocialPerception`
- `BasePerception`
- `CarriedObjectPerception`
- `DangerPerception`
- `MemoryCognitionPerception`
- `ObjectLogisticsPerception`
- `StockpilePerception`
- `SurvivalStatePerception`
- `TreePerception`
- `WaterPerception`
- `WeaponPerception`
- `WorksitePerception`

## 7.8 protagonist 专有能力

### 目录

- `routes/protagonist_ecology/capability/`

### 当前主要组件

- `DrinkWaterCapability`
- `NurseryPickUpCapability`
- `ChopTreeCapability`
- `CraftSpearCapability`
- `ThrowCarriedObjectCapability`
- `ProtagonistHuntCapability`

这说明 protagonist route 已经能覆盖：

- 饮水
- 拾取 / 搬运
- 砍树
- 制矛
- 投掷
- 狩猎

## 7.9 protagonist 路线 world 层

### 目录

- `routes/protagonist_ecology/world/`

### 主要层

- `BaseLayer`
- `CampfireLayer`
- `InterventionDirectorLayer`
- `NurseryLogisticsLayer`
- `NurseryProgressionLayer`
- `NurserySupplyLayer`
- `SandboxStatusLayer`
- `SurvivalStatusLayer`
- `TreeLayer`
- `WaterLayer`
- `WorksiteLayer`

这些层共同构成 protagonist 的“原始聚落训练环境”。

## 7.10 protagonist trace / checkpoint / resume

### 机制：TraceRecorder

- **主文件**：`routes/protagonist_ecology/runtime/TraceRecorder.*`
- **作用**：episode 级 trace 输出

### 当前 trace 产物

- `event_trace.jsonl`
- `agent_trace.csv`
- `world_trace.csv`
- `replay_manifest.json`

### 当前 checkpoint / resume 状态

根据 route progress 与相关实现：

- protagonist population 可恢复
- background species population 可恢复
- innovation tracker 可恢复
- evolution RNG 可恢复
- speciation state 可恢复
- generation+1 resume 语义已接通
- best genome continuity 已接通

这是 protagonist route 当前已经比较扎实的一块。

---

## 8. protagonist route 当前主线：代码与文档交叉后的真实判断

### 8.1 已经基本成型的部分

1. **训练/preview/eval runtime**：已成型
2. **共同训练生态**：已成型
3. **trace/resume**：已成型
4. **continuous-time CT-0 / CT-1 + CT-2A 部分**：已成型
5. **DNC 主记忆链**：已成型
6. **mixed compute 第一轮工程化**：已成型但未完成 GPU 实证

### 8.2 当前最值得继续的默认任务

默认应继续：

1. `Memory Phase A`
   - Spatial 语义拆槽
   - MemoryCognitionPerception / episodic / social 真正闭环
   - GoalManager 最小闭环
2. `CT-2/CT-3`
   - `chop / build / worksite credit / event queue` 的更强连续语义
3. 之后才是：
   - 进一步 profiling
   - GPU smoke profiling
   - 更重的 compute 优化

### 8.3 当前不应默认做的事

1. **不要直接开正式训练**
   - 用户明确要求：开始正式训练前需要停下通知他。
2. **不要把 mixed compute 误判为已经 GPU 成功**
   - 当前只是代码接好了，环境没打通。
3. **不要把当前 Memory V2 误判为已经完整闭环**
   - 现在仍属于“记忆主链已接入，训练闭环尚未完全成立”。

---

## 9. mixed compute 现状（本次 survey 必须单独写清）

这部分我参考了本仓工作树与最近一次 mixed compute 状态记录，并以代码现状为准重新核对。

### 9.1 已落地的内容

1. `Agent` 已拆分：
   - `decide()`
   - `publishDecisionEvent()`
   - `applyCapabilities()`

2. `AgentLayer` 已加入持久化 decision executor
   - 文件：`src/core/world/layers/AgentLayer.*`
   - 目标：支持 per-world agent decision 并行

3. protagonist route 已新增 compute 配置：
   - `compute.protagonist_dense_gpu_enabled`
   - `compute.agent_decision_threads`

4. `ProtagonistEcologyRuntime` 已开始做：
   - 外层 episode pool
   - 内层 agent decision threads
   的预算联动

5. `ProtagonistBrain` 已接入可选 GPU dense backend：
   - `routes/protagonist_ecology/brain/ProtagonistDenseGpu.h`
   - `routes/protagonist_ecology/brain/ProtagonistDenseGpu.cpp`
   - `routes/protagonist_ecology/brain/ProtagonistDenseGpu.cu`

6. route CMake 已接 optional CUDA 编译路径：
   - 顶层 `CMakeLists.txt`
   - route `routes/protagonist_ecology/CMakeLists.txt`

### 9.2 当前阻塞点

本机当前真实状态：

- `where.exe nvcc` 找不到 `nvcc`
- CMake 侧已经出现过 `CMAKE_CUDA_COMPILER:FILEPATH=NOTFOUND`
- `C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v11.8` 和 `v12.4` 目录存在
- 但目录内容不完整，只有 `extras/`，缺标准 `bin/include/lib`

### 9.3 正确结论

- **CPU 侧并行底座已真实接通**
- **GPU backend 代码与编译路径已写好**
- **GPU 路径尚未真正编入当前 build**
- 如继续 GPU 线，第一步不是改代码，而是先修本机 CUDA 工具链

---

## 10. continuous-time 现状

### 10.1 已成立的部分

根据 route progress、相关文档、代码结构以及最近记录，当前 protagonist route 已完成：

- CT-0：时间接口兼容层
- CT-1：runtime 从 tick 驱动迁到 seconds + substeps
- CT-2A：部分动作已按 `dt` 连续化

已涉及的关键现实：

- `episode_duration_seconds`
- `substep_dt`
- `max_substeps_per_advance`
- dual timestamps（tick + time_seconds）
- route memory / trace / goal / social 系统带双时间戳
- protagonist brain forward 使用 `dt`

### 10.2 当前未收口的部分

还需要继续推进：

- `chop`
- `build`
- `worksite credit`
- `future event / due action` 底座
- 更完整的 continuous reward 积分口径

---

## 11. 模块完成度：如何正确理解“已完成”

### 11.1 不能只看 `docs/modules/README.md`

`docs/modules/README.md` 当前会给出很多 `✅⛳⛳`。

这代表的是：

- 机制或基础设施大多已接线
- 至少达到某种 MVP 可跑状态

但不代表：

- 已证明稳定涌现
- 已有跨 seed 统计证据
- 已达到文档原始“完美标准”

### 11.2 必须同时看 `docs/EMERGENCE_AUDIT.md`

这份文档很关键，因为它明确指出：

- 之前很多“完成”其实只是机制存在
- 而不是“涌现已经被严格验证”

接手时要默认：

- `F-COMM` / `F-COOP` / `F-NEUROMOD` / `F-NOVELTY` / `F-COEVO` / `F-DISEASE` 等很多模块属于
  - **基础设施 / 机制层已做**
  - **涌现证据层未完全补齐**

### 11.3 一个具体例子

- `docs/modules/README.md` 中很多模块是 `✅⛳⛳`
- `docs/EMERGENCE_AUDIT.md` 则说：
  - 真正达到原始文档完美标准的模块数为 0
  - 很多只是“机制 ✅，验证 ❌”

这不是冲突，而是两个层面的定义不同。

---

## 12. 当前工作树状态（非常重要）

## 12.1 branch / HEAD / tag

### 当前 branch

- `master`

### 当前 HEAD

- `54d6771 refactor: advance protagonist continuous-time runtime`

### 当前 tag 概况

已有 tags 包括：

- baseline：
  - `baseline-M0-done`
  - `baseline-M1-done`
  - `baseline-M3-MVP`
  - `baseline-M4-MVP`
  - `baseline-M5-MVP`
  - `baseline-M5-done`
  - `baseline-complete`
- modules：
  - `module-F-COMM-MVP`
  - `module-F-COOP-MVP`
  - `module-F-CURRICULUM-MVP`
  - `module-F-DAY-MVP`
  - `module-F-DISEASE-MVP`
  - `module-F-FIGHT-MVP`
  - `module-F-LEARN-MVP`
  - `module-F-NEUROMOD-MVP`
  - `module-F-NOVELTY-MVP`
  - `module-F-PRED-MVP`
  - `module-F-RESOURCE-MVP`
  - `module-F-SEASON-MVP`

### 正确理解

- tags **不是空的**，但也**跟不上最近 protagonist 路线工作树的真实进度**
- 很多 protagonist 关键能力仍只存在于工作树里，尚未进入新的 commit/tag

## 12.2 当前工作树是 dirty 的

本次 survey 时 `git status --short` 显示：

- 大量 modified 文件
- 多个 untracked 新文件

### 当前未提交改动的大类

#### A. 文档 / 协作状态文件

- `AGENTS.md`
- `PROGRESS.md`
- `docs/DESIGN_DECISIONS.md`
- `docs/experience_log.md`
- `routes/protagonist_ecology/docx/PROGRESS.md`

#### B. protagonist route 核心代码

- `ProtagonistBrain.*`
- `ProtagonistGenome.*`
- `MemoryCognitionPerception.*`
- `SpatialMemoryBank.*`
- `BootstrapPopulation.cpp`
- `ProtagonistEcologyConfig.*`
- `ProtagonistEcologyRuntime.cpp`
- `ProtagonistEvolution.*`
- `ProtagonistWorldFactory.cpp`
- `TraceRecorder.cpp`
- 多个 route world / capability 文件

#### C. core 侧共用底座改动

- `src/core/agent/Agent.*`
- `src/core/capability/EatCapability.*`
- `src/core/capability/PredatorHuntCapability.cpp`
- `src/core/evolution/NeatEvolution.*`
- `src/core/evolution/speciation/Speciation.*`
- `src/core/world/layers/AgentLayer.*`
- `src/core/world/layers/ResourceLayer.*`

#### D. 测试改动

- `tests/core/test_comm.cpp`
- `tests/core/test_neat_evolution.cpp`
- `tests/core/test_speciation.cpp`

#### E. 当前 untracked 的重要新文件

- `docs/adr/0006-protagonist-memory-uses-standard-dnc.md`
- `routes/protagonist_ecology/brain/DNCMemory.cpp`
- `routes/protagonist_ecology/brain/DNCMemory.h`
- `routes/protagonist_ecology/brain/ProtagonistDenseGpu.cpp`
- `routes/protagonist_ecology/brain/ProtagonistDenseGpu.cu`
- `routes/protagonist_ecology/brain/ProtagonistDenseGpu.h`
- `routes/protagonist_ecology/docx/COMPUTE_SCALING_GPU_CPU_PLAN.md`
- `routes/protagonist_ecology/docx/MEMORY_PHASE_A_EXECUTION_PLAN.md`
- `routes/protagonist_ecology/docx/MEMORY_SYSTEM_MASTERPLAN.md`

### 关键提醒

**新对话如果只看 git log，会严重低估当前仓库状态。**

---

## 13. 已验证事实与当前阻塞点

## 13.1 当前可以相对放心的事实

1. 仓库结构清晰，文档体系非常完整。
2. 主仓 `core/runtime` 架构边界基本稳定。
3. protagonist route 已有独立完整生命线：
   - config
   - runtime
   - bootstrap
   - world factory
   - evolution
   - trace/resume
   - DNC memory
   - mixed compute groundwork
4. 测试注册数量已到 73。
5. 最近几轮工作都在持续维护 `PROGRESS.md` 与 `experience_log.md`。

## 13.2 当前主要阻塞点

### A. protagonist Memory Phase A 还没闭环

- 这是**功能主线阻塞**
- 不是构建阻塞

### B. GPU 路线被本机 CUDA 工具链卡住

- 这是**环境阻塞**
- 不是代码设计阻塞

### C. 可视化层仍未进入真正实现

- 这是**战略性延后**
- 不是忘了做

### D. 模块“机制完成”和“涌现证据完成”仍有差距

- 这是**科研/验证层阻塞**
- 不是纯工程代码阻塞

---

## 14. 新对话建议的默认接手策略

## 14.1 如果继续当前最合理主线

默认建议：继续 protagonist route。

### 推荐顺序

1. 先确认工作树 dirty 状态
2. 读 route 进度与规划文档
3. 从 `Memory Phase A` 第一步开始继续

### 默认起点

优先从以下几项中选第一项继续：

1. `SpatialMemoryBank` 语义拆槽与写入补全
2. `MemoryCognitionPerception` / episodic / social query 真正读回决策主干
3. `GoalManager` 最小闭环
4. CT-2/CT-3 的 `chop / build / worksite` 持续进度语义

## 14.2 如果继续算力线

顺序应当是：

1. 不先写 GPU 新功能
2. 先修本机完整 CUDA toolkit
3. 重新配置 CMake，确认 `.cu` 真编入
4. 再做 protagonist dense GPU smoke profiling

## 14.3 如果回到主仓主线

最合理的两条方向是：

1. **M2 可视化整合真正开工**
   - 但这与当前用户最近的主要兴趣相比，优先级偏低
2. **把已有模块的证据链补齐**
   - 与 `docs/EMERGENCE_AUDIT.md` / `SYSTEM_IMPROVEMENT_PLAN.md` 对齐

---

## 15. 接手时不要做的事

1. **不要悄悄开正式训练**
   - 用户明确希望在正式训练前参与。

2. **不要把 protagonist route 当成“小实验分支”**
   - 现在它已经是主要工作区。

3. **不要把 `docs/modules/README.md` 的 ✅ 当成“真实涌现已证明”**
   - 必须结合 `docs/EMERGENCE_AUDIT.md` 看。

4. **不要把当前 mixed compute 当成“已经 GPU 成功”**
   - 代码有，环境没通。

5. **不要只重编 `test_core` 就判断主仓 headless 没问题**
   - 如果改了 `core` 或 `runtime_headless`，应重链 `neural-eco.exe` 再回归。

6. **不要把 `build/` 里的旧产物当成真实状态来源**
   - 以源码、配置、文档和必要的重新构建为准。

---

## 16. 关键文件速查表

### 16.1 总入口

| 你想找什么 | 文件 |
|---|---|
| AI 协作规则 | `AGENTS.md` |
| 全仓当前进度 | `PROGRESS.md` |
| 总体开发节奏 | `docs/ROADMAP.md` |
| 架构设计 | `docs/ARCHITECTURE.md` |
| 设计决策索引 | `docs/DESIGN_DECISIONS.md` |
| 真正踩坑记录 | `docs/experience_log.md` |
| 模块索引 | `docs/modules/README.md` |
| 现实性审计 | `docs/EMERGENCE_AUDIT.md` |

### 16.2 主仓主线

| 机制 | 文件 |
|---|---|
| 主 CLI | `src/main.cpp` |
| Headless runtime | `src/runtime/headless/HeadlessRuntime.*` |
| EventBus | `src/core/events/EventBus.*` |
| World2D | `src/core/world/World2D.*` |
| AgentLayer | `src/core/world/layers/AgentLayer.*` |
| Agent | `src/core/agent/Agent.*` |
| NEAT evolution | `src/core/evolution/NeatEvolution.*` |
| Speciation | `src/core/evolution/speciation/Speciation.*` |
| Comprehensive metrics | `src/core/observer/ComprehensiveMetrics.*` |
| Snapshot | `src/core/snapshot/SnapshotIO.*` |

### 16.3 protagonist route

| 机制 | 文件 |
|---|---|
| Route CLI | `routes/protagonist_ecology/main.cpp` |
| Route config | `routes/protagonist_ecology/runtime/ProtagonistEcologyConfig.*` |
| Route world 装配 | `routes/protagonist_ecology/runtime/ProtagonistWorldFactory.cpp` |
| Population seed | `routes/protagonist_ecology/runtime/BootstrapPopulation.cpp` |
| 主 runtime | `routes/protagonist_ecology/runtime/ProtagonistEcologyRuntime.cpp` |
| Protagonist evolution | `routes/protagonist_ecology/runtime/ProtagonistEvolution.*` |
| Trace | `routes/protagonist_ecology/runtime/TraceRecorder.*` |
| 主角脑 | `routes/protagonist_ecology/brain/ProtagonistBrain.*` |
| DNC | `routes/protagonist_ecology/brain/DNCMemory.*` |
| 显式记忆层 | `routes/protagonist_ecology/brain/SpatialMemoryBank.*` 等 |
| GPU dense backend | `routes/protagonist_ecology/brain/ProtagonistDenseGpu.*` |
| 路线总进度 | `routes/protagonist_ecology/docx/PROGRESS.md` |
| 记忆总设计 | `routes/protagonist_ecology/docx/MEMORY_SYSTEM_MASTERPLAN.md` |
| 记忆 Phase A | `routes/protagonist_ecology/docx/MEMORY_PHASE_A_EXECUTION_PLAN.md` |
| 算力规划 | `routes/protagonist_ecology/docx/COMPUTE_SCALING_GPU_CPU_PLAN.md` |

---

## 17. 最终交接结论

如果新的对话只记住一句话，应该是：

**这个仓库当前真正的现实是：主仓 baseline 机制已经很厚，protagonist route 已成为当前主工作区；下一步默认继续 protagonist 的 Memory Phase A 和 continuous-time 收尾，而不是直接开正式训练，也不是误以为 GPU 已经打通。**
