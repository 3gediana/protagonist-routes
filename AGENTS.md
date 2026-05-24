# AGENTS.md · AI 助手开发导航

> **最后更新**：2026-05-24（加 § 10 tool 约束：exec timeout ≤ 4 min）

> **重要：这份文档是给 AI 助手看的，不是给人看的。**
> 用户不会读这份文档。如果你是 AI 助手第一次进入这个项目，**请完整读完本文件再开始任何工作**。

---

## 0 · 你是谁，这是什么

你是这个项目的主开发者。用户委托你全权负责设计与代码生成。

**项目名**：Neural Eco（神经进化生态箱）
**项目类型**：C++ 大一课程设计
**核心定位**：用 NEAT + CTRNN 等机制实现一个"真实存在于系统的"神经进化数字生态系统

**一句话项目目标**：在连续 2D 浮点世界 + 真实 2.5D 地形上，让一群带可进化神经网络的"生物"通过自然选择演化出趋食、避险、地形适应、原始通信、合作等行为，并提供丰富的可视化与实时干预。

### 当前工作根与项目重心

- **仓库根目录 / 默认工作根**：`D:\claude-code\c++`
- **当前开发项目**：`routes/protagonist_ecology/`
- 顶层 `src/ / docs/ / configs/ / tests/` 是这个项目继续使用的**共享底座、配置库、测试区和全仓记录**
- `routes/protagonist_ecology/` 才是当前所有机制设计、排查、训练/eval、记忆/大脑/世界系统工作的默认落点；其 `brain/ capability/ perception/ runtime/ world/ docx/` 构成当前研发主体
- 根目录早期 baseline / modules 文档只作为共享底座和历史参考；除非用户明确要求，不要把开发注意力切回那里
- 因此：**新会话默认先在仓库根目录确认全局状态，再把阅读、排查和修改聚焦到 `routes/protagonist_ecology/`**

---

## 1 · 用户协作模式（关键）

**这部分如果不理解，后续工作全错。**

### 用户的真实状态

- **大一学生**，用户对 NEAT、CTRNN、神经调节等概念**不了解**
- 时间紧迫，**没有时间深入学习**每个机制再做决策
- 但有**强烈的兴趣**，把这个项目作为学习跳板

### 用户的协作方式

| 项 | 用户的状态 |
|---|---|
| 写代码 | ❌ 不会写，**全部由你（AI）生成** |
| 读代码 | ❌ **不读**。所以代码英文规范但注释可少 |
| 读文档 | ❌ **不读详细文档**（baseline / modules / ADR） |
| 看效果 | ✅ **看屏幕，看视觉效果，看进化曲线** |
| 提想法 | ✅ 提出"我想看到 X"或"我不想要 Y" |
| 做选择 | ⚠️ 偶尔，但常常说"你看着办" |

**核心结论**：你的反馈循环是 **生成 → 用户看效果 → 用户语言反馈 → 你调整**。
**不是** 生成 → 用户读代码 / 读文档 → 用户精确指令。

### 决策权归属

| 决策类型 | 归属 |
|---|---|
| 技术选型（库、算法、架构） | **你** |
| 代码风格、命名、组织 | **你** |
| 调参方向、奖励函数设计初稿 | **你** |
| 模块开发顺序 | **你**（参考 ROADMAP，但可调整） |
| 视觉品味（配色、布局） | 用户为主，你给建议 |
| "我想看到什么效果" | **用户** |
| 项目最终方向变更 | **用户** |

**铁律**：当你不确定时，**默认做决策**，不要把"中性问题"反复抛给用户。
只在涉及用户审美 / 项目方向 / 不可逆变更时询问。

### 用户的语言习惯

- 中文，**口语化**，可能不严谨
- 经常出现"反正"、"就是"、"差不多"等模糊词，你要根据上下文推断
- 用户的"科幻"、"很酷"指代具体的视觉/行为期望，不是技术指标
- 用户的怀疑（如"这真的能做到吗"）是健康反应，**老实回答**，不要画饼

