# 模块仓库（F-* Modules）

> baseline 完成后从这里挑功能加。每张卡片是一个独立可加可删的模块。

> 注：按 [ADR-0005](../adr/0005-defer-visualization-until-mechanics-stabilize.md)，当前 baseline 的实际推进顺序是 `M1 -> M3 -> M4 -> M5 -> 可视化整合`。因此模块库仍然默认在机制 baseline 完成后再打开，而不是在 M2 之后提前进入。

---

## 怎么用这个目录

1. 完成 baseline M5 后再来
2. 按"想看什么效果"挑模块（不是按"想用什么算法"）
3. 一次只加一个模块。**前一个模块完美才加下一个**（参见 [ADR-0004](../adr/0004-incremental-perfect-then-add.md)）
4. 每张卡片都包含：完美版/标准版/降级版三档预期，对应"跑出来超棒/正常/失败时怎么挽救"

---

## 模块状态符号

| 符号 | 含义 |
|---|---|
| 📋 | 卡片就位，待开发 |
| 🚧 | 开发中 |
| ✅⛳⛳ | MVP 通过 |
| ✅✅⛳ | 完整通过 |
| ✅✅✅ | 完美通过 |
| ⚠️ | 降级版采纳（未达完美但已挽救） |

---

## 模块清单

### 🤝 交互类

| 模块 | 作用 | 难度 | 依赖 | 状态 |
|---|---|---|---|---|
| [F-COMM](F-COMM_communication.md) | 信号场通信，进化原始语言 | 🔴 高 | M5 | ✅⛳⛳ |
| [F-COOP](F-COOP_cooperation.md) | 合作机制，共同任务奖励 | 🔴 高 | F-COMM | ✅⛳⛳ |
| [F-FIGHT](F-FIGHT_combat.md) | 对抗机制，攻击防御能量转移 | 🟡 中 | M5 | ✅⛳⛳ |
| [F-PRED](F-PRED_predator_prey.md) | 捕食者-猎物多物种共进化 | 🟡 中 | F-FIGHT | ✅⛳⛳ |
| [F-TOOL](F-TOOL_tools.md) | 工具与可移动物体，涌现"建筑" | 🔴 高 | M5 | 📋 |
| [F-LEARN](F-LEARN_lifetime_learning.md) | 终身学习 + 拉马克开关 | 🟡 中 | M3 | ✅⛳⛳ |
| [F-NEUROMOD](F-NEUROMOD_neuromodulation.md) | 神经调节情绪 | 🟡 中 | M3 | ✅⛳⛳ |

### 🌍 环境类

| 模块 | 作用 | 难度 | 依赖 | 状态 |
|---|---|---|---|---|
| [F-DAY](F-DAY_day_night.md) | 昼夜循环 | 🟢 低 | M5 | ✅⛳⛳ |
| [F-SEASON](F-SEASON_seasons.md) | 季节变化 | 🟢 低 | M5 | ✅⛳⛳ |
| [F-DISEASE](F-DISEASE_disease.md) | 疾病传播 | 🟡 中 | M5 | ✅⛳⛳ |
| [F-RESOURCE](F-RESOURCE_dynamic_resource.md) | 资源衰竭与再生 | 🟢 低 | M1 | ✅⛳⛳ |
| [F-CAVE](F-CAVE_cave_system.md) | 进阶洞穴系统 | 🟡 中 | M5 | 📋 |

### 🔬 进化算法类

| 模块 | 作用 | 难度 | 依赖 | 状态 |
|---|---|---|---|---|
| [F-NOVELTY](F-NOVELTY_novelty_search.md) | 新颖性搜索 | 🟡 中 | M3 | ✅⛳⛳ |
| [F-COEVO](F-COEVO_coevolution.md) | 多种群共进化 | 🟡 中 | M5 | ✅⛳⛳ |
| [F-NSGA](F-NSGA_multi_objective.md) | 多目标进化 NSGA-II | 🟡 中 | M3 | ✅⛳⛳ |
| [F-CURRICULUM](F-CURRICULUM_curriculum.md) | 课程学习逐级解锁难度 | 🟢 低 | M3 | ✅⛳⛳ |

### 🔬 观测类（新增）

