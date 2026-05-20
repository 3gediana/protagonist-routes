# 系统完善计划 · 让机制真正"活起来"

> **目标**：让每个机制不只是"存在"，而是有**连续可追踪的变化过程**、**机制间的联动**、**物种间的行为差异**。用户要看到的是"真的在发生"，不是"开头和结尾的数字"。

---

## 一、当前问题诊断

### 1.1 通信涌现失败（MI < 0.3）

**现状**：signal_behavior_mi_avg ≈ 0.11，远低于 0.3 阈值。

**文献根因**（Grim et al. 2004; Rawal & Miikkulainen 2012, 2014）：
- 通信涌现的**必要条件**是：信号能**间接帮助生存**，而非直接感知目标
- 当前系统 FoodScentPerception 已经给了粗略方向，SignalPerception 给精确位置——但**信号发出者没有足够的激励**
- 文献发现：**食物稀少且难以定位**时，食物信号才容易涌现；**捕食者威胁大**时，警报信号才容易涌现
- 当前 backprop reward 太弱：receiver 吃到食物后只给 source 0.3×reward delta，但 delta 本身很小

**解决方案**：
- (a) **增加"信号引导觅食"直接奖励**：当 receiver 在信号附近吃到食物时，给 source 一个固定奖励（不是按比例，而是固定值如 +5），这比按比例回溯有效得多
- (b) **减少 channel 数 4→2**：4 个 channel 太多，语义难以分化；2 个 channel 更容易涌现"食物/危险"二分语义
- (c) **食物更稀少更集中**：减少食物数量但增加单个食物量，让信号"指路"的价值更高
- (d) **增加信号衰减距离**：让信号只在近距离有效，迫使 agent 靠近信号源才能获益

### 1.2 病原体-宿主军备竞赛不可见

**现状**：host_immunity_pathogen_virulence_corr ≈ 0，看不到连续变化。

**问题**：
- PathogenGenome 的 31 个权重没有可视化，无法看到毒力如何变化
- immunity_gain_rate 虽然在进化，但 CSV 只记录平均值，看不到个体层面的免疫-感染动态
- 缺少"毒株追踪"：不知道哪个毒株在什么时间感染了谁

**解决方案**：
- (a) **pathogen_strain_log.jsonl**：每代记录所有活跃毒株的"签名"（权重向量前3主成分）+ 感染数 + 宿主物种
- (b) **infection_timeline.csv**：每个 tick 记录 [tick, agent_id, event(infected/recovered/dead), strain_id]
- (c) **virulence_trend.csv**：每代记录病原体 NN 的平均输出（= 毒力代理值），连续追踪毒力上升/下降

### 1.3 物种没有行为差异

**现状**：NEAT speciation 按拓扑/权重距离分物种，但所有物种享受完全相同的 reward/capability 配置。物种间没有生态位分化。

**文献**（Behavior-based Speciation, Risi & Stanley 2010）：
- NEAT 的物种是**基因型物种**，不是**生态位物种**
- 真正的行为分化需要：不同物种面对不同的选择压力
- 生态位分化通常来自：资源分区、栖息地偏好、时间活动差异

**解决方案**：
- (a) **栖息地偏好奖励**：给每个物种一个"偏好区域"（由其物种中心位置决定），在偏好区域内给额外 survival reward
- (b) **饮食分化**：某些物种只能吃特定类型的资源（如只有特定 channel 的信号附近的资源）
- (c) **活动时间分化**：某些物种白天活跃，某些夜晚活跃（利用已有的 day_night 系统）
- (d) **物种行为画像**：每代输出 species_behavior_profile.csv，记录每个物种的平均位置、活跃时间、食物来源比例

### 1.4 机制间联动不足

**现状**：各模块独立运行，缺少交叉影响。

**应该有的联动**（基于生态学文献）：

