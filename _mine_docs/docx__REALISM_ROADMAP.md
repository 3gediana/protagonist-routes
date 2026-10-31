# protagonist_ecology 拟真路线图（REALISM ROADMAP）

> 永久北极星（2026-05-30 用户锁定）："**奔着真实去，把列出的缺陷都弥补**"。
> 拟真度（realism）= 项目最高永久优先级。累积训练为次要目标。
> 每阶段流程：smoke(2-4代) → short(10-20代) → long(60-80代) → harmony 7/7 不破 → commit。
> 本文件 = "已做的 + 未来要做的"单一可信清单，随 git 走，每阶段收尾必更。
> 最后更新：2026-05-30。

---

## 0. 一句话现状

我们在 **Phase B（环境真实压力）**。环境系统（季节/天气/昼夜/温度/疾病/树木枯竭再生）引擎层早就有且全开，但此前对 agent 是"装饰品"——agent 感知不到、且压力被人为调到近零。Phase A 已把环境感知接进主角（+10 维新血脉）；Phase B 正在把"真实生存压力"加回来，让 agent 不得不"看天行事"。后面还有 C/D/E/F 四大阶段 + A 的尺度放大 + dot-based 可视化。

---

## 1. 已做（DONE，按 commit 实证）

### Phase A —— 环境感知接线（已落地）
- **commit `652818d`**：把已有的 TimeOfDay/Season/Weather 三个感知模块接进主角（背景物种本来就用这套），NEAT 输入 **+10 维**。
  - 藏在 flag `protagonist_environment_perception_enabled` 后，默认关 → 不影响任何旧血脉。
  - 同时压缩季节/天气/昼夜时长，使其在单局 600 秒内真正轮换（否则感知到的是常量）。
  - 硬证据：拿旧血脉 checkpoint 续跑，开关一开就被"基因组形状不匹配"正确拒绝 → 证明 +10 维真生效、开了就是**新血脉**（旧存档不兼容）。
- **commit `1aae4ff`**：ON/OFF 对照（seed 50001，各 80 代都 7/7 PASS）→ **诚实负结果**：开不开环境感知，行为几乎无差异。昼夜节律是世界经食物分布间接带出的，不是"感知到时间"。
  - **根因**：世界没有"非用感知不可"的生存压力 → 进化没动力去用感知 → 引出 Phase B。

### Phase B —— 环境真实压力（进行中，已落地部分）
- **commit `b26a9a6`**：给 trace 加 **7 个环境真值列**（season / season_progress / time_of_day / light_level / weather / core_temperature / hunger）。纯可观测性，不改 reward/dynamics/维度。
  - 这是所有"环境响应分析"的 ground-truth 来源。此前 trace 里根本没有环境列，导致没法验证 agent 是否响应环境。
- **commit `e795bdf`**：把 5 个**写死的生存压力旋钮**提到 toml `[world.survival.*]`：
  - `cold_temp_threshold` `hot_temp_threshold` `thermal_damage_per_second` `hunger_damage_threshold` `hunger_damage_per_second`
  - 默认值 = 2026-05-24 调软后的"地板"值 → 零行为变化；改 toml 即可加压，**无需重编译**。
- **根因实锤（trace + 代码）**：压力此前近零。
  - trace 实测：整局 health 平均 **0.95**、最低 0.71，水分 0.97，**零冻死/饿死**。
  - 代码实锤：2026-05-24 故意调软（cold band 32/42→25/45、dmg 0.4→0.15、hunger thr 0.7→0.9、core_temp 钳在 [20,50]°C，冬天环境 5°C 也跌不破 20）。当初是为防 bootstrap 阶段全灭。
- **压力调参（Phase B #2b）找到 Goldilocks 档**：
  - dmg=0.5 太狠（pop 135→62 腰斩、有灭绝风险）。
  - **dmg=0.25 + cold_thr=30 = 正确档**：16 代短跑 C7 不灭绝 PASS（6/7，仅 C6=0.28 短跑未收敛差一点），gen15 health avg 0.72 / 20.5% 低血 / 79.7% 受冻 = **真差异化压力但不杀光**。
  - 涌现行为复核（16 代短跑）：C1 围猎/捕食者、C2+C4 捕食食物网、C3 协作、C5 建造、C7 物种不灭绝 **全 PASS** → 加冷压力**没有破坏原有生态**。

### 验证基础设施（已具备）
- `scripts/protagonist_harmony_check.py` → 7 项生态健康度（C1-C7）。
- `scripts/protagonist_trajectory_analyze.py` + `_compare.py` → 逐 tick 轨迹/目的性分析。
- trajectory 闭环结论：3/3 干净同 seed run gen79 全 PURPOSEFUL（不是无头苍蝇）。

---

## 2. 正在做（IN PROGRESS）