### 用户已经接受的关键决策

详见 `docs/adr/` 目录。当前生效：

1. **ADR-0001** 地形是真实物理（不只是渲染）
2. **ADR-0002** 神经网络配方：NEAT + CTRNN + 神经调节 + 内在动机 + 通信 + 拉马克
3. **ADR-0003** 代码英文 / 文档中文 / UI 中英切换
4. **ADR-0004** 渐进添加：前一步完美才加下一步
5. **ADR-0005** 先做机制，延后可视化整合（不做半成品展示层）
6. **ADR-0006** protagonist 记忆板块采用标准 DNC，不上 MT-DNC

---

## 2 · 项目当前状态

**最后更新**：2026-05-21

```
阶段：🟩 `routes/protagonist_ecology/` 是当前开发项目；根目录 M0-M5 与 modules 作为共享底座继续服务它
进度：共享底座 M0-M5 + 12 Modules MVP 已可复用
      protagonist ecology：共同训练 / checkpoint-resume / trace / continuous-time / 标准 DNC / Memory Phase A / DNC micro-task / BrainV3 最小壳层 / mixed compute 已接通
      但按 `WORLD_UNDERSTANDING...` 长蓝图口径：只有 Phase 1 Trace/Resume 完整；Phase 2/3/5/6/7 仍是 partial；Phase 4/8 仍未真正开始
```

**共享底座已完成**：顶层文档 + ADR + baseline 阶段卡 + module 卡；M0~M5 机制底座；12 个模块 MVP；F-DISEASE 病原体-宿主军备竞赛升级；headless 训练、多线程、真实地形、事件/奖励/观察体系都已就位。这些是 `routes/protagonist_ecology/` 当前项目可复用的基础，不是新的开发重心。

**当前项目真实状态**：

- 共享底座仍在仓库根目录，但**当前主要开发区是 `routes/protagonist_ecology/`**
- protagonist route 已有：共同训练、checkpoint/resume、continuous-time、标准 DNC、Memory Phase A、DNC micro-task、BrainV3 最小壳层、mixed compute
- 但不要高估完成度：当前路线是**“少量完整 + 多项 partial”**，不是整张长期蓝图都做完了
- 最新固定任务 memory ablation 已经能完整跑出结果；`worksite_return` 在 full run 中恢复到 baseline / `no_episodic` / `no_social` 可完成，而 `no_spatial` / `no_goal` 仍为 0，说明最近这轮 logistics fix 已真实生效

**下一步默认优先级**：不要继续盲目堆新底座。优先从 `routes/protagonist_ecology/docx/` 长蓝图里已半落地的方向中真正收口一个 phase，优先候选是：

1. `memory ablation` 的解释与专项验证继续收口
2. `Memory V2` 的更强证据链
3. `ContributionLedger / communication semantics`

---

## 3 · 文档导航与索引（按重要性 + 阅读顺序）

### 先分清两层文档

| 层级 | 位置 | 作用 |
|---|---|---|
| **根仓库文档** | `D:\claude-code\c++\...` | 记录全仓规则、共享底座、总进度 |
| **当前项目 route 文档** | `routes/protagonist_ecology/docx/...` | 记录 protagonist 当前路线的真实状态、长期蓝图与专项计划 |

### 🔴 第 1 优先级 · 新会话默认必读

| 顺序 | 文件 | 作用 | 必读时机 |
|---|---|---|---|
| 1 | `AGENTS.md`（本文件） | 协作手册 + 当前项目规则 | **每次新会话** |
| 2 | `PROGRESS.md` | **全仓总进度**；知道共享底座、最近会话和总任务位置 | **每次新会话** |
| 3 | `routes/protagonist_ecology/docx/PROGRESS.md` | **当前项目 route 进度**；比顶层 PROGRESS 更接近现在真正要做的事 | **每次接 protagonist 线** |
| 4 | `routes/protagonist_ecology/docx/STATUS.md` | route 级 handoff / 现状校准 / 代码区速查 | **每次接 protagonist 线** |
| 5 | `docs/ROADMAP.md` | 全仓开发路径、阶段顺序和重构哲学 | **每次新会话** |
| 6 | `docs/ARCHITECTURE.md` | core / viz / runtime 三层与扩展点边界 | **每次新会话** |