| 联动 | 当前状态 | 应该实现 |
|------|----------|----------|
| 地形→疾病 | ❌ 无 | 洞穴内感染率降低（shelter），低洼处感染率升高（潮湿） |
| 地形→信号 | ❌ 无 | 山脊阻挡信号传播（已有 isVisible 但未用于信号） |
| 地形→工具 | ❌ 无 | 石头滚下斜坡（物理），洞穴内放置物体建造巢穴 |
| 疾病→信号 | ❌ 无 | 感染个体发出"求救"信号（channel 3），健康个体回避 |
| 昼夜→捕食 | 部分 | 应该：夜间捕食者更强（伏击），白天猎物更易发现捕食者 |
| 季节→疾病 | ❌ 无 | 冬季感染率升高，夏季降低 |
| 工具→合作 | ❌ 无 | 搬大石头需要 2 个 agent 合作（类似 HeavyResource） |
| 信号→合作 | 部分 | 信号应能标记合作任务位置 |
| 挖掘→洞穴→庇护 | 部分 | 挖的洞穴应成为庇护所，降低被捕食概率 |

### 1.5 网络复杂度可能不足

**现状**：初始 NEAT 网络 = input_dim × output_dim 全连接。典型配置 ~20 input × 8 output = ~160 连接。NEAT 通过变异逐步增加。

**文献**（Beer 2003; Stanley & Miikkulainen 2002）：
- Beer 的 CTRNN 用 5-15 个神经元就能产生趋食/避障行为
- 但**工具使用、合作、信号语义**需要更复杂的内部表征
- NEAT 的优势正是**从简单开始逐步复杂化**，但需要足够的进化代数和选择压力
- 当前 100-200 代可能不够让网络复杂到支持高级行为

**建议**：
- 不需要手动增加初始网络大小（NEAT 会自动增长）
- 但需要**增加进化代数**到 500+，让网络有时间复杂化
- 增加 add_node_rate 到 0.05（当前 0.03），加速网络增长
- 增加 population_size 到 40+，维持更多物种探索

### 1.6 地图太小

**现状**：80×80 的世界，24 个 agent。平均每个 agent 占据 ~267 平方单位的空间。

**问题**：
- Agent 密度太高，信号/合作/捕食几乎"必然发生"而非"涌现"
- 地形变化太少（64×64 高度场在 80×80 世界中太密）
- 没有足够的空间让物种在地理上隔离

**建议**：
- 世界扩大到 200×200
- 高度场分辨率 128×128
- 人口增加到 40-60
- 资源数量按比例增加但密度降低

---

## 二、连续变化追踪方案

用户核心需求：**看到变化过程，不只是开头和结尾**。

### 2.1 逐代追踪（已有基础，需扩展）

| 追踪内容 | 当前 | 需要添加 |
|----------|------|----------|
| fitness 曲线 | ✅ evolution.csv | - |
| 物种数量/大小 | ✅ species_log.jsonl | - |
| 信号 MI | ✅ emergence_audit.csv | - |
| 捕食成功率 | ✅ emergence_audit.csv | - |
| 病原体毒力趋势 | ❌ | **virulence_trend.csv** |
| 免疫人口比例 | ❌ | **immunity_demographics.csv** |
| 毒株多样性 | ❌ | **strain_diversity.csv** |
| 物种行为画像 | ❌ | **species_behavior_profile.csv** |
| 工具使用统计 | ❌ | **tool_usage_stats.csv** |
| 地形修改统计 | ❌ | **terrain_mod_stats.csv** |
| 洞穴占用率 | ✅ emergence_audit.csv | 需要每代追踪而非仅最后50代平均 |

### 2.2 逐 tick 追踪（新需求）

| 追踪内容 | 文件 | 格式 |
|----------|------|------|
| 感染事件 | infection_timeline.csv | tick, agent_id, species_id, event, strain_id |
| 捕食事件 | hunt_timeline.csv | tick, pred_id, prey_id, success |
| 信号发送 | signal_timeline.csv | tick, source_id, channel, pos_x, pos_y |
| 工具拾取/放下 | tool_timeline.csv | tick, agent_id, object_id, event(pick/drop) |
| 合作任务完成 | coop_timeline.csv | tick, task_id, workers, reward |