### Phase B #2c —— 80 代长跑验证（RUNNING）
- 配置：seed 50001，dmg=0.25，cold_thr=30，env-perc **ON**，runs_dir = `realism_pressure_long`。
- 现状：长跑进行中，avg_health ~0.66-0.69（冷压力在咬人，符合预期），有活跃篝火/工地，无脱水/灭绝。
- 跑完要做三件事：
  1. `harmony_check` 确认 7/7（尤其 C7 不灭绝、C1-C5 涌现行为在满代数下依旧稳）。
  2. 对比 gen0 ↔ gen79：进化有没有学出**避寒行为**（冷 tick 往篝火/巢聚集）。
  3. 用环境真值 trace 重做 **ON/OFF 环境感知对照**：真实压力下，env-perc 是否终于被选择性用上（ON > OFF = 因果成立 / 无差异 = 压力仍不足或感知仍没用上）。
- 决策门：
  - 7/7 + health 改善 + 避寒行为涌现 → Phase B 收官，进 Phase C。
  - C7 灭绝 → 回退压力（dmg→0.15）重 smoke。
  - 7/7 但零改善 → 排查（是否需降 core_temp 钳位下限 / 加大 dmg）。

---

## 3. 未来要做（TODO，按拟真路线图 A-F）

> 关键工程权衡：Phase A 已做的"环境感知接线" + Phase C 都会**改 NEAT 输入维度** → 旧血脉 checkpoint 不兼容（shape guard 实测会拒）。拟真 > 纯累积，所以富世界值得重开新血脉。`protagonistSensoryDim(config)` 必须 == `createProtagonist()` 里注册的感知数。

### A —— 尺度放大（半截待办）
- 已做：环境感知接线（见上）。
- 待办："放大"本身——世界尺寸（600×600 → 更大）+ 更多 agent + 更长单局/代数。
- 风险：单卡 8G 显存（RTX 5060 Laptop, 8151 MiB）+ 吞吐，需盯着别爆。
- 方法：逐档放大，每档 smoke 确认显存/吞吐/harmony 不破。

### B —— 环境真实压力（进行中，见 §2）
- 收官标准：长跑 7/7 + 避寒行为涌现 + ON/OFF 因果定论。
- 后续可选增量：冬季食物枯竭压力（需核 TreeLayer/食物再生是否支持季节性枯竭，不臆断）、暴风扣移动、夜间专属天敌。

### C —— 局部感知（待办，新血脉）
- 内容：真·视野（FOV）+ 遮挡（occlusion）+ 感知噪声。agent 不再"全知"，只看得到视野内。
- 现状：引擎支持度已初步确认（不用改引擎核心），改输入维 → 新血脉。
- 价值：拟真"智能感"增益最大的一刀。
- 验证：smoke→short→long，harmony 不破；对比有无 FOV 的行为差异。

### D —— 连续世代 + 血缘（待办）
- 内容：年龄 / 亲代抚育 / 亲缘识别 / 世代重叠，取代现在偏离散的代际进化。
- 现状：已有 in-world 繁殖（LifecycleLayer + 谱系），但更像离散代际。
- 验证：是否出现亲代护幼、亲缘选择等行为。

### E —— 高阶社会（待办）
- 内容：领地 / 等级 / 长期联盟 / 欺骗等社会博弈。
- 现状：有协作信号 + 协作度，但没有领地/等级/长期联盟。
- 验证：是否涌现稳定领地边界、支配等级、跨个体长期合作。

### F —— 持久世界史（待办）
- 内容：扩 `MainWorldPersistence`，把**世界实时状态**（agent 位置 / 工地进度 / 树 / 天气）也序列化存下，跑与跑之间世界真连续。
- 现状：checkpoint 只存"脑子"（NEAT 基因组 + 进化状态），世界实时状态每跑重建 = "脑子接力、世界重开"。
- 这是"接力累积"升级成"真·连续世界"的最后一块。

---

## 4. 可视化最终形态（dot-based，2026-05-30 用户拍板）

> 替掉旧"Godot 精细 3D 模型"设想。理由：本路线是**几百实体**的群体世界，精细 3D 会卡死；隔壁 deep_protagonist 是单主角才适合精细 3D。

- **地形/地图**：3D（起伏 + 季节天气视觉变化）。
- **物种**：每个 archetype 一种颜色的小圆点（apex_predator 红 / pack_hunter 橙 / rabbit 白 / 主角亮蓝 …）。
- **投掷物**：更小的点。
- 价值：对观察涌现行为（围猎 / 食物网流动 / 群体聚散）反而比精细 3D 更清楚 = 科学可视化。
- 时机：等拟真阶段推进到合适节点再做这个 dot-based viewer。

---

## 5. "还剩几个阶段"口径

现在在 **B**。从现在算 = **C / D / E / F 四大阶段 + A 的尺度放大半截 + dot-based viewer**。
C/D/E 都要改架构起新血脉，较慢；每阶段 smoke→short→long→harmony 不破才算数。方向和顺序清楚，一阶一阶啃。

---

## 6. 红线（做拟真改造时绝不碰）

- 不动 `routes/deep_protagonist/`（另一条 route，另一个 AI 负责）。
- 不为某个指标"给太直接的奖励"（Goodhart）。只调环境施加的压力（阈值/伤害率），不动 reward shaping / HRL goal 权重。
- 不假设旧 checkpoint 在新维度下能用（shape guard 会拒）。
- 改完一批必 commit（PROGRESS + STATUS + 本文件 + knowledge note 同步）。