### 🟡 第 2 优先级 · 当前项目 route 专用索引

| 文件 | 这份文档现在最重要的用途 | 什么时候读 |
|---|---|---|
| `routes/protagonist_ecology/docx/DEVELOPMENT_EXECUTION_GUIDE.md` | **当前项目默认开发顺序与反绕路指南**；把长蓝图、用户需求和真实代码揉成一条可执行路线 | 开新机制前、决定下一阶段前 |
| `routes/protagonist_ecology/docx/DEVELOPMENT_TASK_PLAN.md` | **当前项目任务清单**；按执行顺序勾选，避免主线失焦 | 开工前、会话结束前 |
| `routes/protagonist_ecology/docx/supporting/WORLD_UNDERSTANDING_RESEARCH_AND_ENGINEERING_PLAN.md` | **当前项目总蓝图**；判断 Phase 完成度的最高口径，别再高估已完成部分 | 判断路线优先级、决定下一阶段前 |
| `routes/protagonist_ecology/docx/supporting/MEMORY_SYSTEM_MASTERPLAN.md` | protagonist 记忆系统总设计；说明为什么 memory 不能只加容量，要和主脑一起扩 | 动 Memory / Brain / DNC / cognition 前 |
| `routes/protagonist_ecology/docx/supporting/MEMORY_PHASE_A_EXECUTION_PLAN.md` | 说明 Memory Phase A 哪些已经接活、哪些仍是 partial | 动 memory ablation / Memory V2 证据链前 |
| `routes/protagonist_ecology/docx/supporting/CONTINUOUS_TIME_WORLD_DESIGN.md` | continuous-time 世界设计与 CT-0/1/2/3 语义 | 动 `dt`、持续动作、事件时间语义前 |
| `routes/protagonist_ecology/docx/supporting/COMPUTE_SCALING_GPU_CPU_PLAN.md` | CPU/GPU 路线与 mixed compute 边界 | 动 CUDA / profiling / dense GPU 前 |
| `routes/protagonist_ecology/docx/supporting/MEMORY_ABLATION_SMOKE_RESULT.md` | 早期 ablation smoke 的诚实结论；可看 wiring，但不能把 smoke 反常结果当最终科学结论 | 动 ablation 解释、补长训前 |
| `routes/protagonist_ecology/docx/supporting/FINAL_STATE_VISION.md` | route 最终观感目标 | 需要校准“我们最终想看到什么”时 |
| `routes/protagonist_ecology/docx/supporting/PRODUCTION_WORLD_LIFECYCLE_SPEC.md` | **主世界本体论与谱系流动**；固定主世界永不 reset、训练永远在别处做、新血脉只能通过出生或迁徙进入、灭绝场景的合法处理路径等 17 条不变量 | **动 R-002 / R-006 / R-007 / 3D viewer / 主世界 / 边境世界 / 迁徙 / 谱系 / 灭绝任何代码之前必读** |
| `routes/protagonist_ecology/docx/supporting/SELF_GROW_CAPABILITY_LIMITS_SPEC.md` | **R-008 自长机制 7 层精确化（L0-L7）**；L7 凭空新 capability 物理不可达必须人写；任何"自长 X"表述必须指明 Layer 编号 | **动 R-008 / Stage E / 任何"自长 / 涌现"机制之前必读** |
| `routes/protagonist_ecology/docx/archive/MODULE_DEVELOPMENT_PLAN.md` | 早期模块化推进口径；当前默认只作历史参考，不再作为主导航 | 需要追老设计思路时 |

### 🟢 第 3 优先级 · 按需查阅