> 注意：逐 tick 追踪会产生大量数据，建议**每 5 tick 采样一次**，且仅在关键事件发生时记录。

### 2.3 可视化连续变化（M2 阶段）

当进入 M2 可视化时，以上追踪数据将用于：
- **实时热力图**：agent 位置/信号强度/感染区域
- **时间轴图表**：fitness/MI/毒力/物种数随代际变化
- **事件回放**：选择某个 tick，回放该 tick 的所有事件
- **物种行为对比**：不同物种的位置分布/活动时间/食物来源饼图

---

## 三、机制联动详细设计

### 3.1 地形→疾病联动

```
DiseaseLayer::tick():
  for each agent:
    if terrain->inCave(agent.pos):
      agent.infection_prob *= 0.3   // 洞穴庇护，大幅降低感染
    if terrain->heightAt(agent.pos) < low_threshold:
      agent.infection_prob *= 1.5   // 低洼潮湿，增加感染
```

### 3.2 地形→信号联动

```
SignalLayer::sample(pos):
  // 信号不能穿过山脊
  for each sample point:
    if terrain->isVisible(agent.pos, sample_point) == false:
      signal_at_point *= 0.1  // 大幅衰减被遮挡的信号
```

### 3.3 疾病→信号联动

```
EmitSignalCapability::apply():
  if agent->infected():
    // 感染个体自动在 channel 3 发出"求救/感染"信号
    signal_layer->emit(pos, channel=3, intensity=infection_progress)
```

同时：
```
SignalPerception::sense():
  // channel 3 高强度 → 回避行为（通过 brain 学习）
  // 如果 brain 学到了回避 channel 3，就实现了"感染回避"行为
```

### 3.4 昼夜→捕食联动

```
PredatorHuntCapability::apply():
  auto* tod = world->getLayer<TimeOfDayLayer>();
  float light = tod ? tod->lightLevel() : 1.0;
  if (agent->isPredator()):
    hunt_radius *= (1.5 - 0.5 * light)  // 夜间 hunt_radius 更大（伏击）
  else:
    flee_radius *= (0.5 + 0.5 * light)  // 白天 flee_radius 更大（容易发现捕食者）
```

### 3.5 季节→疾病联动

```
DiseaseLayer::tick():
  auto* season = world->getLayer<SeasonLayer>();
  float season_factor = season ? season->infectionModifier() : 1.0;
  // winter=1.5, summer=0.7, spring/autumn=1.0
  infection_prob *= season_factor;
```

### 3.6 工具→合作联动

```
ObjectLayer:
  // 某些物体标记为 "heavy"
  // heavy 物体需要 2 个 agent 同时 pick_up 才能搬动
  // 类似 HeavyResourceLayer 的合作机制
  struct MovableObject:
    bool heavy;  // 新增
    std::size_t carriers_required;  // 新增
```

### 3.7 挖掘→洞穴→庇护联动

```
TerrainModifier::apply():
  // 挖掘创建的洞穴自动成为庇护所
  // 已有 cave_mask，需要让 TerrainModifier 修改 cave_mask
  terrain->caveMask().addCave(agent.pos, dig_radius);
```

同时：
```
// 洞穴内 agent 被捕食概率降低
PredatorHuntCapability::apply():
  if terrain->inCave(prey.pos):
    damage *= 0.3  // 洞穴庇护
```

---

## 四、物种行为分化设计

### 4.1 当前物种分化机制

NEAT speciation 按**拓扑+权重距离**分物种。这产生的是**基因型物种**，不是**生态位物种**。

### 4.2 生态位分化方案

**方案 A：栖息地偏好（最简单，最有效）**

每个物种有一个"偏好中心"（取该物种所有成员的平均位置）。在偏好中心附近的成员获得额外 survival reward。

```
// 在 SurvivalReward::evaluate() 中
auto species_center = species_centers[agent.speciesId()];
float dist_to_center = distance(agent.pos, species_center);
float habitat_bonus = exp(-dist_to_center² / (2 * habitat_sigma²));
fitness += habitat_bonus * habitat_weight;
```

