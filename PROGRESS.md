# PROGRESS.md · 项目进度追踪

> **这份文档是项目的"实时进度仪表盘"**。任何 AI 助手进入项目时，先读 [`AGENTS.md`](AGENTS.md)，再读这份，立刻就知道项目走到了哪一步、下一步该做什么、需要看哪些文件。

---

## ⏱️ 顶部状态

| 项 | 值 |
|---|---|
| **最后更新** | 2026-05-19 |
| **当前阶段** | 🟦 M3 已基本收尾（headless-first）；NEAT 主干、multi-seed 验证与 SpeciesObserver 已就位，下一步进入 M4 调研与实现准备 |
| **整体进度** | ▰▰▰▰▰▰▱▱▱▱ 约 56% |
| **最近会话摘要** | 已完成 M3 主干与收尾：NeatGenome / Speciation / NeatBrain / NeatEvolution / `brain.type = neat` headless 接入；新增 `evolution.csv`、`species_log.jsonl`、`run_m3_acceptance.py`；完成 `5 seed × 100/200 代` 验证；修复 `addNodeMutation()` 的悬空引用 bug 并补回归测试 |
| **下一步建议** | 按既定流程开始 M4：先读 `docs/baseline/M4_async.md` 与相关资料，确定多线程评估 / 异步决策 / perf observer 的最小落地方案，再开始实现 |

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
> 状态：⬜⬜⬜

- [ ] EvaluationPool 多线程
- [ ] 异步个体决策周期（基因可进化）
- [ ] 能量消耗系统
- [ ] 线程安全审计（InnovationTracker 加锁、thread_local RNG）
- [ ] PerformanceObserver
- [ ] 训练时长加速 ≥ 3 倍
- [ ] 决策频率分化可观察
- [ ] git tag `baseline-M4-done`

---

### 🟦 M5 · 真实 2.5D 地形

> 详细文档：[`docs/baseline/M5_terrain.md`](docs/baseline/M5_terrain.md)
> 状态：⬜⬜⬜

- [ ] TerrainLayer + HeightField + CaveMask
- [ ] PerlinNoiseGenerator（默认地形生成）
- [ ] TerrainPerception（5 维输出接入感知）
- [ ] 移动速度 / 能量消耗按地形修正
- [ ] 视野遮挡（raycasting + 高度比较）
- [ ] 山洞机制（进入/离开事件）
- [ ] IsometricProjection 渲染（不做真 3D）
- [ ] 资源-地形耦合分布
- [ ] **接口压力测试 #2**：core 主框架 0 修改
- [ ] 跑 100 代后地形分化可观察
- [ ] git tag `baseline-M5-done` + `baseline-complete`

---

## 🟨 模块库进度

> 完成 baseline 后从这里挑模块加。
> 完整推荐路径见 [`docs/modules/README.md`](docs/modules/README.md) 末尾。

### Stage A · 答辩底线（必做）
- [ ] **F-TREE** 物种树 → [`docs/modules/F-TREE_phylogenetic.md`](docs/modules/F-TREE_phylogenetic.md)
- [ ] **F-HEAT** 行为热力图 → [`docs/modules/F-HEAT_heatmap.md`](docs/modules/F-HEAT_heatmap.md)
- [ ] **F-GOD** 上帝按钮 → [`docs/modules/F-GOD_god_panel.md`](docs/modules/F-GOD_god_panel.md)

### Stage B · 中等亮点（争取做 1-2 个）
- [ ] **F-NEUROMOD** 神经调节情绪
- [ ] **F-COMM** 通信信号场
- [ ] **F-LEARN** 拉马克开关

### Stage C · 王炸彩蛋（运气项）
- [ ] **F-COOP** 合作机制（围猎涌现）
- [ ] **F-LLM** LLM 解说员

### Stage D · 锦上添花
- [ ] **F-FORK** 平行宇宙存档
- [ ] **F-COMPARE** 对照实验

### 其它（备选）
- [ ] F-FIGHT / F-PRED / F-TOOL / F-NOVELTY
- [ ] F-DAY / F-SEASON / F-DISEASE / F-RESOURCE / F-CAVE
- [ ] F-STAT / F-REPLAY / F-CURRICULUM
- [ ] F-COEVO / F-NSGA

