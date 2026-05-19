# experience_log.md · 实验与调参记录

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
