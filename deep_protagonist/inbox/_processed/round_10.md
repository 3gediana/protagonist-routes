# round_10 — deep_protagonist · Diag-1/2 结论 + Stage3 Level-1(造衣) 结构性阻塞 + 求向

UTC 2026-05-31 03:12 · executor · 引用 decision_round_9 + decision_round_9_addendum(fire)

## 0. 一句话
冠军仍是 **bc_v9**（未动）。本轮按 decision_round_9 跑了**零训练诊断 Diag-1/Diag-2**（结论都成立、都指向"冷/造衣"是对的方向），并实现了 oracle **Layer 3.5 WARMTH-PREP**（造衣层）+ smoke。但 smoke 暴露一个**结构性阻塞**：当前 `Inventory::CAPACITY==3` 且一件草裙要 **3 格草**（占满整个背包），agent 永远凑不齐 → 造衣层在现机制下**几乎无法触发**。需要你/决策者就"如何解锁造衣"或"是否直接上火"定向。

## 1. Diag-1（冻死 ep 确认，纯分析现有 bc_v9 eval jsonl，零训练）
**① clothing token ≈ 0：成立。** 13 个 seed × 10 局里，craft_grass_dress(act7)+wear(act9) 全程 ≈0（clone 从不穿衣）。

**② 死因=冷驱动：成立（对瓶颈 seed）。** 用 r_bites(<-1=狼咬) vs nShel(夜暴露) 给每个死亡 ep 归因，再叠加因果论证：DACHU 局里 agent 整局 r_food=r_water=0 仍存活（说明饿/渴在一局内不致死，只要有屋）→ 未庇护时的非狼死亡只能是夜冷=冻死。逐 seed：

| seed | DACHU% | 总死 | 狼咬 | 冻死 | nShel | 判定 |
|---|---|---|---|---|---|---|
| **2100000** | **40** | 5 | 1 | **4** | 0.47 | **冻死主导**（瓶颈 seed）|
| 1000000 | 60 | 3 | 1 | 2 | 0.77 | 冻死主导 |
| 555000(san) | 60 | 1 | 0 | 1 | 0.74 | 冻死 |
| 1700000 | 80 | 2 | **2** | 0 | 1.00 | **狼咬**（穿衣救不了它）|
| 900000 | 80 | 3 | 3 | 0 | 1.00 | 狼咬 |
| 其余高分 seed | 90+ | ≤2 | 多狼 | — | ~1.0 | 零散狼咬，不拖中位 |

**结论**：拖低中位的 **低分 seed（2100000/1000000/555000）是冻死主导** → 御寒(造衣/火)是对的杠杆。狼咬死都发生在已 80-90% 的 seed（零散单发），不拖中位，留给后续 fire 驱兽/狩猎层。

## 2. Diag-2（r8/bc_v10 退化归因，轻量 A/B）
关键判别：**无狼的纯定居 seed 700000/800000 是否也退化？**

| | v9 | v10 | Δ | v10 新增死法 |
|---|---|---|---|---|
| 700000(CLEAN,无狼) | 100 | 80 | **-20** | +1 **冻死** |
| 800000(CLEAN,无狼) | 100 | 80 | **-20** | +2 **冻死** |
| 1100000 | 90 | 50 | -40 | +4 冻死 |
| 1700000 | 80 | 10 | -70 | +2 冻死 |
| move 幅度(mv) | 0.55-0.58 | 0.55-0.57 | ~0 | 无"逃跑"行为漂移 |

**结论**：r8 退化 = **容量稀释（教训A）主导，不是早狼 flee 信号污染**。证据：(1) 无狼 clean seed 也退化，且新增的是**冻死**不是狼咬；(2) move 幅度无变化（没学出逃避行为）；(3) 冻死在全 bank 普遍上升。→ 从零稀释重训把"建房御寒"核心冲淡了。**Level-1 必须用聚焦增量 DAgger + 维持已赢分布，绝不重复 r8 式从零稀释。** 另：这也**再次坐实冻死是主失败模式**。

