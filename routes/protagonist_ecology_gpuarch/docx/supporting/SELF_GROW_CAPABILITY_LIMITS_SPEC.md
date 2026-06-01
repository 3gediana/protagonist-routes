# Self-Grow Capability Limits Spec · 模型扩展自身能力的 7 层精确定义

> 位置：`routes/protagonist_ecology/docx/supporting/SELF_GROW_CAPABILITY_LIMITS_SPEC.md`
>
> 创建：2026-05-22
>
> 作用：R-008 期待"模型自己长出机制"的精确化定义。任何"自长 X"承诺必须先指明指向哪一层。

---

## 0. 立场

- "能力扩展" ≠ 单一概念，分 7 层
- 每层可达性、工程前提、文献支持、训练成本不同
- 不允许用模糊词"自长机制 / 涌现能力"承诺工作
- 任何机制实验必须先声明目标 Layer 编号

---

## 1. 7 层能力扩展总览

| Layer | 名称 | 一句话定义 | 项目可达 |
|---|---|---|---|
| L0 | 参数微调 | 同结构权重调整 | ✅ 已默认 |
| L1 | 结构生长 | NEAT 加 node / 加 link | ✅ 已默认 |
| L2 | 策略组合 | 已有 actions 的新序列 | ✅ 已默认 |
| L3 | 策略涌现 | 整体行为新颖、非显式编程 | ✅ 可期待 |
| L4 | 技能分化 | 多技能簇 / 模块化 | ⚠️ 需架构留空间 |
| L5 | 通信协议涌现 | 信号语义自发形成 | ⚠️ 需通信任务立起来 |
| L6 | 组合性能力 | 多 primitive 复合成"看起来像新 capability" | ⚠️ 需 capability 通用化 |
| L7 | 真正新 capability | 凭空创造新 action / 新物理 | ❌ 必须人写 |

---

## 2. 每层精确定义

### L0 · 参数微调

| 项 | 值 |
|---|---|
| 定义 | brain 结构不变，权重 / bias / DNC 槽位值变化 |
| 触发 | 任何 weight mutation / gradient step |
| 工程前提 | 已有 brain 结构 + fitness 或 gradient 信号 |
| 文献最强证据 | 所有 NEAT / RL 文献 |
| 训练成本 | 低 |
| 是否算"自长" | 否，属于 fine-tune |

### L1 · 结构生长

| 项 | 值 |
|---|---|
| 定义 | brain 拓扑增加 node 或 connection |
| 触发 | NEAT add-node / add-link mutation |
| 工程前提 | NEAT 或 HyperNEAT 类拓扑可变骨架 |
| 文献最强证据 | Stanley & Miikkulainen 2002 NEAT |
| 训练成本 | 中 |
| 是否算"自长" | 是结构层面，不是 capability 层面 |

### L2 · 策略组合

| 项 | 值 |
|---|---|
| 定义 | 同 action space 下新的 action 序列 |
| 触发 | 任何成功 RL / 进化训练 |
| 工程前提 | action space + reward 信号 |
| 文献最强证据 | 所有 RL benchmark |
| 训练成本 | 低-中 |
| 是否算"自长" | 是行为层面，不是 capability 层面 |

### L3 · 策略涌现

| 项 | 值 |
|---|---|
| 定义 | 整体行为新颖、非显式编程，需要多 agent 或长链交互才能出现 |
| 例子 | 围猎、堵门、ramp use、box surfing |
| 触发 | 多 agent autocurricula 或 open-ended pressure |
| 工程前提 | 多 agent + 物理交互 + 长训 |
| 文献最强证据 | Baker et al. 2019 OpenAI Hide-and-Seek（6 阶段策略涌现） |
| 训练成本 | 高 |
| 是否算"自长" | **是，最低门槛的"真自长"** |

### L4 · 技能分化

