# Protagonist Ecology · 开发执行指南

> 位置：`routes/protagonist_ecology/docx/DEVELOPMENT_EXECUTION_GUIDE.md`
>
> 作用：这份文档不负责讲所有远景，也不负责记录零散交接；它只负责一件事：**在当前真实代码基础上，决定接下来按什么顺序开发，才能最少绕路。**
>
> 更新时间：2026-05-22
>
> 文档分工：
> - `supporting/REALTIME_USER_REQUIREMENTS.md`：用户要什么
> - `supporting/WORLD_UNDERSTANDING_RESEARCH_AND_ENGINEERING_PLAN.md`：长期总蓝图与 8 个 Phase 的最高口径
> - `supporting/MEMORY_SYSTEM_MASTERPLAN.md`：记忆/主脑总设计口径
> - `STATUS.md` / `PROGRESS.md`：当前真实进度
> - **本文件**：把上面几份和真实代码揉成一条可执行的开发顺序

---

## 0. 先给结论

当前项目最危险的事不是“想得太大”，而是：

- 一边继续堆 `BrainV3 / GPU / live world / 3D`
- 一边还有 `Memory V2 / credit / signal semantics / species role / lifecycle` 没真正收口

这会把项目拖进“每条线都半成品”。

**从最少绕路的角度，后续默认顺序固定为：**

1. **先收口当前已半落地的 cognition-social 主线**
   - `Memory V2` 解释与 query 深化
   - `ContributionLedger / EventLineage` 收口
   - `communication semantics` 真正成立
2. **再做鲜明物种生态角色**（R-005）
3. **再做生命周期与繁殖闭环**（R-006）
4. **再做 train / trial / frontier / live 自动闭环**（R-007）
5. **最后再做开放式增殖能力**
   - online QD / archive
   - world model
   - hierarchical goal
   - POET / 自长机制（R-008）
6. **3D 展示、正式长训、生产世界演化**一律最后

一句话：

> **先把“会记、会分功、会交流、会分角色”做扎实，再谈“会出生繁殖、会自动晋升、会自己长机制”。**

---

## 1. 当前真实代码地基

## 1.1 已经完整或基本完整的部分

这些不是想法，是真代码已接通的底座：

- `Trace / replay manifest / checkpoint / resume`
- continuous-time 主链
- protagonist + background species 共同训练
- fixed-task memory ablation 运行入口
- 标准 `DNC`
- `Memory Phase A` 基础 wiring
- `BrainV3` 最小壳层
- mixed compute 第一轮工程化
- `train / eval / live` 角色分离与基础配置

对应真实代码落点：

- `runtime/ProtagonistEcologyRuntime.cpp`
- `runtime/TraceRecorder.cpp`
- `runtime/ProtagonistWorldFactory.cpp`
- `runtime/BootstrapPopulation.cpp`
- `brain/ProtagonistBrain.cpp`
- `brain/DNCMemory.cpp`
- `tests/route_protagonist/test_checkpoint_resume.cpp`
- `tests/route_protagonist/test_memory_ablation_eval.cpp`

## 1.2 已有代码落点但仍是 partial 的部分

这些最容易让人误判成“差不多做完了”，实际上没有：

- `Memory V2`：bank 和 readback 已有，但 query 还不够深
- `ContributionLedger`：建造/围猎已有 share-based 雏形，但还不是统一 event-lineage
- `communication semantics`：基础设施已补很多，但还没形成强语义压力和清晰 listener shift
- `MapElites / QD`：目前是离线 archive，不是在线 reproduction loop
- `world model`：只有 dataset builder，没有 predictor 闭环
- `goal system`：当前还是 rule-based updater，不是 manager/worker
- `GPU`：CUDA build 路径可运行，不等于训练已提速

## 1.3 用户需求里已经确定，但代码还没真正开始的大块

- R-005：鲜明物种生态角色
- R-006：完整生命周期与性繁殖
- R-007：train/trial/frontier/live 自动晋升闭环
- R-008：在 richer pressure 下自长技能/局部机制

## 1.4 当前不要误判的事实