## 3. Level-1 造衣实现 + smoke（已做）
- 加 oracle **Layer 3.5 WARMTH-PREP**（在 Layer3 维持 与 Layer4 建造 之间）：冷风险(approaching_night/is_night/temp<0.55) 且 worn_none 时 → (a)有衣就穿 / (b)满草就造 / (c)定居完成+白天+无在建工地 时就近补草。grass-dress only、**从不 attack**、**不加任何奖励**、自限（穿上即停）。
- 同时修了 **Layer 6 沿用的老 bug**：阈值 `inv_g > 3/20`(=0.15) 在 `/CAPACITY` 归一下其实是"≥1 格草"，于是 craft 在草不足 3 时被反复请求却失败（craft_grass_dress() 要 3 格草），grass 不消耗→**死循环**。已改为需"满草载"(>0.95)。

**smoke（settler oracle，3 fresh seed×3 ep）**：
- 修前：dress(craft)=**1101/141/1014**、**wear=0** ← craft 死循环暴露 bug。
- 修后：dress=**0**、wear=**0**、attack=0、nShel 0.99-1.00、bldg ~8533-8965、deaths=0 ← 循环消除，但**完全不造衣了**。

## 4. ★结构性阻塞（核心发现，需定向）★
`Inventory::CAPACITY == 3`（背包只有 3 格）。一件草裙 = **3 格草**（占满整包）。spawn 起手包=Wood+Stone+Grass（满，仅 1 草）。Layer7 又持续把 wood/stone/grass 各补到 1 维持建造缓冲 → 背包恒满 → **agent 永远凑不出 3 格草**（满包时 collect 不进），造衣在**现机制下几乎不可能**。这正是 Diag-1 里 clone 造衣 token≈0 的根因（不仅是没示范，是机制上太贵）。

矛盾点：`Environment.cpp:253` 注释明说预置草"so first craft_grass_dress works step 0"，但配方要 3 草而起手只给 1 草——**配方成本(3)与 3 格背包/起手设计意图(step0 可造)自相矛盾**，疑似历史回归 bug。

含义：
- **造衣 Level-1 在不改 sim 的前提下基本无法落地**（除非改配方/背包/加丢弃动作）。
- **火(Level-2)恰好绕开此约束**：营火是**放置在世界里的实体**，不占 3 格背包；且是主动热源(temp 回升)，比草裙 resist=0.10 的被动御寒强得多。用户 fire 硬指令在这个约束下**结构上更对**。

## 5. 蓝图位置
Stage2 双 bank 已确认（90% 真泛化）。Stage3 升级序列：Level-1 造衣（**本轮卡在结构阻塞**）→ Level-2 火（用户硬指令、已授权、排在 Level-1 后）。冠军 bc_v9 未动；attack 全程 0；评测 bank 只读未碰；fresh seed 用 2200100+ 新段。

## 6. 候选下一步（请决策者选/改）
- **A（推荐，最快见效）**：跳过/降级造衣，**直接上 Level-2 火**。火是用户硬指令、是冻死的最强杠杆、且绕开 3 格背包约束。先实现 sim 营火机制(取火/搭火/续柴/半径取暖挂进 VitalSystem)+机械 smoke+单测 → oracle fire 层 → 聚焦 DAgger(冷 seed) → bc_v12 → 多轴防退门。造衣留作后续可选小补丁。
- **B（先小修解锁造衣，再 bc_v11）**：把草裙配方成本 `3 grass → 1 grass`（与 `Environment.cpp:253` "step0 可造"的原设计意图一致，属 bugfix 而非平衡作弊）。这样起手即可造+穿，WARMTH-PREP 立即生效。然后聚焦 DAgger→bc_v11→多轴门。再上火。**需你确认这算 bugfix 而非"为指标放水"。**
- **C（最保守）**：保 bc_v9 不动，造衣判定为"现机制不可行"归档，直接进火（同 A 但明确不做任何造衣改动）。

我倾向 **A 或 B**：若你认为草裙 3→1 是合理 bugfix 就 B（顺带把造衣这层补全，再上火）；若想集中火力就 A。等 decision_round_10（10min）。期间我并行读 sim 代码(BuildingSystem 放置实体 / VitalSystem 热源)预设计火，不停。

## 7. 红线自检
✅ 未改奖励（无 r_warmth/r_clothing）✅ attack 全 0 ✅ 未碰评测 bank ✅ bc_v9 冠军未动、已备份 ✅ oracle 改动先 smoke ✅ WARMTH-PREP 经 smoke 确认对定居无扰动(nShel/bldg/deaths 不变)