| 项 | 值 |
|---|---|
| 定义 | 同 agent 在不同 context 激活不同技能 cluster |
| 例子 | 巡逻 / 觅食 / 战斗 / 回巢 各自一组独立的 policy 行为 |
| 触发 | DIAYN-like skill discovery 或 Option-Critic |
| 工程前提 | brain 架构留出 modular slots / option 头 |
| 文献最强证据 | Bacon et al. 2017 Option-Critic; Eysenbach et al. 2018 DIAYN |
| 训练成本 | 高 |
| 是否算"自长" | 是模块层面 |

### L5 · 通信协议涌现

| 项 | 值 |
|---|---|
| 定义 | 多 agent 之间发展出有信息内容的信号约定 |
| 例子 | 一个信号代表"危险"、另一个代表"集合" |
| 触发 | 任务必须依赖通信才能高 reward |
| 工程前提 | 通信 channel 已存在 + 通信依赖任务 |
| 文献最强证据 | Lazaridou & Baroni 2020 emergent communication; Foerster et al. 2016 |
| 训练成本 | 高 |
| 是否算"自长" | 是协议层面 |

### L6 · 组合性能力

| 项 | 值 |
|---|---|
| 定义 | 用多个底层原子 capability 复合出"看起来像新 capability"的行为 |
| 例子 | `TouchResource` × `water` 复合出"喝水"；`StrikeNearby` × `tree` 复合出"砍树" |
| 触发 | 把 capability 改写成更原子 / 更通用的接口 |
| 工程前提 | capability 必须原子化设计 + 物理后果定义在物体而非 capability |
| 文献最强证据 | Wang et al. 2023 Voyager（LLM 在 API primitives 上组合 skill）；Karl Sims 1994（在刚体 + 关节 primitives 上组合形态） |
| 训练成本 | 极高（神经路线）/ 中（LLM-as-author 路线） |
| 是否算"自长" | 是 capability 表面层面，不是 primitive 层面 |

### L7 · 真正新 capability

| 项 | 值 |
|---|---|
| 定义 | 在没有底层 primitive 的情况下凭空创造新 action / 新感知 / 新物理 |
| 例子 | 没给 DrinkCapability 也没给 TouchResource，agent 学会喝水 |
| 触发 | 不存在 |
| 工程前提 | 不存在 |
| 文献最强证据 | **零** |
| 训练成本 | 不可达 |
| 是否算"自长" | 物理上不可能 |

---

## 3. 文献最强证据汇总

| Layer | 论文 | 关键 finding |
|---|---|---|
| L1 | Stanley & Miikkulainen 2002 | 拓扑可生长，但 capability 来自 action space |
| L3 | Baker et al. 2019 OpenAI Hide-and-Seek | 6 阶段策略 emerge 全部基于已有 grab/lock/move |
| L3 | Sims 1994 Evolving Virtual Creatures | 形态 + 行为 emerge 全部基于刚体 + 关节 primitives |
| L4 | Bacon et al. 2017 Option-Critic | 多 option 分化基于已有 action space |
| L4 | Eysenbach et al. 2018 DIAYN | 多 skill 分化基于已有 action space |
| L5 | Lazaridou & Baroni 2020 | 信号语义 emerge 基于已有 communication channel |
| L5 | Foerster et al. 2016 | 多 agent 信号协议 emerge 基于已有 message slot |
| L6 | Wang et al. 2023 Voyager | LLM 写新 skill 代码基于已有 Mineflayer API |
| L6 | Goyal et al. 2019 RIMs | 模块分工基于已有 recurrent unit slots |
| L7 | — | **任何论文都没有突破这一层** |

---

## 4. R-008 原文期待逐项落层