- **没有正式长训结果**
- **没有可交付的 live production world**
- **没有完整 communication semantics**
- **没有完整 lifecycle / reproduction**
- **没有 online QD / POET / world-model-in-brain**
- **没有 3D 演示层**

---

## 2. 最少绕路开发原则

## 2.1 一次只收口一条纵向链，不并行开 4 条大线

所谓“纵向链”，指的是：

`任务 -> 感知 -> 内部状态 -> 决策 -> 可见行为 -> 环境后果 -> 指标/ablation`

如果一条链没有收口，不开下一条大链。

## 2.2 每个新机制必须先写证据链，再写代码

任何机制都先写清楚：

- trigger
- input
- internal change
- decision shift
- visible behavior
- environment consequence
- final metric

没有这条链，就不允许进入代码实现。

## 2.3 每个机制都先过小任务，再进主生态

默认流程固定为：

1. `L0`：单测 / wiring
2. `L1`：不训练脚本验证
3. `L2`：micro-task 短训
4. `L3`：集成 smoke
5. `L4`：正式 mid / long 实验

不允许跳过 `L2/L3` 直接把大机制扔进主生态长训。

## 2.4 route-local 优先，只有共性接口才回写 core

`routes/protagonist_ecology/` 现在是主战场。

优先把：

- task
- eval
- memory query
- reward shaping
- species role
- lifecycle
- closed-loop promotion

都先做成 route-local。

只有满足以下条件才动 `src/core/`：

- 已经确认是跨 route 通用能力
- route-local 写法会造成接口重复
- 后续 2 个以上机制都依赖它

## 2.5 先做用户能看到的行为闭环，不先做抽象“大脑升级”

不要优先追：

- 更大参数量
- 更漂亮架构图
- 更高级词汇

优先追：

- `signal_response` 能不能明显分离
- 物种是不是肉眼看得出不同
- 生命周期是不是改变了群体结构
- 自动晋升是不是少了手动搬 checkpoint

## 2.6 大机制默认先保守，不默认一口气神经化

R-007 / R-008 允许 learned controller，但当前默认原则是：

- 神经网络负责提议、排序、加权、调度
- 硬门槛负责保底、回归、回滚、基线保护

不能把所有裁决都交给一个黑箱。

---

## 3. 明确不要先做的事

以下方向现在都不该优先：

- 正式长训 / overnight 批量试错
- `ProtagonistGenomeV2` 大改
- manager/worker 双脑头
- full live world 闭环上线
- GPU profiling 深挖
- 3D replay panel
- POET / EnvironmentGenome
- 把生命周期、物种角色、closed-loop 一起开工

这些都不是“不做”，而是**不能现在先做**。

---

## 4. 推荐开发顺序

## Stage A · 收口当前 cognition-social 主线

### 目标

把已经半接通的：

- `Memory V2`
- `ContributionLedger`
- `signal semantics`

真正收成一条可证明的链。

### 为什么必须先做它

因为它：

- 离当前代码最近
- 已有 trace / ablation / task / tests 基础设施
- 直接决定后面物种协作、角色分化、自动晋升是否站得住

如果这条线都没收口，就往上加生命周期、production 闭环、self-growing，只会把噪声放大。

### 具体顺序

#### A1. 先把 `Memory V2` 从“可读回”推进到“真驱动任务”

先盯 3 个固定任务：

- `water_return`
- `worksite_return`
- `signal_response`

重点不是继续加 bank，而是：

- 补强 spatial query
- 补强 episodic query
- 补强 social query
- 解释 ablation 为什么强/弱
- 去掉替代路径和假阳性

#### A2. 再把 `ContributionLedger` 统一成事件谱系

当前建造/围猎已有局部 credit。下一步应统一成：

- `Build`
- `Hunt`
- `Signal`

三条线都能落到：

- 谁触发
- 谁响应
- 谁真正贡献
- reward 怎么按 share 分

目标不是“分数更复杂”，而是让 communication / cooperation 的因果链可证。

#### A3. 最后收口 `communication semantics`

不是继续乱调 reward，而是补成真正有语义压力的 3 个任务：

- scout-worker
- danger warning
- build request

要新增：

- `communication_metrics.csv`
- signal ablation
- listener policy shift
- producer/listener contribution