| 模块 | 作用 | 难度 | 依赖 | 状态 |
|---|---|---|---|---|
| [F-TREE](F-TREE_phylogenetic.md) | 物种谱系树 | 🟡 中 | M3 | ✅⛳⛳ |
| [F-HEAT](F-HEAT_heatmap.md) | 行为热力图 | 🟢 低 | M1 | ✅⛳⛳ |
| [F-FORK](F-FORK_parallel_universe.md) | 平行宇宙存档 | 🟡 中 | M3 | ✅⛳⛳ |
| [F-STAT](F-STAT_advanced_stats.md) | 高级统计 | 🟡 中 | M3 | ✅⛳⛳ |
| [F-REPLAY](F-REPLAY_replay.md) | 录制回放 | 🟡 中 | M3 | 📋 |

### 🌍 环境交互类（新增）

| 模块 | 作用 | 难度 | 依赖 | 状态 |
|---|---|---|---|---|
| [F-TOOL](F-TOOL_terrain_mod.md) | 物种改造地形 | 🟡 中 | M5 | ✅⛳⛳ |

### 🎬 演示类

| 模块 | 作用 | 难度 | 依赖 | 状态 |
|---|---|---|---|---|
| [F-GOD](F-GOD_god_panel.md) | 上帝按钮实时干预 | 🟢 低 | M2 | 📋 |
| [F-LLM](F-LLM_narrator.md) | LLM 实时解说员 | 🟡 中 | M5 | 📋 |
| [F-REPLAY](F-REPLAY_replay.md) | 录制 / 回放系统 | 🟡 中 | M3 | 📋 |
| [F-COMPARE](F-COMPARE_side_by_side.md) | 对照实验左右分屏 | 🔴 高 | F-FORK | 📋 |

---

## 推荐路径（如果你不知道从哪个开始）

完成 baseline M5 后的推荐顺序：

### Stage A — 答辩底线"看得见的进化"（必做，约 1 周）
1. **F-TREE** 物种树 → 拓扑进化的最强可视化
2. **F-HEAT** 行为热力图 → 涌现现象最直观
3. **F-GOD** 上帝按钮 → 答辩互动神器

### Stage B — 中等亮点"涌现复杂度"（争取做 1-2 个，约 1-2 周）
4. **F-NEUROMOD** 神经调节情绪 → "看起来像活物"
5. **F-COMM** 通信信号场 → 通信涌现尝试
6. **F-LEARN** 拉马克开关 → 哲学对照实验

### Stage C — 王炸彩蛋（运气项，时间够再做）
7. **F-COOP** 合作机制 → 围猎涌现
8. **F-LLM** 解说员 → 答辩超 surprise

### Stage D — 锦上添花（视心情和时间）
9. **F-FORK** 平行宇宙 → 真"科学家"既视感
10. **F-COMPARE** 对照实验 → 演示效果倍增

---

## 模块卡片格式约定

每张卡片都遵循统一结构：
```
# F-XXX · 标题
## 一句话
## 前置条件
## 你会看到什么效果（完美版 / 标准版 / 降级版）
## 实现要点
## 三档完美标准
## 🧠 你的补充点
## 🔌 接入点
## ⚠️ 易踩坑
## 答辩话术（怎么讲这个东西）
```

---

## 当前阶段状态

```
baseline:   M0 ✅ / M1 ✅ / M2 ⏸ / M3 ✅ / M4 ✅ / M5 ✅
modules:    F-FIGHT ✅⛳⛳ / F-NEUROMOD ✅⛳⛳ / F-LEARN ✅⛳⛳ / F-DAY ✅⛳⛳ / F-SEASON ✅⛳⛳ / F-DISEASE ✅⛳⛳ / F-RESOURCE ✅⛳⛳ / F-COMM ✅⛳⛳ / F-COOP ✅⛳⛳ / F-NOVELTY ✅⛳⛳ / F-PRED ✅⛳⛳ / F-CURRICULUM ✅⛳⛳ / F-COEVO ✅⛳⛳ / F-NSGA ✅⛳⛳ / F-TREE ✅⛳⛳ / F-HEAT ✅⛳⛳ / F-STAT ✅⛳⛳ / F-FORK ✅⛳⛳ / F-TOOL ✅⛳⛳ / F-REPLAY 📋 / others 📋
```