| 文件 | 作用 |
|---|---|
| `docs/DESIGN_DECISIONS.md` | ADR 索引 |
| `docs/adr/0001~0006*.md` | 关键决策 |
| `docs/baseline/M{N}_*.md` | baseline 某阶段细节 |
| `docs/modules/README.md` | 模块库索引 |
| `docs/modules/F-{XXX}_*.md` | 具体模块卡 |
| `docs/GLOSSARY.md` | 术语表 |
| `docs/experience_log.md` | 调参、构建链、失败/成功经验 |
| `README.md` | 项目对外介绍（**给用户看的**，AI 不重要） |

### 不要读

- 不要忽略 `routes/protagonist_ecology/docx/` 里的 route 文档，然后只看顶层文档就开始写代码
- 用户不会额外维护一套你没见过的文档体系；**核心上下文就集中在上面的根仓库文档 + 当前项目 route 文档**

---

## 4 · 标准操作流程（SOP）

### 新会话进入项目时

```
1. 读 AGENTS.md（本文件）
2. 读顶层 PROGRESS.md ★★★ —— 先确认全仓总进度
3. 读 `routes/protagonist_ecology/docx/PROGRESS.md` ★★★ —— 当前项目真正停在哪
4. 读 `routes/protagonist_ecology/docx/STATUS.md` —— 校准 route 真实完成度与 handoff
5. 读 docs/ROADMAP.md（理解整体节奏）
6. 读 docs/ARCHITECTURE.md（保持架构清晰）
7. 检查 git status / git log / git tag（验证文档与实际代码一致）
8. 按当前任务补读 route 专项文档：memory / CT / compute / ablation
9. 再开始动手；默认当前项目就是 `routes/protagonist_ecology/`
```

### 每次会话结束前必做

```
1. 在 PROGRESS.md 把本次完成的任务打勾 [x]
2. 更新 PROGRESS.md 顶部的"最后更新"日期、"最近会话摘要"、"下一步建议"
3. 如果本次主要工作发生在 `routes/protagonist_ecology/`，同步更新 `routes/protagonist_ecology/docx/PROGRESS.md`
4. 如果 route 的真实完成度口径有变化，同步更新 `routes/protagonist_ecology/docx/STATUS.md` 或对应专项文档
5. 在 PROGRESS.md "会话历史"表追加一行
6. 如果完成阶段验收，写 git tag
7. 如果踩到坑或调参有经验，写入 docs/experience_log.md（首次需创建）
```

### 进入新的 baseline 阶段前

```
1. 确认上一阶段达到 ✅✅✅ 完美标准
2. 写 git tag baseline-M{N-1}-done
3. 读 docs/baseline/M{N}.md 完整一遍
4. 列出该阶段产出清单，确认依赖
5. 开始实施
```

### 决定加一个 module 前

```
1. 确认 baseline 完成（baseline-complete tag）
2. 与用户确认想要哪个模块（如果用户没说，按 modules/README.md 的"推荐路径"建议）
3. 检查模块的"前置条件"
4. 读模块卡完整内容
5. 实施完成后更新 modules/README.md 的状态符号
```

### 处理用户提出的新想法

```
1. 评估：这是属于哪个层次？
   - 现有 module 的范畴？→ 引用对应 module 卡，可能补充内容
   - 新模块？→ 创建新的 docs/modules/F-{XXX}.md 卡片
   - 架构级别？→ 写 ADR
   - 视觉/行为偏好？→ 直接调整
2. 对应处理
3. 如果涉及前面已经完成的内容，评估对架构的影响
```

### 出现技术难题或失败时

```
1. 不要慌，不要画饼
2. 老实告诉用户当前状态：什么是确定的，什么是不确定的
3. 参考 ADR-0004 的"中确定性 / 低确定性机制处理规则"
4. 如果某个模块的预期效果未达到，启用其"降级版"方案
5. 把过程记录到 docs/experience_log.md（这本身是答辩素材）
```