### 这一阶段允许动的主要文件

- `runtime/ProtagonistEcologyRuntime.cpp`
- `runtime/MemoryAblationEval.cpp`
- `runtime/GoalManager.cpp`
- `brain/SpatialMemoryBank.*`
- `brain/EpisodicMemoryBank.*`
- `brain/SocialMemoryBank.*`
- `perception/MemoryCognitionPerception.*`
- `runtime/TraceRecorder.*`
- `configs/protagonist_ecology_train_camp_signal.toml`
- `tests/route_protagonist/*memory*`, `*goal*`, `*task*`

### 退出条件

满足以下 5 条才算 Stage A 结束：

- `water_return` 的 spatial ablation 解释清楚且分离稳定
- `worksite_return` 继续保持干净分离
- `signal_response` 不再靠 GoalManager 假指标，而是真 listener 行为变化
- `Build/Hunt/Signal` 三条 credit 都能追到事件链
- 至少一条 communication task 通过 ablation 证明“关闭信号后明显掉”

---

## Stage B · 鲜明物种生态角色（R-005）

### 目标

把背景物种从“参数不同”推进到“生态人格不同”。

### 为什么排第二

物种角色要站得住，必须建立在：

- 稳定感知
- 稳定信用分配
- 稳定通信/协作任务

之上。

否则只会变成速度、攻击、血量的换皮。

### 默认开发顺序

#### B1. 先做最容易看出来的 4 类角色

- 独居顶级掠食者
- 群猎者
- 食腐者
- 人类/主角协作围猎者

#### B2. 角色优先靠生态位规则，不靠奖励刷性格

优先加：

- territory / home range
- pack spacing / shared target
- carcass attraction / producer-scrounger
- signal-assisted hunt / group response

不要先靠大量 reward 去硬拧角色。

#### B3. 每个角色都要有专门观测指标

至少要补：

- territory overlap / conflict
- pack surround / group kill share
- carcass-first feeding
- signal-follow / assist / converge

### 主要代码落点

- `runtime/BootstrapPopulation.cpp`
- `runtime/ProtagonistEcologyRuntime.cpp`
- route-local world layers
- route-local config / eval tasks

### 退出条件

- 用户只看屏幕几分钟就能区分 4 类角色
- 角色差异不依赖单一参数解释
- 角色指标与 trace 能对上

---

## Stage C · 生命周期与繁殖闭环（R-006）

### 目标

把世界从“静态单位对打/搬运”推进到“真有代际变化的生态系统”。

### 为什么排第三

生命周期会同时改：

- agent state
- fitness
- death/corpse
- disease
- reproduction
- lineage metrics
- checkpoint compatibility

这是高耦合改动。

如果在物种角色和 cognition-social 主线没收口前先做，排查成本会爆炸。

### 默认顺序

#### C1. 先做年龄阶段，不先做复杂遗传

先补：

- juvenile
- adult
- elder
- natural death

并让年龄影响：

- movement
- combat
- reproduction window
- recovery / disease susceptibility

#### C2. 再做尸体与疾病生态后果

要有：

- corpse persistence
- scavenger feeding
- corpse-borne disease risk

#### C3. 最后做性繁殖与谱系

最小闭环必须有：

- mate conditions
- parent pairing
- crossover / mutation
- offspring insertion
- lineage tracking

### 退出条件

- 群体结构会因老病死改变
- 后代不是简单复制
- 谱系差异能在行为和群体分布上留下痕迹

---

## Stage D · train / trial / frontier / live 自动闭环（R-007）

### 目标

消灭“手动搬 checkpoint”的主流程。

### 为什么排第四

自动闭环必须依赖已经稳定的：

- challenge task
- lineage metrics
- regression suite
- species / lifecycle 基础状态

否则 production incident 只会回流成噪声。

### 默认顺序

#### D1. 先做 4 个对象

- `Incident`
- `Challenge`
- `Lineage`
- `PromotionDecision`

#### D2. 再做自动回流链

`live incident -> challenge -> challenger lineage -> frontier trial -> canary release -> rollback`

#### D3. 学习型控制器最后加