---

## 🆕 临时任务（用户随时插入）

> 当用户在某次会话提出新需求/想法且不属于现有阶段任务时，记在这里。
> 完成后归档到对应阶段，或长期不消化的转为 ADR。

格式：`- [状态] 任务描述（来源：日期 / 用户原话简述）`

_目前无_

---

## 📝 会话历史

> 每次会话结束时简要追加一条。让接力的下一个 AI 知道前面发生了什么。

| 日期 | 会话摘要 | 产出 | 停在哪 |
|---|---|---|---|
| 2026-05-18 | 第 1 次会话：项目立项 + 文档体系 | 顶层 README/ROADMAP/ARCHITECTURE/DESIGN_DECISIONS/GLOSSARY；4 份 ADR；6 份 baseline 阶段卡；24 份模块卡；工程占位文件；AGENTS.md；PROGRESS.md | 准备进入 M0 实施 |
| 2026-05-18 | 第 2 次会话：完成 M0 实施 | FetchContent 依赖接入；基础类型；12 个核心接口；EventBus；Config；Logger；Headless/Interactive Runtime 骨架；CLI；24 个单测；Release 构建通过；`--hello` 与 headless smoke test 通过 | 开始 M1 实现 |
| 2026-05-18 | 第 3 次会话：完成 M1 核心实现 | FixedTopologyGenome/Brain；Agent / Perception / Capability；World2D + AgentLayer + ResourceLayer；ClassicGA；Reward；FitnessRecorder；真实 Headless 代际循环；新增测试后总计 32 个全过；12 代真实试跑已看到 fitness 明显上升 | 待做 100 代正式验收与调参 |
| 2026-05-18 | 第 4 次会话：补齐 M1 验收工具并完成多 seed 基线测量 | `plot_fitness.py`；`run_m1_acceptance.py`；默认配置 5 seed × 100 代基线（平均倍率 2.256x）；经验配置 5 seed × 100 代复核（平均倍率 1.929x）；`configs/m1_acceptance.toml`；文档数值改为“示例/起始猜测”口径 | 待补更可解释的验收指标并继续调参 |
| 2026-05-18 | 第 5 次会话：确认延后可视化并改路线 | ADR-0005；ROADMAP/PROGRESS 同步为“先机制、后可视化整合”；下一阶段确定为 M3 headless-first | 待开始 M3 NEAT 基础数据结构 |
| 2026-05-19 | 第 6 次会话：完成 M3 实现、验收与回归保护 | `NodeGene/ConnectionGene/InnovationTracker/NeatGenome/NeatBrain/Speciation/NeatEvolution`；`brain.type = neat` headless 接入；`evolution.csv` + `species_log.jsonl`；`run_m3_acceptance.py`；`5 seed × 100/200 代` 验证；修复 `addNodeMutation()` 悬空引用 bug 并补 regression test | 准备进入 M4 调研与实现 |

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
2. **NEAT 输入维度不能写死**——M5 加地形会扩展，F-COMM 还会再扩展
3. **EventBus 用 deferred queue**，避免回调死锁
4. **每个 agent 一个独立 RNG（thread_local）**，不共享全局
5. **NEAT 物种 ID 永远递增不复用**，否则物种树会画错

### 实施相关
1. **Windows 本机没有全局 `cmake` 在 PATH**，但 VS2022 BuildTools 自带的 `cmake.exe` / `ninja.exe` 可直接用
2. 当前已验证的构建链：**VS2022 BuildTools + MSVC 14.44 + CMake 3.31（BuildTools 自带）**
3. `git tag` 尚未执行；代码、构建、测试状态已就绪，可在后续有 git 写操作时补打 `baseline-M0-done`
4. M1 当前卡点已从“实现”转为“验收与调参”——已完成 5 seed × 100 代默认配置测量，平均倍率 `2.256x`
5. 文档里的具体数值（tick、种群规模、reward 权重、倍率目标）都只应视为**示例/起始猜测**，后续以真实实验结果为准
6. `FoodReward` 已修正为按“吃到的食物个数”计 reward；修正前后的 fitness 绝对值不能直接横向比较

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