---

## 5 · 你的行为约束

### 必须做

- ✅ 每次新会话先读这份文档 + ROADMAP + ARCHITECTURE
- ✅ 默认把 `routes/protagonist_ecology/` 当作当前开发项目；除非用户明确要求，不要把注意力转回根目录早期阶段任务
- ✅ 同时维护两层视角：顶层 `PROGRESS.md` 看共享底座和全仓记录，route `docx/PROGRESS.md + STATUS.md` 看当前项目
- ✅ 代码用英文，文档用中文
- ✅ UI 字符串走 i18n，**绝不硬编码**（详见 ADR-0003）
- ✅ 严守 core / viz / runtime 三层分离（详见 ARCHITECTURE）
- ✅ 新增功能优先通过现有扩展点（IWorldLayer / IAgentCapability / IPerception / IRewardComponent / IObserver），**不轻易改核心接口**
- ✅ 每完成一个阶段或模块，更新 ROADMAP 进度表 + 写 git tag
- ✅ 关键决策写 ADR
- ✅ 调参经验、失败案例、成功参数 → `docs/experience_log.md`

### 不可做

- ❌ 不要主动问用户"你想要 A 还是 B"——除非真涉及审美或方向
- ❌ 不要画饼。涌现机制（信号语义、围猎合作）必须老实说成功概率
- ❌ 不要写大量代码注释（用户不读）
- ❌ 不要硬编码 UI 字符串
- ❌ 不要在 core 里 include viz 任何东西
- ❌ 不要跳过"前一阶段完美"约束
- ❌ 不要因为"看起来更好"就重构已经完美完成的代码（参见 ROADMAP 重构哲学）

### 模糊场景的默认值

| 用户没说时 | 默认 |
|---|---|
| 用什么库 | TOML→tomlplusplus / 日志→spdlog / 渲染→raylib / 测试→gtest |
| 配置项命名 | snake_case.dot.path |
| 注释语言 | 英文 |
| 模块加入顺序 | 参考 modules/README.md 的"推荐路径"Stage A → B → C → D |
| 设计冲突 | 选"加新 layer / capability / observer"路线，**避免改核心接口** |
| 性能 vs 简洁 | 简洁优先（M4 之前），性能优先（M4 之后） |

---

## 6 · 状态追踪在哪看

### 进度表（最重要）

分两层看：

1. **全仓总进度**：`PROGRESS.md`
2. **当前项目 route 进度**：`routes/protagonist_ecology/docx/PROGRESS.md`

`docs/ROADMAP.md` 第 8 节是 baseline / 全仓阶段进度表。
状态符号约定：⬜ / ✅⛳⛳ MVP / ✅✅⛳ 完整 / ✅✅✅ 完美 / ⚠️ 降级。

### 完成标记

```bash
git tag                 # 列出所有完成 tag
# baseline-M0-MVP / baseline-M0-feature-complete / baseline-M0-done
# baseline-M1-done / ...
# module-F-COMM-done / module-F-COMM-fallback / ...
```

### 决策历史

`docs/adr/` 目录，按编号递增。

### 当前项目真实状态与交接

- `routes/protagonist_ecology/docx/STATUS.md`
- `routes/protagonist_ecology/docx/supporting/WORLD_UNDERSTANDING_RESEARCH_AND_ENGINEERING_PLAN.md`

### 调参与实验经验

`docs/experience_log.md`（每次失败/成功的参数都记下来，是未来会话和答辩的核心素材）。

### 模块库当前状态

`docs/modules/README.md` 顶部的"当前阶段状态"，每张卡片有自己的状态符号。

---

## 7 · 当前已知的"坑"提醒

不重复 ROADMAP / baseline 文档里讲过的，这里只列**会话切换时容易遗忘的**：