只允许它做：

- task scheduling
- challenge prioritization
- archive ranking
- promotion scoring

不能直接取代：

- baseline gate
- regression gate
- canary gate
- rollback gate

### 退出条件

- 新谱系可以小流量放归
- 失败可以自动回滚
- 不再需要手动在 train/test/live 间搬模型

---

## Stage E · 开放式增殖能力（R-008 + Phase 5/6/7）

### 目标

让系统开始具备：

- 多类策略并存
- richer environment 下新技能出现
- 更长链的目标维持

### 为什么排第五

因为它是“放大器”，不是“起步器”。

在前面几条线没收口前做它，只会把半成品机制放大成更难排查的大系统。

### 默认顺序

#### E1. 先做 online QD

把当前离线 `MapElitesArchive` 真接入 reproduction / archive 保留。

#### E2. 再做 world model

先保持离线监督路径：

- trace dataset
- predictor
- latent feature
- eval report

先别上 dream training。

#### E3. 再做 hierarchical goal

等 world model / memory / communication 都稳定后，再上 manager/worker。

#### E4. 最后才考虑 POET / environment genome / 自长机制放大器

这一步默认属于“后期增殖器”，不是当前主线。

### 退出条件

- archive 里不止一个策略簇
- latent feature 确实提升 eval，不只是参数变大
- 长链目标持续性可测

---

## Stage F · 长训与展示

最后再做：

- 多 seed 正式长训
- replay export 整理
- 3D panel
- live world 可参与干预

**没有前面 A-E 的收口，不进入 Stage F。**

---

## 5. 每个机制统一怎么开发

后面所有机制都按同一模板走，禁止再各写各的。

## 5.1 开工前

先写 6 件事：

1. 机制目标
2. 证据链
3. 兼容级别（wiring-only / compatible extension / partially incompatible / fully incompatible）
4. 最小任务
5. ablation 方案
6. 退出条件

## 5.2 开发顺序

固定顺序：

1. `test`
2. `trace / metrics`
3. `micro-task`
4. `integrated smoke`
5. `mid run`
6. 必要时才 `formal long run`

## 5.3 文档顺序

每个机制最少要同步 3 处：

- `supporting/REALTIME_USER_REQUIREMENTS.md`
- 对应专项设计文档
- `PROGRESS.md` / `STATUS.md`

---

## 6. 新旧文档关系与清理原则

## 6.1 当前应保留的主文档

必须保留：

- `supporting/WORLD_UNDERSTANDING_RESEARCH_AND_ENGINEERING_PLAN.md`
- `supporting/MEMORY_SYSTEM_MASTERPLAN.md`
- `supporting/MEMORY_PHASE_A_EXECUTION_PLAN.md`
- `supporting/CONTINUOUS_TIME_WORLD_DESIGN.md`
- `supporting/COMPUTE_SCALING_GPU_CPU_PLAN.md`
- `supporting/REALTIME_USER_REQUIREMENTS.md`
- `supporting/FINAL_STATE_VISION.md`
- `supporting/MEMORY_ABLATION_SMOKE_RESULT.md`
- `STATUS.md`
- `PROGRESS.md`
- **本文件**
- `DEVELOPMENT_TASK_PLAN.md`

## 6.2 不再作为主导航入口的文档

以下文档不再应被当成默认主入口：

- `archive/MODULE_DEVELOPMENT_PLAN.md`
- `archive/HANDOFF_NEXT_AI_2026-05-21.md`

原因不是它们毫无价值，而是：

- 一个属于较早阶段的模块化推进口径，已不再匹配当前真实优先级
- 一个属于单次对话交接，不应长期压在主导航里

后续默认导航应转到：

- `DEVELOPMENT_EXECUTION_GUIDE.md`
- `DEVELOPMENT_TASK_PLAN.md`
- `STATUS.md`

---

## 7. 当前默认下一步

如果没有新的用户方向变更，当前默认第一步就是：

1. 继续收口 `signal_response`
2. 同步把 `Memory V2 -> credit -> communication semantics` 写成一条完整证据链
3. 只有这条链收口后，才进入物种角色线

这就是当前**最少绕路**的路线。