效果：物种会在地理上聚集，不同物种占据不同区域 → 自然产生行为差异。

**方案 B：饮食分化（中等难度）**

不同物种对食物来源有偏好：
- 物种 A：主要吃普通资源
- 物种 B：主要吃 heavy resource（合作获取）
- 物种 C：主要吃其他 agent（纯捕食者）

实现：给每个物种一个 `food_preference` 向量（可进化），影响不同食物来源的 reward 权重。

**方案 C：活动时间分化（利用已有 day_night）**

不同物种在不同时间活跃：
- 昼行性：白天代谢低，夜晚代谢高（被迫休息）
- 夜行性：夜晚代谢低，白天代谢高

实现：给每个物种一个 `nocturnality` 基因（0=纯昼行，1=纯夜行），影响 base_metabolism 随昼夜变化。

### 4.3 物种行为画像输出

每代输出 `species_behavior_profile.csv`：

```
generation,species_id,population,avg_x,avg_y,avg_energy,pct_nocturnal,pct_carnivore,pct_in_cave,avg_signal_ch0,avg_signal_ch1,hunt_attempts,food_eaten
```

这样就能**连续看到**每个物种的行为如何随代际变化。

---

## 五、参数调整建议

| 参数 | 当前值 | 建议值 | 理由 |
|------|--------|--------|------|
| world_size | 80×80 | 200×200 | 空间更大，物种可地理隔离 |
| population_size | 24 | 48 | 更大种群维持更多物种 |
| generations | 100-200 | 500+ | 网络需要时间复杂化 |
| episode_ticks | 200 | 300 | 更长 lifetime 让行为充分展现 |
| signal_channels | 4 | 2 | 少 channel 更易分化语义 |
| signal_backprop_factor | 0.3 | 0.8 | 更强的回溯奖励 |
| signal_emit_cost | 0.05 | 0.02 | 降低发送成本，鼓励信号 |
| add_node_rate | 0.03 | 0.05 | 加速网络复杂化 |
| add_modulatory_rate | 0.01 | 0.03 | 增加神经调节节点出现概率 |
| terrain_grid_resolution | 64 | 128 | 更细腻的地形 |
| heavy_required_workers | 2 | 2 | 保持不变 |
| disease_infection_prob | 0.1 | 0.15 | 增加感染压力，驱动免疫进化 |
| pred_ratio | 0.25 | 0.2 | 稍降捕食压力，给通信/合作留空间 |

---

## 六、TODO 列表（按优先级）

### 🔴 P0 · 连续追踪基础设施（让变化"看得见"）

- [ ] **T1** 创建 `TimelineLogger` 类：统一管理逐 tick 事件记录，每 5 tick 采样，关键事件即时记录
- [ ] **T2** 实现 `infection_timeline.csv`：tick, agent_id, species_id, event(infected/recovered/dead), strain_signature
- [ ] **T3** 实现 `hunt_timeline.csv`：tick, pred_id, prey_id, success, pred_species, prey_species
- [ ] **T4** 实现 `virulence_trend.csv`：generation, avg_virulence_output, max_virulence, strain_count
- [ ] **T5** 实现 `species_behavior_profile.csv`：generation, species_id, pop, avg_pos, pct_cave, pct_infected, food_source_ratio
- [ ] **T6** 实现 `tool_usage_stats.csv`：generation, pick_count, drop_count, objects_moved, avg_carry_duration

### 🔴 P0 · 机制联动（让系统"有意思"）

- [ ] **T7** 地形→信号联动：山脊遮挡信号传播（SignalLayer 采样时检查 isVisible）
- [ ] **T8** 地形→疾病联动：洞穴降低感染率，低洼增加感染率
- [ ] **T9** 疾病→信号联动：感染个体自动在 channel 3 发出信号
- [ ] **T10** 昼夜→捕食联动：夜间捕食者 hunt_radius 增大，白天猎物 flee_radius 增大
- [ ] **T11** 季节→疾病联动：冬季感染率 ×1.5，夏季 ×0.7
- [ ] **T12** 挖掘→洞穴联动：TerrainModifier 修改 cave_mask，创建新庇护所
- [ ] **T13** 洞穴→捕食联动：洞穴内 agent 被捕食伤害降低 70%

