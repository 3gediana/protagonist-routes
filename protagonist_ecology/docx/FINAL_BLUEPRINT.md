# Protagonist Ecology · FINAL BLUEPRINT

> **位置**：`routes/protagonist_ecology/docx/FINAL_BLUEPRINT.md`
> **状态**：v1 草稿 · 2026-05-23 创建 · 等用户审阅
> **作用**：本路线最终蓝图。所有 staged plan（PROGRESS / DEVELOPMENT_TASK_PLAN / 任何 Stage X）从这份蓝图导出。任何与蓝图冲突的工程决策不得进入实现。
>
> **读者**：AI 助手 + 用户审阅。
>
> **优先级**：本文件之上**只**有 `AGENTS.md`（协作规则）。本文件之下：所有 supporting/*.md spec、ADR、PROGRESS、STATUS。

---

## §0. 文档定位与组织

本蓝图是一份**汇总性文档**，不重新设计已经有 spec 的部分。

**已有的硬规格**（蓝图直接引用，不重写）：

| Spec | 蓝图采纳为 | 备注 |
|---|---|---|
| `REALTIME_USER_REQUIREMENTS.md` R-001 ~ R-008 | 全部采纳 | 用户硬需求 |
| `PRODUCTION_WORLD_LIFECYCLE_SPEC.md` 三层世界 + 17 不变量 | 全部采纳 | 用户授权 |
| `SPECIES_DIFFERENTIATION_SPEC.md` v2 8 物种 + 4 层差异化 | 全部采纳 | 用户授权 |
| `SELF_GROW_CAPABILITY_LIMITS_SPEC.md` L0-L7 | 全部采纳 | L7 物理不可达 |
| `CONTINUOUS_TIME_WORLD_DESIGN.md` hybrid continuous-time | 已部分实施，继续 | |
| `MEMORY_SYSTEM_MASTERPLAN.md` Memory V2 路线 | 全部采纳 | Phase A 已实施 |
| `COMPUTE_SCALING_GPU_CPU_PLAN.md` mixed GPU + 深度 CPU 并行 | 全部采纳 | |
| `FINAL_STATE_VISION.md` 高光画面 | 全部采纳，本蓝图把它"机制化" | |
| `WORLD_UNDERSTANDING_RESEARCH_AND_ENGINEERING_PLAN.md` | 作为路径参考 | |

**本蓝图新增**（用户 2026-05-23 给的新硬约束 + 我整合后的决策）：

- §1 系统终态定义
- §2 10 维度总规格表（含新硬约束 600×600 / 天气 / 树木细分 / 蒙古包级建筑）
- §3 Gap matrix（spec 已有 vs 代码已实施）
- §4 实施依赖层（8 层 + 依赖图，不分阶段）
- §5 关键决策点（按"效果最好"路线全采纳）

---

## §1. 系统终态定义

### §1.1 一句话定义

> **一个由神经进化驱动的、用户可参与的、持续运行的原始聚落沙盒生态系统**。在一个 600×600 拟真连续世界里，一个主角群体和 8+ 物种背景生态共存：主角搭建可见的蒙古包小聚落、配合采集围猎、应对天气季节灾害；背景物种独立繁衍、相互捕食、自然演化；用户可作为玩家在世界里近距离观察，可投放事件干预；整个系统不 reset，从启动到关停连续运行。

### §1.2 用户最终看到的画面（验收级）

打开系统后，用户应当能看到：

**视觉层**：
- 600×600 连续地图（perlin 地形 + 多 biome：草原/森林/河谷/山地/洞穴）
- 多种植被：草丛（连续分布而非离散）、3-4 种树（常青/落叶/果树/枯木）、石头堆、木棍散布
- 季节切换：地表色调、积雪、植被状态、水域结冰
- 天气切换：晴/雨/雷暴/大风（粒子 + 光照变化）
- 昼夜：日出日落 + 月光 + 火堆夜间发光
- 个体差异：8+ 物种 low-poly 模型有明显轮廓区别（羊小绿、狼灰、虎橙、protagonist 人形）
- 主角搭建的蒙古包样建筑：木桩 + 茅草顶 + 围墙 + 入口缝（不需要 AAA 精度）

**行为层**（v2 spec 实施 + autocurricula 后）：
- 羊群聚集吃草，狼出现时整群同向逃跑
- 狼群（≥3）围猎中型 prey，单狼会被 prey 反击赶走
- 虎独居占地，巡视范围互不重叠
- 秃鹫盘旋观察，狼吃完后下来捡尸体
- protagonist 在工地堆木头石头 → 完工后形成实体围墙 → 夜里狼撞墙散开
- protagonist 之间：发信号、配合搬运、伤员被照顾、火堆边聚集
- 季节变化：冬天动物迁徙到向阳坡、protagonist 多采集多砍柴
- 天气：雷暴时所有动物躲入洞穴/树下，闪电可能点燃森林

**生命周期层**（R-006）：
- 出生：父母配对 → 幼体出现 → 跟随父母成长
- 衰老：成体逐渐变 elder → 移动变慢 → 自然死亡
- 死亡：留尸体 → 食腐者来吃 / 病菌传播
- 谱系：每个个体可追踪父母 + 代数 + 起源（main_birth 或 frontier_migration）

**持续运行**（R-002 + PRODUCTION_WORLD_LIFECYCLE）：
- 用户连续观察几十分钟 → 几小时 → 几天，世界**永不 reset / 不黑屏 / 不弹"训练中"**
- 用户可投放事件（闪电、寒潮、新资源点、捕食威胁）→ 系统真实响应
- 用户可暂停 / 跟随某个个体 / 切换上帝视角

### §1.3 不做答辩剧本

本项目不对答辩做特殊适配。最终系统应当对任何观察者（用户、来访者、随机时间打开看的人）都能直接看出 §1.2 描述的所有画面，不需要 4-5 分钟剧本引导。

---

## §2. 10 维度总规格表

### §2.1 维度 1 · 世界规模与拟真度

| 项 | 现状 | 蓝图目标 | spec 来源 |
|---|---|---|---|
| **地图尺寸** | 200×200 | **600×600**（9× 面积） | 用户 2026-05-23 硬约束 |
| **地形** | perlin height + slope_resistance + cave | perlin + 多 biome（草原/森林/河谷/山地/洞穴）+ 显著差异化 | R-002 + 用户硬约束 |
| **资源分布** | 84 食物点 + 14 树 + 52 物件 + 2 水源 + 2 火堆 + 2 worksite（200×200） | **按面积扩 9×** 到 ~750 食物点 + 126 树 + 18 水源 + 18 火堆候选地。**且按 biome 分布**（树在森林多、草在草原多、石在山地多） | 用户硬约束 |
| **植被连续性** | 离散 entity（一棵树一个点） | 草丛**连续分布**（grass density grid）+ 树木**离散但有冠层视觉**（不只是一根棍） | 用户硬约束"拟真连续" |
| **小物件** | sticks/stones 作为 carry 对象 | 增加 mushroom/berry_bush/dead_log/flint_node 等小物件 | 用户硬约束"石头木棍尽量精致" |

**关键决策**：
- ✅ 已拍：600×600
- ⚠️ 待拍：是不是要支持更大（如 1000×1000）？还是 600×600 锁定？(我建议先锁 600×600，证明扩 9× 性能 OK 再考虑更大)

### §2.2 维度 2 · 植被细分

| 树类型 | 视觉 | 季节 | 资源产出 | 备注 |
|---|---|---|---|---|
| **常青针叶**（pine） | 深绿，圆锥形 | 不变 | wood（耐砍） | 山地/北部 |
| **落叶阔叶**（oak） | 春夏绿 / 秋黄 / 冬秃 | 落叶 | wood + occasional acorn | 平原/森林 |
| **果树**（apple/berry） | 春花 / 夏果（食物源）/ 秋落 / 冬秃 | fruit_seasonal | wood + **fruit**（已在 SPECIES_SPEC §3.1） | 河谷/林缘 |
| **枯木**（dead_log） | 灰白 | 不变 | wood（脆易砍）+ 偶尔 mushroom | 老林区 |

**草丛**：连续 density grid（不是 entity），grass_quality grid 影响 large_herbivore 食物效率。

**石头**：3-4 种（小石、大石、黑曜石 flint、矿石）—— flint 是 craft_weapon 的关键资源。

**关键决策**：
- ⚠️ 待拍：4 种树够吗？要不要加竹子 / 灌木？(我建议 4 种锁定，灌木用 grass density grid 表示)

### §2.3 维度 3 · 天气系统

| 天气 | 触发 | 效果 | 视觉 |
|---|---|---|---|
| **晴**（默认） | 70% 时间 | core_temperature ±0 | 天蓝 |
| **多云** | 15% | core_temperature -1（凉爽） | 灰云 |
| **雨** | 10% | hydration 自动恢复 +0.5 / hp -0 / fire 熄灭 | 雨粒子 + 暗 |
| **雷暴** | 3% | 雨 + 5% 概率闪电点燃树（lightning fire 事件）+ 动物躲避 | 雷电 + 暗 |
| **大风** | 1.5% | 移动 cost ×1.5 / signal field 扩散 ×2 | 粒子风 + 树摇 |
| **台风** | 0.5%（罕见，仅夏季） | 大风 + 雨 + 5% 概率吹倒树（变 dead_log） | 强粒子 + 视觉摇晃 |

**weather_duration** 平均 600 ticks（10 分钟实时），不能频繁切换。

**关键决策**：
- ✅ 已拍：6 种天气（用户硬约束"台风/雷雨/多样"）
- ⚠️ 待拍：极寒/暴雪要不要单独列？(我建议雪由"冬季 + 雨" 自动转换，不另立天气)

### §2.4 维度 4 · 季节系统

| 季节 | 占比 | 主要效果 | 视觉 |
|---|---|---|---|
| **春** | 25% | 落叶树发新芽 / 果树开花 / 动物繁殖率 ×1.5 | 嫩绿、花瓣 |
| **夏** | 25% | 果实成熟 / hydration_decay ×1.3（炎热）/ 雷暴更频繁 | 浓绿、强光 |
| **秋** | 25% | 落叶树变黄 / 果实落地 / 动物储食 | 金黄、红 |
| **冬** | 25% | 落叶树秃 / 草地 dormant / 水源结冰 50% / hunger_decay ×1.2 / core_temperature 下降 | 灰白、积雪 |

**一年** = 4800 ticks（一天 1200 ticks × 4 days/season × 4 seasons），即 **80 分钟实时**。

**关键决策**：
- ⚠️ 待拍：一年 80 分钟会不会太快？(可调整到 240 分钟一年——观察用户需求后决定)

### §2.5 维度 5 · 物种 niche

直接采用 `SPECIES_DIFFERENTIATION_SPEC.md` v2 完整设计。**8 个物种 + protagonist，4 层差异化机制**。

可能新增的物种（让生态更循环）：
- **小型啮齿**（rabbit/squirrel）：草食 + 繁殖快 + 食物链底层（被几乎所有 predator 吃）
- **鸟类**（crow/raven）：飞行 + 中性 + 食腐 + 视觉警报源

**关键决策**：
- ✅ 已拍：8 物种 v2 作为基础
- ⚠️ 待拍：要不要再加 1-2 个底层 prey（啮齿/鸟）让食物链更完整？(我建议加 1 种 rabbit 让食物链底层稳，不加鸟避免飞行机制复杂度)

### §2.6 维度 6 · 生理 + 生命周期

直接采用 R-001 + R-006。

**生理系统**（所有活体，不只 protagonist）：
- health / hydration / **hunger**（新）/ **core_temperature**（新）
- 多阶段 debuff（轻/中/重）
- 季节耦合（冬冷夏热）
- 运动负荷耦合（跑得多更耗水耗食）
- 进入 perception 输入决策回路

**生命周期**（所有活体）：
- juvenile（幼体）→ adult（成体）→ elder（老年）→ death
- 年龄影响：移动速度 / 战斗力 / 繁殖能力 / 生存率
- 5 种死法：老死 / 病死 / 战死 / 生理压力死 / 灾难死
- 死后留尸体 → 食腐者 + 疾病源
- 繁殖：父母配对 → crossover + mutation → 幼体出生 → parent_ids + generation + lineage_id 持久化

**疾病系统**（R-006）：
- 接触传播 + 尸体污染
- 年龄/营养/拥挤度影响易感性
- 多代演化抗病能力

### §2.7 维度 7 · 世界可发展（建筑可视）

**核心**：worksite 完工后**变成可见物理实体**，不是 abstract 圆点。

**建筑类型**（实施优先级 L1-L5）：
- **L1 围墙**（已实现机制，需视觉升级）：木桩 + 石头堆，挡 predator。视觉：4-6 根木桩横放，石头堆在底
- **L2 茅草顶**（新）：建在围墙内的简易遮蔽，protagonist 进入 +warmth +hunger_decay 减半
- **L3 蒙古包**（新）：围墙 + 茅草顶 + 入口 + 内部地面，protagonist 群体 sleep 节点
- **L4 火堆改良**（升级已有）：可见火焰粒子 + 烟柱 + 周围木炭区
- **L5 储物堆**（stockpile 可视化）：木头堆 / 石头堆 / 食物堆，**实时随 stockpile 数量变化**

**stigmergic perception**：堆料、尸体、足迹、燃尽火堆这些 environment traces 进入 protagonist perception——用 perception 接管"群体协调"，不用 reward。

**关键决策**：
- ✅ 已拍：蒙古包级别建筑 + 5 种建筑类型
- ⚠️ 待拍：建筑能不能被 predator 破坏（damage + 维护机制）？(我建议先做单向：建完只会因季节/天气慢慢损耗，不被 predator 直接砸——避免训练 task 复杂度)

### §2.8 维度 8 · 持续生产环境（三层世界）

直接采用 `PRODUCTION_WORLD_LIFECYCLE_SPEC.md` 完整设计。

**三层世界**：
- **训练沙盒**：reset / episode / fitness / 用户不可见 — 现有训练继续在这里
- **边境世界**：仅维护期 reset / 自然死亡 / 用户次要可见 — 新血脉迁徙前的存活验证
- **主世界**：**永不 reset** / 自然死亡 / 用户主要可见 / 用户可干预（白名单）

**17 条不变量**：见 `PRODUCTION_WORLD_LIFECYCLE_SPEC.md` §7，全部纳入蓝图。

**lineage_archive**：永久保留所有 lineage（含灭绝），archive 基因必须走 frontier → migration，不可直接复活。

**关键决策**：
- ✅ 已拍：三层世界 + 17 不变量
- ⚠️ 待拍：实施顺序（PRODUCTION_WORLD_LIFECYCLE 的 W1-W6 顺序合适吗？）— 我建议按 W1 → W2 → W3 顺序，W4-W6 推后

### §2.9 维度 9 · 算法栈

**最终算法栈**（神经进化主线，不引 LLM）：

| 层 | 算法 | 当前状态 |
|---|---|---|
| **基础进化** | NEAT（拓扑增长 + speciation） | ✅ 已实施 |
| **大脑核心** | CTRNN + 神经调节 + DNC + HRL goal manager | ✅ Memory Phase A 完成，CTRNN 还是离散 tick |
| **多样性** | MAP-Elites（QualityDiversity）+ 行为 BD | ⚠️ v17 用 3D BD action-count，v18 CVT 失败。需要换 outcome-level BD |
| **Reward** | 极简 outcome reward（survival + death）+ environment-driven（drive penalty + 物种相互压力）+ HER fitness relabel 加 implicit goal（建墙、围火等） | ❌ 当前是 14 个行为目录式 reward |
| **课程** | Autocurricula（OpenAI Hide-and-Seek style）：predator 浪涌强度随代数递增 | ❌ 未实施 |
| **Stigmergic** | 环境 trace 进 perception（堆料 / 尸体 / 足迹），不进 reward | ⚠️ perception 部分可加 |
| **跨代** | Lamarckian（父代学到的传给子代）— 通过 DNC 记忆部分继承 | ❌ 未实施 |
| **持续学习** | 边境世界小流量在线进化 + canary 回归门 | ❌ 未实施 |

**故意不引入**：
- ❌ LLM-as-author（违背神经进化主线）
- ✅ 完整 model-based RL（World Model）— Layer 8 纳入
- ⚠️ ICM / RND / curiosity — Layer 8 选择性加入（如 stigmergic perception 已覆盖部分功能，不必重复）

**关键决策**：
- ✅ 已拍：NEAT + DNC + HRL + MAP-Elites + HER + Autocurricula + Stigmergic perception + **Lamarckian 跨代记忆继承**（Layer 8 纳入）

### §2.10 维度 10 · Visualization (3D)

**技术栈**（用户授权拍）：**Godot 4 + Kenney.nl 资产 + Mixamo 角色动画 + AI procedural mesh**

**资源策略**：
- **现成资产**（用户不建模）：Kenney Nature Kit / Animal Kit / Character Pack（全 CC0）+ Mixamo character animations（Adobe 免费）
- **AI 生成**（用户不会的部分）：蒙古包 / 工地 / 火堆 / 石头堆等用 Godot CSG + procedural mesh 由 AI 写脚本生成
- **风格统一**：low-poly cartoon，不追 AAA

**渲染模式（两种都做完整版）**：
- **Replay mode**：C++ 后端 dump trace.jsonl → Godot loader 加载 + 时间轴播放 + 暂停 / 跟随 / 上帝视角
- **Live intervention mode**：C++ 后端通过 socket/IPC 实时推送世界状态给 Godot → Godot 实时渲染 + 完整 UI 投放事件 + 实时响应可视

**Overlay**：
- 个体生理状态条（血/水/饿/温）
- 死亡谱系树（点开个体看祖先）
- 行为统计时间线
- me_coverage 多样性视图

**关键决策**：
- ✅ 已拍：Godot 4 + Kenney + Mixamo + AI procedural
- ✅ 已拍：Replay + Live intervention 完整 UI 双模式都做

---

## §3. Gap matrix · spec 已有 vs 代码已实施

| 维度 | spec 已有 | 代码已实施 | gap |
|---|---|---|---|
| 1. 世界规模 600×600 | ❌（spec 没明确） | ❌（200×200） | 全新 |
| 2. 植被细分（4 种树 + 草丛 grid） | ⚠️ 部分（SPECIES_SPEC fruit_seasonal） | ❌（单一 tree） | 大半新 |
| 3. 天气系统（6 种） | ❌ | ❌ | 全新 |
| 4. 季节系统 | ⚠️ 部分（SeasonLayer 配置存在） | ⚠️ 弱（无视觉，无完整耦合） | 大半新 |
| 5. 物种 niche v2 | ✅ 完整 | ⚠️ 部分（7 物种已有，但 v2 4 层机制未实施） | 实施 v2 |
| 6. 生理 R-001 | ✅ 完整 | ⚠️ 部分（只 health+hydration，且 protagonist-only） | 加 hunger/temp + 全体覆盖 |
| 6. 生命周期 R-006 | ✅ 完整 | ❌ | 全新 |
| 7. 建筑可视 | ⚠️（FINAL_VISION 描述 + L1 collidable wall 完成） | ⚠️ L1 机制有，视觉 abstract | 视觉升级 + 4 种新建筑 |
| 8. 三层世界 | ✅ 完整（PRODUCTION_WORLD_LIFECYCLE） | ❌（只训练沙盒） | 全新边境 + 主世界 |
| 9. 算法栈 | ⚠️ 部分（NEAT/DNC/HRL/MAP-Elites 已 spec） | ⚠️ 部分（v17 ME-NEAT 实施但效果未达预期） | 重做 reward + 加 HER/Autocurricula |
| 10. Godot 3D viewer | ⚠️ 部分（demo 雏形 + res_* 资源） | ⚠️ 雏形 | 升级到完整 viewer + replay |

**总结**：spec 已经覆盖了大半蓝图（这是个好消息——很多硬规格已经过认真设计）。缺的主要是**代码实施**，而不是**蓝图设计**。

---

## §4. 实施依赖层（不是时间表）

蓝图不分 milestone，目标是**完整实现到位**。但层与层之间有**物理依赖关系**（hunger 没加之前没法做"饿了找食物"），所以仍有顺序。

### 工程速度说明

本项目由 AI 主导写代码。**按人类工程师标准的"每个 layer 几周"工期已删除**——AI 写代码速度远超那个估计。但以下瓶颈不能被加速：

- **训练实验**：每次 30-60 代 → ~30-90 分钟实际 GPU 时间。不能加速。
- **集成调试**：等训练结果 → 修 → 再训练。是 IO-bound 循环。
- **用户审阅 + 决策**：与用户在线时间耦合。
- **回归与稳定**：从"代码能编译"到"系统不崩"需要多轮真实运行。

实际项目工期估计：**1-2 周到几周实际时间**（不是几个月，也不是几天）。每层内部代码工作量按 AI 速度算，但跨层集成 + 训练验证仍需排队。

### Layer 1 · 世界基础

**目标**：把世界从"软沙盒"做实，让 environment 本身能驱动行为，不再依赖 reward 替代环境压力。

- 世界扩到 600×600 + biome 分布
- BaseLayer 加 hunger + core_temperature（R-001）
- 生理多阶段 debuff + 全体活体覆盖
- 季节耦合（冬冷夏热）+ 运动负荷耦合
- DayNightLayer（一天 1200 ticks，sin/cos perception）
- SeasonLayer 视觉升级（树木落叶、积雪、color tint）

**完成判据**：protagonist 真饿真渴真冷热、夜里 vision 减半、季节切换可见 + 生理曲线可见。

### Layer 2 · 天气系统

**目标**：环境多样性，让生态有"今天有事发生"的感觉。

- WeatherLayer（晴/多云/雨/雷暴/大风/台风 6 种）
- 闪电点燃树（lightning fire 事件）
- 大风扩散信号场 / 吹倒树
- 季节×天气联合（夏多雷暴 / 冬无台风等）

**完成判据**：用户连续观察 30 分钟能看到 4 种以上天气切换 + 1 次闪电点火事件。

### Layer 3 · 物种 v2 完整实施

**目标**：SPECIES_DIFFERENTIATION_SPEC v2 完整落地。

- L0 damage threshold（狼群核心：单狼必死、≥3 同时打才能 kill）
- L0 stealth + line-of-sight（豹）
- L0 scavenge efficiency ×3（食腐）
- L1 5 drive states（hunger / thirst / fatigue / fear / loneliness）+ drive-delta reward
- L2 4-channel resource（low_quality_grass / high_quality_browse / fruit_seasonal / corpse）
- L3 autocurriculum（regret-based level selection + 6 templates）
- **新增 1 种 rabbit**（食物链底层稳定 prey，effect 决策 D3）
- ESS 多策略共存（独狼允许活，不强制群猎）

**完成判据**：用户能在镜头里直接区分"独居 apex / 群猎 wolf / 伏击 leopard / 食腐 vulture / 群居 herbivore"5 种生态人格。

### Layer 4 · 生命周期 + 繁殖 + 谱系

**目标**：R-006 完整实施。

- 生命周期 3 阶段（juvenile / adult / elder）+ 年龄影响移动 / 战斗 / 繁殖 / 生存率
- 5 种死法（老 / 病 / 战 / 生理压力 / 灾难）+ 死亡留尸体
- 配对繁殖 + crossover + mutation + 幼体入世
- 疾病系统（接触传播 + 尸体污染 + 年龄/营养/拥挤度易感性）
- lineage_id / parent_ids / generation / origin 持久化到世界 state

**完成判据**：用户能观察到出生 → 成长 → 衰老 → 死亡完整过程 + 谱系树有 ≥3 代。

### Layer 5 · 建造闭环（vision 4.6 核心）

**目标**：实现"采集 → 搬运 → 堆放 → 建造 → 变实体墙 → 挡 predator → 围火睡觉"完整闭环 + 视觉。

- worksite 视觉升级（5 类建筑：围墙 / 茅草顶 / 蒙古包 / 火堆 / 储物堆）
- Reward paradigm 转向（行为目录 14 → outcome-level + drive penalty + environment-driven）
- HER fitness relabel（implicit goal: 建墙完成 / 夜里 shelter / predator wave 无伤亡 / 围火生存）
- Stigmergic perception（堆料 / 尸体 / 足迹进 perception，不进 reward）
- 建筑暂不被破坏（Layer 5 单向，Layer 8 加入 damage + 维护）

**完成判据**：vision 4.6 标志画面真实出现 —— protagonist 群在工地堆木头 → 完工形成实体围墙 → 夜里狼群冲过来撞墙散开 → protagonist 在墙后围火睡觉。**这是整个项目的核心高光**。

### Layer 6 · 持续生产环境（三层世界）

**目标**：PRODUCTION_WORLD_LIFECYCLE_SPEC 完整实施。

- 训练沙盒（保留现状）
- 边境世界 runtime（W3：自然死亡 / 迁徙前存活验证）
- 主世界 runtime（W1+W2：永不 reset / 热保存 / 热恢复 / lineage 持久化）
- incident 录制 + 1-2 类 challenge 翻译（W4）
- archive 持久化 + 谱系流动闭环（W5）

**完成判据**：主世界连续运行 ≥几小时不 reset / 不黑屏 / 不弹"训练中"，至少观察到 1 次迁徙 + 1 次 incident 录制。

### Layer 7 · 3D Replay + Live viewer（Godot）

**目标**：用户能像玩家进入世界。

- Godot viewer 升级（继续现有 godot_demo 雏形）：
  - 加载 trace.jsonl + 时间轴播放
  - 第一人称 / 跟随 / 上帝视角切换
  - 个体生理状态条 overlay
  - 谱系树 overlay
  - me_coverage 多样性视图
- 完整 Kenney + Mixamo 资产接入（Viking 原始人 + Cow + Wolf + 等等已有 + 补 Sheep/Tiger/Rabbit）
- AI procedural mesh（蒙古包升级版、储物堆动态可视）
- **Live intervention 完整 UI**（决策 D6 升级版：完整实时事件投放 + 响应可视）

**完成判据**：用户在 Godot 里能像玩家一样进入主世界、近距离观察个体、实时投放事件并看到响应。

### Layer 8 · 可选高级机制（决策 D5 升级版：纳入核心）

**目标**：把"效果最好"路线的可选项纳入实施。

- **Lamarckian 跨代记忆继承**（决策 D5：纳入 — 父代 DNC 关键 slot 传给子代）
- L4 技能分化（DIAYN-like skill discovery）
- L5 通信协议涌现（emergent communication 任务驱动）
- L6 组合性能力（capability 原子化 — 例如 Touch/Strike/Launch primitives 复合出"砍树/扔石/捕猎"）
- World Model（Ha & Schmidhuber 2018 风格预测器，作为 protagonist 额外 perception）
- 完整 incident → challenge → 训练 → 边境 → 迁徙闭环（W5+W6）

**完成判据**：每个加入的高级机制都有 §R-004 完整证据链（使用证据 + 状态证据 + 决策证据 + 行为证据 + 结果证据 + 消融证据）。

---

### 层间依赖图

```
Layer 1 (世界基础)
   ↓
   ├──→ Layer 2 (天气) ────┐
   ├──→ Layer 3 (物种 v2) ─┤
   └──→ Layer 4 (生命周期)─┤
                          ↓
                    Layer 5 (建造闭环 / vision 4.6)
                          ↓
                    Layer 6 (三层世界)
                          ↓
                    Layer 7 (3D viewer)
                          ↓
                    Layer 8 (高级机制)
```

Layer 1 是所有后续的基础。Layer 2/3/4 可并行（无强依赖）。Layer 5 依赖 Layer 1+3+4。Layer 6 依赖 Layer 5。Layer 7 可与 Layer 6 并行。Layer 8 是 Layer 7 完成后的持续投入。

---

## §5. 关键决策点

### §5.1 已拍（用户授权或显式确认）

1. ✅ 地图 600×600 拟真连续
2. ✅ 6 种天气 + 4 季节
3. ✅ 4 种树 + 草丛 grid
4. ✅ 8 物种 v2 + 4 层差异化 + 加 rabbit = 9 物种
5. ✅ R-001 全体生理（health + hydration + hunger + core_temperature）
6. ✅ R-006 完整生命周期 + 繁殖 + 谱系
7. ✅ 三层世界 + 17 不变量（PRODUCTION_WORLD_LIFECYCLE）
8. ✅ 建筑 5 类（围墙 / 茅草顶 / 蒙古包 / 火堆 / 储物堆），Layer 8 加 damage + 维护
9. ✅ 算法栈：NEAT + CTRNN + DNC + HRL + MAP-Elites + HER + Autocurricula + Stigmergic + Lamarckian
10. ✅ Godot 4 + Kenney + Mixamo + AI procedural（low-poly 风格统一）
11. ✅ Replay + Live intervention 完整 UI 双模式都做（Layer 7）
12. ✅ 效果最好路线（不分 milestone / 不为答辩妥协 / 全部 8 层完成）

### §5.2 决策（按"效果最好"路线全采纳，2026-05-23 用户确认）

用户口径："我的要求就只有一个啊，效果最好"。所有原"待拍"决策按效果最好路线确定：

**D1** 地图最终最大：先锁 600×600，跑通后再评估是否扩到 1000×1000（不锁死上限，但首先要 600×600 跑得好）

**D2** 一年实时多长：**80 分钟一年**（一天 1200 ticks × 4 天/季节 × 4 季节）。让用户能在短时间内观察到完整季节轮替。

**D3** 物种数：**加 1 种 rabbit**（食物链底层稳定 prey），实际是 8 物种 + protagonist + rabbit = 9 物种

**D4** 建筑可否被 predator 破坏：**Layer 5 先做单向**（建完不被砸），**Layer 8 加入 damage + 维护**作为高级机制（效果更好）

**D5** Lamarckian 跨代记忆继承：**Layer 8 纳入核心**（不是可选）。效果最好路线要求做。

**D6** Live intervention：**Layer 7 完整 UI**（不是 1-2 按钮简化版）。效果最好路线要求做。

**D7** 不设答辩 deliverable 概念。**目标 = 全部 8 层完成**。如果中途时间不允许，自然停在某层（停在 Layer 5 已有 vision 4.6 高光画面）。

### §5.3 不会变的硬约束（用户 2026-05-23 口径）

- **没有答辩** —— 不为任何特定时间点的展示做妥协设计
- **效果最好** —— 所有决策选 ambitious 路线，不选 conservative
- **AI 主导工程** —— 按 AI 写代码速度估，不按人类工程师 1-2 周一个 milestone 估
- **现实瓶颈仍存在** —— 训练 / 调试 / 用户审阅 / 回归仍是 IO-bound，不能加速到"几分钟"

---

## §6. 与已有文档的关系

| 文档 | 与本蓝图的关系 |
|---|---|
| `AGENTS.md` | 上级（协作规则） |
| 本文件 | **总蓝图（宪法）** |
| `PROGRESS.md` / `STATUS.md` | 反映当前工程进度 vs 本蓝图的 Layer 1-8 |
| `DEVELOPMENT_TASK_PLAN.md` | Layer 1-8 的具体任务分解 |
| `DEVELOPMENT_EXECUTION_GUIDE.md` | 反绕路指南，基于本蓝图 |
| `supporting/*.md` | 各维度的详细 spec，本蓝图通过 §0 引用 |
| `archive/*.md` | 历史参考，不再驱动决策 |

---

## §7. 变更与维护

- 本蓝图变更需用户显式同意（除显然 typo / 引用补全）
- 任何 Layer 重大延期或完成判据变更需更新本蓝图
- 任何与蓝图冲突的工程决策必须先更新蓝图
- 蓝图本身不是 staged plan；staged plan 在 `DEVELOPMENT_TASK_PLAN.md`

---

## §8. 最终一句话

> 一个由神经进化驱动的、用户可参与的、持续运行的原始聚落生态沙盒，8 层全部完成时是一个连续 600×600 拟真世界 + 9 物种生态 + 完整生命周期 + 蒙古包级建筑闭环 + 三层世界永不 reset + Godot 3D viewer 玩家可进入的完整产品。**唯一指标：效果最好**。