| R-008 原文期待 | 落到哪层 | 可达 |
|---|---|---|
| "自己长出新的技能" | L3 + L4 | ✅ |
| "行为分化" | L4 | ✅ 需架构留空间 |
| "局部模块化" | L4 | ✅ 同上 |
| "通信 / 协作模式" | L5 | ✅ 需通信任务立起来 |
| "richer data + 选择压力" | L3 / L4 / L5 触发条件 | ✅ |
| "多样性保留" | L4 必要条件 | ✅ NSGA / Novelty / MAP-Elites |
| 隐含期待"自己长出新 capability" | L7 | ❌ 物理不可达 |
| 隐含期待"自己长出新物理规则" | 超 L7 | ❌ 物理不可达 |

---

## 5. 项目可达上限（NEAT + DNC + 13 action / 30+ perception）

| Layer | 当前状态 | 上限路径 |
|---|---|---|
| L0 | 已默认 | — |
| L1 | 已默认 | — |
| L2 | 已默认 | — |
| L3 | 已部分（围猎 / 信号共注意已现） | 需多 agent + 长训 + 物理交互 |
| L4 | 未做 | 需 brain 加 option head 或 modular slots |
| L5 | 未做 | 需 scout-worker / danger-warning / build-request 任意一条通信任务立起来 |
| L6 | 未做 | 需把 1-2 个 capability 改原子化（如 `Touch / Strike / Launch`） |
| L7 | 不可达 | 无路径 |

---

## 6. 不变量

1. L7 必须人写，不在任何文档承诺
2. 任何"自长 X"表述必须指明 Layer 编号（如"L4 自长"、"L5 自长"）
3. 不允许在 R-008 期待项里隐含 L7 目标
4. 任何 ablation 必须指明针对哪一层
5. capability 写成原子化算 L6 设计，不是"agent 长出新 capability"
6. LLM-as-author 路线（Voyager）属于 L6 上界，与"神经进化"主线冲突，默认不采用
7. L3 / L4 / L5 训练成本远高于 L0-L2，不在 cognition-social 主线收口前不投预算
8. 文献证据栏不可写"涌现 / 自长"等模糊词，必须引用具体论文 + 具体 finding

---

## 7. 命名与口径硬规则

- 禁止用："自长机制"、"涌现新能力"、"模型自己长出 X"（无 Layer 编号）
- 允许用："L3 strategy emergence"、"L4 skill differentiation"、"L5 communication protocol emergence"、"L6 compositional capability"
- 任何机制证据链必须包含字段 `target_layer: L0~L7`
- 任何机制完成度声明必须包含字段 `evidence_layer: L0~L7`

---

## 8. 与 R-008 的接口

- R-008 是用户硬需求文档，保持原文不动
- 本 spec 是 R-008 的**精确化执行边界**
- 后续机制 spec 引用 R-008 时必须同时引用本 spec 的目标 Layer
- 本 spec 与 R-008 冲突时以本 spec 为准（用户已确认 L7 不可达的事实）

---

## 9. 三个可选路线

| 路线 | 内容 | 推荐 |
|---|---|---|
| A · capability 通用化 | 选 1-2 个 capability 改原子（`Touch / Strike / Launch`），让 agent 在 L6 自长 | ✅ 推荐，与现有栈兼容 |
| B · LLM-as-author | 加 LLM-in-the-loop 让 agent 写新 skill 代码 | ❌ 违背神经进化主线，不采用 |
| C · 修正 R-008 期待 | 把"自长机制"明确锚定在 L3-L5，capability 必须人写写进 spec | ✅ 必须做，与 A 并行 |

---

## 10. 后续工作

| 阶段 | 内容 |
|---|---|
| S1 | 在 `REALTIME_USER_REQUIREMENTS.md` R-008 末尾加 cross-reference 指向本 spec |
| S2 | 在 `DEVELOPMENT_EXECUTION_GUIDE.md` Stage E 章节加 Layer 标注 |
| S3 | 在 `DEVELOPMENT_TASK_PLAN.md` E 主线每个任务标注 target_layer |
| S4 | 选 1-2 个 capability 做原子化设计（L6 实验候选） |
| S5 | L3 / L4 / L5 各设计一个最小验证任务，确认 evidence_layer 可观测 |