### 🟡 P1 · 通信涌现修复

- [ ] **T14** 增加"信号引导觅食"直接奖励：receiver 在信号附近吃到食物时，source 获得 +5 固定奖励
- [ ] **T15** 减少 signal_channels 4→2
- [ ] **T16** 调整食物配置：减少数量、增加单量，让信号"指路"价值更高
- [ ] **T17** 创建 `comm_emergence_v2.toml` 配置并跑 500 代验证 MI ≥ 0.3

### 🟡 P1 · 物种行为分化

- [ ] **T18** 实现栖息地偏好奖励：species_center 计算 + 距离衰减 bonus
- [ ] **T19** 实现活动时间分化：nocturnality 基因（可进化），影响代谢随昼夜变化
- [ ] **T20** 实现饮食分化：food_preference 基因，影响不同食物来源的 reward 权重
- [ ] **T21** 验证物种行为差异：跑 500 代，检查 species_behavior_profile.csv 是否显示分化

### 🟢 P2 · 参数与规模调整

- [ ] **T22** 世界扩大到 200×200，人口 48，地形分辨率 128
- [ ] **T23** 增加 add_node_rate 到 0.05，add_modulatory_rate 到 0.03
- [ ] **T24** 创建 `full_ecosystem_v2.toml` 综合配置
- [ ] **T25** 跑 500 代综合验证，检查所有联动和追踪是否工作

### 🟢 P2 · 工具→合作联动

- [ ] **T26** ObjectLayer 增加 heavy 物体类型，需要 2 agent 同时 pick_up
- [ ] **T27** 实现 `CoopCarryCapability`：两个 agent 共同搬运重物
- [ ] **T28** 验证合作搬运涌现

---

## 七、文献参考

1. **Grim et al. (2004)** - "Making Meaning Happen" - 通信涌现需要信号间接帮助生存
2. **Rawal & Miikkulainen (2012)** - "Evolution of a Communication Code in Cooperative Tasks" - 合作任务中通信代码涌现
3. **Rawal, Rajagopalan, Miikkulainen (2014)** - "Evolution of Communication in Mate Selection" - 直接感知目标会阻止通信涌现
4. **Nolfi & Floreano (1998)** - "Coevolving Predator and Prey Robots: Do Arms Races Arise?" - 共进化军备竞赛条件
5. **Beer (2003)** - "The Dynamics of Active Categorical Perception in an Evolved CTRNN" - CTRNN 复杂行为所需最小网络
6. **Stanley & Miikkulainen (2002)** - NEAT 原论文 - 从简单网络逐步复杂化
7. **Risi & Stanley (2010)** - "Behavior-based Speciation" - 基于行为的物种分化
8. **Odling-Smee et al. (2003)** - "Niche Construction" - 生态位构建理论（beaver dam 类比）
9. **Nextstrain (Hadfield et al. 2018)** - 病原体进化实时追踪可视化
10. **Lehman & Stanley (2011)** - "Abandoning Objectives: Novelty Search" - 新颖性搜索避免局部最优

---

## 八、预期效果

完成后，用户应该能看到：

1. **通信涌现**：MI ≥ 0.3，信号 channel 0 = "食物在这"，channel 1 = "危险在这"
2. **病原体军备竞赛**：毒力代际上升 → 宿主免疫上升 → 毒力再上升，连续可追踪
3. **物种行为分化**：物种 A 住洞穴、夜间活跃；物种 B 住平原、白天觅食；物种 C 是纯捕食者
4. **机制联动**：洞穴庇护 → agent 挖洞 → 洞穴感染率低 → 病原体演化出新毒株突破
5. **工具使用**：agent 搬石头建巢穴入口 → 巢穴内安全 → 合作搬运大石头
6. **所有变化连续可追踪**：不是"开头和结尾的数字"，而是"每代/每 tick 的变化曲线"