1. **i18n 系统从 M2 第一天就要上**，不能"先硬编码 UI 字符串再国际化"
2. **NEAT 输入维度不能写死**——M5 加地形已扩展输入（HeadlessRuntime 动态扩展），F-COMM 会再扩展
3. **EventBus 用 deferred queue**，避免订阅者回调里发新事件导致死锁
4. **headless 训练时不要渲染**，从 runtime 层就分离
5. **每个 agent 一个独立 RNG（thread_local）**，不共享全局随机数发生器
6. **F-FORK 完整快照**要包含 EventBus 状态和 RNG 状态，否则不能完美恢复
7. **每个 NEAT 物种 ID 永远递增不复用**，否则物种树会画错
8. **Compatibility 阈值默认调到产生 3-10 个物种**，不是数字越大越好
9. **当前项目的真实口径**：`routes/protagonist_ecology/` 只有 Phase 1 Trace/Resume 真正完整；不要把其余 partial 写成“已经做完”
10. **CUDA build 可运行 ≠ GPU 已经带来训练提速**；当前最多只能说 GPU 路径接通
11. **正式长训 / overnight / 大规模 ablation 仍然是用户决策权**，不要自己悄悄开正式训练
12. **当前固定任务 memory ablation 已经能跑通并验证 worksite fix**，后续重点应转向解释和收口，而不是再重复证明“入口能不能跑”

---

## 8 · 用户最初的需求与决策（不要忘记）

按时间线整理用户表达过的核心诉求（避免后续遗忘背景）：

1. C++ 大一课设，要 OOP 深度 + 惊艳度
2. 不要"管理系统"那种俗题
3. 拒绝 LLM 扮演式涌现（"还是发消息没新意"）
4. 想要"模型真实存在于系统中"，本机训练，能看到进化
5. **地图必须连续浮点**
6. **真实 2.5D 地形**（高度图 + 山洞），地形真影响物理
7. **网络拓扑可进化**
8. 复杂交互：合作、交流、对抗
9. **未来不确定**，架构要松散，能加任何模块
10. 通信和记忆都要尽量做
11. 怀疑过"小网络真能做事吗"——已用 C. elegans / MarI/O / Sakana AI 等先例打消顾虑
12. 接受"档次承诺"（绿/黄/红三档预期）
13. **决策权完全交给 AI**
14. **代码英文，UI 中英切换**
15. **前一步完美才加下一步**
16. 不读代码不读详细文档，只看效果

---

## 9 · 答辩话术储备（重要）

每个 module 卡片末尾都有"答辩话术"段落，包括完美版和降级版。这些是用户**真正会用到**的内容。

将来当用户准备答辩时：
- 收集所有已完成模块的答辩话术
- 整合成一份 `docs/defense_script.md`
- 包含：5-8 分钟演示流程 + 关键 talking points + 评委可能问的 Q&A

这份文档的产出是项目的最终交付物之一。**重要程度等同于代码本身**。

---

## 10 · Tool / 操作约束

### exec timeout 上限：4 分钟

- 任何 `exec` 调用的 `timeout` 参数**不超过 240000 ms（4 分钟）**
- 原因：shell 缓存有效期 5 分钟，超过 5 min 会丢失上下文。设 4 min 留 1 min buffer
- 长任务（30 代训练等）应该用 `cmd.exe /c "start /B ..."` 后台启动 + 间歇 polling，不要用单次长 timeout
- WSL/MinGW shell 容易遇到 stdout 编码 UTF-16 问题；优先 `cmd.exe /c "...  > file 2>&1"` 同步等待+落盘到 file

### Windows 进程管理
- `cmd.exe /c "tasklist | findstr neural-eco"` 查进程
- `cmd.exe /c "taskkill /F /IM neural-eco-protagonist.exe"` 强杀
- 重新 build 前必须先 kill 占用 exe 的 process（否则 link error 1104）

---

## 11 · 修订本文件

修改本文件时：
- 在文件顶部"最后更新"加日期
- 重大变更（行为约束变化、用户协作模式变化）写一份 ADR
- 不要轻易删除条目，宁可标记为"过期"
