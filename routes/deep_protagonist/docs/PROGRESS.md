# deep_protagonist · PROGRESS

> 最后更新：2026-05-26 中（D-044 跨项目止血）
> 当前阶段：**S4.1 D-043 ppo_d043_2M 在 1.06M 步停了（last 20 mean -20.84，撩狼回归），等下一轮决策**
> Self-test：**112 个全过**（D-043 加 4 个 + 1 个删 first_hunt）
>
> ⚠️ **未来 AI 必读**：用户决策 D-038 —— HUD 推迟。具体待办清单见 `docs/待办_视觉化与打磨.md`
> Wander baseline (D-043 后): mean +14 / score 0.072 / 死亡率 ~10%
> PPO run3 (GRU+SpatMem, 1M, Bernoulli): mean +18.6 / 死亡率 0%
> PPO run5/6 (multinomial 调参): 卡 plateau
> PPO run7 (D-041, 2M): last_20 -80（撩狼塌缩，100×milestone 但反 robust）
> PPO d043_2M (D-043, 1.06M 步停): last_20 mean -20.84 / score 0.127 / r_bites -37.6（撩狼回归 vs 早期 ep 15-19 cherry-pick noise）/ 死亡率 35% / r_milestone +183.7 高 / r_potential +83.3 高（agent 持有家衣物）

## ✅ 最新会话完成（2026-05-26 中）：D-044 跨项目止血

用户报"之前出过 190G 巨型日志，要避免再出现"。先排查 **deep_protagonist 本身日志逻辑健康**（spdlog 全 stdout / jsonl 每 ep ~400B，最大文件 ppo_run6 98K）；爆量源是 **protagonist_ecology**：`data/runs/_credit_log.csv` 2.1GB + `tmp/*.log` 5 个 856MB，CreditLedger.cpp 注释自爆 *"2026-05-23 incident hit 7.07 GB / 110M rows"*。

3 道护栏 + 全删旧文件（详见 `决策记录.md` D-044）：

| 改动 | 文件 |
|---|---|
| `evaluatePopulation` 签名 bool→path; 3 个 hardcode CSV 路径迁到 `<run_dir>/...` | `routes/protagonist_ecology/runtime/ProtagonistEcologyRuntime.cpp` |
| `CreditLedger::appendCsv` 加 200MB cap | `routes/protagonist_ecology/runtime/CreditLedger.cpp` |
| `CommunicationMetrics::appendCsv` 加 200MB cap | `routes/protagonist_ecology/runtime/CommunicationMetrics.cpp` |
| `_signal_debug.csv` inline 200MB cap | `routes/protagonist_ecology/runtime/ProtagonistEcologyRuntime.cpp` |
| 顶部巡检条目 | `routes/protagonist_ecology/docx/STATUS.md` |

`neural_eco_route_protagonist` 编译干净；`test_core --gtest_filter=*CreditLedger*:*Communication*` **20/20 PASS**。腾盘 ~3GB。

**deep_protagonist 主线没动**——下一步仍是 D-043 plateau 后的决策（继续训 / D-044-onward 撩狼根因二次诊断 / 你直接说）。

## ✅ 最新会话完成（2026-05-24 19:00-20:00）：D-043 reward 哲学重设计

用户原话："你先去查查别人是怎么设计奖励的吧 / 我指的是网上的论文"。

AI 用 tavily-mcp 搜索 + 精读 4 篇论文：
1. **Crafter (Hafner ICLR 2022)**: 项目对应物，+1/achievement, score 几何均值
2. **PBRS (Ng/Harada/Russell ICML 1999)**: 状态势函数数学定理（持有 Φ>0 时 F 反为负）
3. **Achievement Distillation (Moon NeurIPS 2023)**: Crafter SOTA, 改架构非改 reward
4. **Homeostatic RL (Keramati 2014)**: drive_reduction 公式 + 反向抑制

**关键洞察**：你 first_X +20 vs Crafter +1 = **20× 偏离学术界**。撩狼是 specification gaming 教科书例子。

### D-043 实施清单（3 个 commit）

| 步 | 内容 | 状态 | Commit |
|---|---|---|---|
| C1 P0 | first_X +20→+1, 死亡 -100→-10, wolf_killed +5→+1, 删 first_hunt | ✅ | 45f80d3 |
| C1 P0 | jsonl 加 unlocks[] + score (几何均值) 字段 | ✅ | 45f80d3 |
| C2 P1 | event_eat/drink 改 drive_reduction, 加 crisis_scale, 加 reward clip | ✅ | 4535912 |
| C2 P1 | main.cpp viewer 同步 drive 公式 (drive_v + eat_drive lambda) | ✅ | 4535912 |
| C3 P2 | state_bonus_coef × Φ(s), Φ = house+clothes+followers+inventory_diversity | ✅ | 65ec123 |
| Train | 2M PPO 训练（进行中）| 🔄 | — |

### 早期成果（前 20 ep / 18 分钟）

- **score** 从 wander 0.072 → PPO 0.189-0.223（**3× 提升**）
- ep 15-19 **连续 5 episode 无死亡**，bites=0（避狼策略学到）
- potential 累积 +0.4 → +27.7（agent 保持物品 + 偶建房）
- milestone 累积 +2 → +47（每 ep 解锁 2-3 个 first_X）
- 死亡率前 15 ep ~33% → 后 5 ep 0%（学习曲线明显）

### 4 篇论文笔记

`docs/论文精读_奖励设计.md` 338 行整理：
- Crafter 17 动作 + 22 achievements 设计（"Do" 上下文动作 + 没有 hunt_wolf）
- PBRS F=γΦ'-Φ 数学证明 + 与"持有奖励"反效果分析
- Achievement Distillation contrastive next-achievement loss（远期方向）
- Homeostatic drive 函数 + 反向 drive 抑制完整公式

## ✅ 上轮会话完成：S4.1 D-039 multinomial 架构修复 + 4 轮迭代 + 2M 训练

用户原话"规划下一步"。AI 没有盲投扩参，先做 30 分钟 **C 行为诊断** 抓 ppo_run3 5 局 act 直方图，发现"16 个 sigmoid 全 0.5"病——根因是架构问题不是参数不够。

### S4.1 实施清单

| 步 | 内容 | 状态 |
|---|---|---|
| 1 | 加 main_train.cpp ActionStats（16 离散动作触发计数 + mean \|move\| / \|yaw\|）；写入 jsonl | ✅ |
| 2 | scripts/analyze_actions.py：分析 act 直方图 + NOOP%（支持 last_n） | ✅ |
| 3 | PPO 离散头 16 Bernoulli → 17 Categorical（softmax + multinomial.sample）| ✅ |
| 4 | Transition.disc_act vector<float>[16] → int；update 用 log_softmax.gather + categorical entropy | ✅ |
| 5 | 4 轮 100k smoke 校准 ent_coef + trigger_cost：v1 0.001/无cost → wear 65% / v2 0.005/无cost → craft_spear 19% / v3 0.005/cost 0.005 → NOOP 98% / **v4 0.005/cost 0.001 → 16 动作 2.55-9.78% 平均** ✅ | ✅ |
| 6 | RewardSystem.event_action_cost：每按一个 trigger 扣 0.001（计入 vital） | ✅ |
| 7 | 2M 步训练 ppo_run5_multi_cost（进行中） | 🔄 |

### v4 早期成果（200k 步 / ep22）

- ep 12: R=**+49.3** 含 milestone +40（解锁阶段，ppo_run3 1M 没出现过）
- ep 5/6/7 shelter +10 / +10 / +5
- 死亡率 1/22 ≈ **4.5%**（vs ppo_run3 0% / wander 29%）— 略高但可接受
- ent 4.7 维持，act drink 25.7% (top 1)，wear_clothes 0.82%（消亡）

### 700k 步 / ep83 阶段（plateau 期）

- mean reward -6.72（被 7 死拖累），中位数 +11.30
- r_water +1.36 / r_food +0.96 / r_collect +2.74（健康）
- r_shelter / r_craft / r_milestone 全 0（plateau，没再触发新建造）
- 死亡率 8.4%

### 关键洞察（D-039）

**multinomial 不是免费午餐**——任何"廉价 noop"（如 wear_clothes 没衣服时静默）都会被找出来当默认。修复：
- categorical 互斥选 1 个动作（vs sigmoid 同时按多个）
- ent_coef 0.005（而非 0.001 过激或 0.01 太散）
- **trigger cost 0.001/step**（按了无效就扣钱，按了有效仍 net 正），堵廉价 noop 漏洞

## ✅ 之前会话完成：S4.0 记忆架构（GRU + SpatialMemory）+ 第一次 1M 训练对比

用户问"并行项目用 DNC 我们要不要用"。AI 查文献（Memory Gym ICLR'24, RLBenchNet'25, Parisotto-Salakhutdinov "Neural Map" 2017, MERLIN）+ 对比 protagonist_ecology 场景，得出 5 条证据：DNC 训练困难、PPO 无主流先例、需要绝对坐标、甜点是图谱推理不是实时控制、场景需求不同。决策 D-037：**用 GRU + SpatialMemory（Neural Map 路线），不用 DNC**。

### S4.0 完成清单

| 步 | 内容 | 状态 |
|---|---|---|
| 1 | obs P0 补料（last_action 19 + river 3 + biome 6 + building_type 4 + blueprint_left 3 = +35维） | ✅ |
| 2 | SpatialMemory 模块 (64×64 grid @ 16m/cell × 6 features) + 8 self-tests | ✅ |
| 3 | SpatialMemory 接入 obs（8×8 local window × 6 = +384维），obs 145 → 564 | ✅ |
| 4 | PPO 网络改造：Linear(564) → ReLU → GRUCell(256) → 3 heads（参数 108k → 544k） | ✅ |
| 5 | Rollout buffer 加 h_in 缓存（stale-hidden 方案，CleanRL 风格） | ✅ |
| 6 | 1M 步训练 ppo_run3_gru_sm 跑完，对比 ppo_run2 | ✅ |

### 1M 步训练结果（last 20 ep mean，详见 docs/BASELINE.md）

| 指标 | run2 MLP | run3 GRU+SpatMem | 变化 |
|---|---:|---:|---:|
| 总 reward | +16.29 | **+18.57** | +14% |
| r_water | 0.00 | **+1.79** | **首次稳定喝水** |
| r_bites | -26.46 | **-2.17** | **避狼 +92%** |
| r_collect | +1.70 | +2.85 | +68% |
| r_kills | 0.00 | +1.25 | 主动战斗 |

**质变**：PPO 学会了"渴了往河边走"——run2 1M 步都没学到，run3 在 ep80+ 后每局都能稳定喝水。避狼能力大幅提升（spatial map 标记 danger cell + GRU 维持意图）。

**未达预期**：还在 🟨（mean >0）级别，没突破到 🟧（mean >+50）。1M 步对 GRU 来说不够，milestone 解锁少，shelter 没盖。需要 5-10M 步长训。

## ✅ 之前会话完成：S4-pre 训练前置补完

## ✅ 最新会话完成：S4-pre 训练前置 5 步

用户原话："可以开始训练了吗，训练环境和指标都搞定了吗"。AI 评估发现 4 个硬缺口（AgentAction 缺 9 动作 / 没 PPO 框架 / 没 headless / 没指标看板），决策做 S4-pre 5 步前置补完，再进 S4 主体长训。

### S4-pre 完成清单

| 步 | 内容 | 状态 |
|---|---|---|
| 1 | AgentAction 扩到 19 通道（3 cont + 16 disc）；main.cpp 全部走 action 路径 | ✅ |
| 2 | 新建 `dp::env::Environment` headless 包装；新建 `deep_protagonist_train.exe`；50k steps/s | ✅ |
| 3 | MetricsLogger 写 JSONL（11 reward 分量 + episode 信息）；Python 统计脚本 | ✅ |
| 4 | Wander baseline 100 局；写 `docs/BASELINE.md` 评判口径锚点 | ✅ |
| 5 | PPO 骨架（MLP 145→256→256 + actor/critic head + GAE-λ + clip update + Adam）；CUDA smoke 10000 步通过 | ✅ |

### 新增模块

- `include/agent/AgentAction.hpp`：扩展为 19 通道
- `include/env/Environment.hpp` + `src/env/Environment.cpp`：headless 环境包装
- `include/policy/PPO.hpp` + `src/policy/PPO.cpp`：完整 PPO 实现（108k 参数）
- `src/main_train.cpp`：headless trainer 入口
- `scripts/summarize_metrics.py`：JSONL 统计工具
- `docs/BASELINE.md`：训练成功 / 失败的判定基线

### 二进制
- `build/bin/deep_protagonist.exe` - raylib viewer + self-test
- `build/bin/deep_protagonist_train.exe` - headless trainer

### CLI
```
deep_protagonist_train.exe --policy {wander|noop|ppo}
                           --steps N --episodes N --seed S
                           --metrics path.jsonl
```

### 现在能做什么
1. 跑 PPO smoke：`--policy ppo --steps 10000`（10 秒）
2. 跑 wander baseline：`--policy wander --steps 1000000 --episodes 100 --metrics runs/wander_baseline.jsonl`（17 秒）
3. 统计：`python3 scripts/summarize_metrics.py runs/wander_baseline.jsonl`
4. **下一步 S4 主体**：长训 1M-10M 步，看 PPO 是否打败 wander median +15 → +50 → +100

## ✅ 之前会话完成：S3.10 完整生存模拟（蓝图 v2 升级）

用户原话："让它行为遵循一个主线，前期采草造衣，中期建房子（真实建造材料堆到那系统自动建好 3D，多种类型按需选择），后期农耕饲养驯服"。

AI 上网查了 Don't Starve / Minecraft / Construction Simulator / Berkeley BAIR 课程学习论文之后，写 D-034 升级蓝图 v2（废除原 line 77 "❌ 农业/衣物" 限制），做了完整 S3.10 主线。

### S3.10 完成清单

| 子阶段 | 内容 | 状态 |
|---|---|---|
| **S3.10a** | ProgressTracker + 真实建造 ConstructionSite（4 房型 + 图纸 deposit 系统）| ✅ |
| **S3.10b** | Wardrobe + 草裙（3 草）/ 兽皮护寢（2 兽皮）+ 冷 resist | ✅ |
| **S3.10c** | FarmingSystem + 种子掉落（30%）+ 种田 + 浇水 + 长大 + 收果 | ✅ |
| **S3.10d** | TameSystem + 喂食 + 信任度 + 跟随 | ✅ |
| **S3.10e** | Observation 124→145 维，含阶段/进度/建筑/衣着/农田/驯服 | ✅ |
| **WorldRenderer** | 画 ConstructionSite 白框 + 进度条；画 4 种房子；画 farm 长苗 | ✅ |
| **self-test** | 100 个全过（69 baseline + 31 新增）| ✅ |

### 新键位

| 键 | 功能 |
|---|---|
| **P** | 在脚下放下建筑图纸（中期解锁） |
| **Tab** | 切换要建的房型（Shed/Wood/Stone/Big） |
| **R** | 优先把背包材料放进附近图纸；没图纸时正常拾资源 |
| **X** | 合成草裙（3 草） |
| **Z** | 合成兽皮护寢（2 兽皮） |
| **V** | 穿上衣服（优先 cloak > dress） |
| **N** | 种子种地（后期解锁） |
| **J** | 浇水（后期解锁） |
| **K** | 喂食驯服（后期解锁） |

## ✅ 之前会话完成：S3.7 → S3.8 → S3.9 三轮机制层完善

用户睡前指示："美化不重要，先把世界机制完善好让训练能出效果"。AI 重读最终蓝图发现 S3.5 视觉整改其实违反 line 79（"❌ 真实美术资源，留给 S6"），于是推翻 S3.5，连续做了三轮机制完善。

### S3.7 完成清单（按依赖顺序）

| 子阶段 | 内容 | 状态 |
|---|---|---|
| **S3.7a** | `AgentAction` 接口（PPO 输入定义；wander 改成 action-driven）| ✅ |
| **S3.7b** | `RewardSystem` v1（10 个奖励组件，按"加新任务保留旧奖励"）| ✅ |
| **S3.7c** | `EpisodeManager`（max_seconds=300、死/超时 reset；地形保留）| ✅ |
| **S3.7d** | `ObservationBuilder`（124 维 PPO 输入向量，全部归一化）| ✅ |
| **S3.7e** | `ResourceSystem`（干柴 600 / 石 400 / 长草 800，按 biome 分布）| ✅ |
| **S3.7f** | `Inventory`（3 格背包，木+石→长矛 craft）| ✅ |
| **S3.7g** | `Shelter`（1W+1G 建造，3m 半径覆盖体温保护）| ✅ |
| **S3.7h** | Wolf 夜行加强（perception +50%, speed +20% × night_factor）| ✅ |
| **集成** | main.cpp 接入 R/B/C 键、HUD 显示 inventory + shelter + episode + reward | ✅ |
| **可视化** | WorldRenderer 画出 wood/stone/grass 几何体 + shelter A-frame | ✅ |
| **self-test** | 64 个测试全过（原 32 + 新增 32）| ✅ |

### S3.8 完成清单（蓝图 line 16-25 覆盖）

| 子阶段 | 内容 | 状态 |
|---|---|---|
| **S3.8a** | `AnimalSystem::agent_attack`（F 键，spear 加 reach+damage）| ✅ |
| **S3.8b** | Wolf 狩猎 rabbit/deer（tick_wolf 扫描 prey + 1.5m kill）| ✅ |
| **S3.8c** | Shelter 防狼（在窝内时 wolf perception×0.3, speed×0.7）| ✅ |
| **S3.8d** | 抓鱼（Q 键在 river/swamp 时一并触发 2m 攻击）| ✅ |
| **S3.8e** | Mushroom 中毒 30%（扣 hunger 12 + stamina 25 + 负 reward）| ✅ |

### S3.9 完成清单（蓝图 line 20 地形适应）

| 子阶段 | 内容 | 状态 |
|---|---|---|
| **S3.9a** | Swamp 减速（CapsuleAgent move_speed × 0.5）| ✅ |
| **S3.9b** | Desert 加速失水（sun_h>0.4 时 thirst × 2）| ✅ |

### 阶段路线

| 阶段 | 内容 | 状态 |
|---|---|---|
| **S0** | 项目骨架 + libtorch + CUDA 跑通 | ✅ |
| **S1** | 3D 世界基础（高度图 + 洞穴 + 物理） | ✅ |
| **S2** | 1024m 大世界 + 6 biome + 河流 + 昼夜 | ✅ |
| **S3** | 动物 + 植被 + agent 生命系统 | ✅ |
| **S3.7** | 训练前置机制（action/reward/episode/obs/资源/背包/庇护/夜狼）| ✅ |
| **S3.8** | 蓝图战斗/狩猎/抓鱼/中毒/防狼覆盖 | ✅ |
| **S3.9** | 地形适应（沼泽减速 + 沙漠加速失水）| ✅ |
| **S3.10** | 完整生存模拟主线（进度/建造/衣服/农耕/驯服）| ✅ |
| **S4-pre** | **训练前置补完**（19-action 接口/headless/PPO 骨架/baseline）| ✅ 本次完成 |
| **S4** | PPO 长训 + 课程学习 + 5-10M 模型扩容 | ⬜ **下一阶段** |
| **S5** | 生火 + 扩 50M + 长训（8-16 周）| ⬜ |
| **S6** | 视觉美化 + 用户干预 + 答辩预演 | ⬜ |

## 🔴 下次会话第一件事：进入 S4（PPO + 课程学习）

S4 = 用 libtorch 实现 PPO，让 agent 自己学会三阶段主线：早期觅食/躲狼 → 中期建造 → 后期农耕/驯服。

### S4 准备就绪清单（已具备）
- ✅ AgentAction：动作格式（神经网络输出格式）
- ✅ RewardSystem：标量 reward 信号（含 +50 阶段解锁稀疏奖励）
- ✅ EpisodeManager：一局边界（advantage 计算需要）
- ✅ ObservationBuilder：**145 维** 输入向量
- ✅ ProgressTracker：课程学习的"通关"信号
- ✅ Wander baseline：可对比"网络学到了 vs 随机走"
- ✅ 所有 S3.10 系统集成进 main loop，可观测训练效果

### S4 第一步要做的事
1. **设计网络架构**：MLP + GRU/LSTM + actor-critic head（输入 145 → 隐 **512/256** → 动作分布 + value，扩大因 action 多）
2. **实现 PPO loop**：rollout collect → advantage compute → policy/value update
3. **跑通最简 smoke test**：1 个环境跑 10000 步，看 reward 是否上升
4. **接 28 并行环境**：CPU 多线程 rollout，GPU forward+backward
5. **接 checkpoint + resume + EWC 防遗忘**（学新阶段不忘旧）

预估：开发 1-2 天 AI 时间。训练时长 **8-16 周**（多了 30+ 动作要学，蓝图 D-034 已说明）。

## 最近会话摘要

### 2026-05-24 凌晨 03:45 · S3.9 地形适应
- **swamp 减速**：CapsuleAgent.apply_action 在 swamp biome 时 move_speed × 0.5
- **desert 白天加速失水**：VitalSystem.tick 在 desert + sun_h>0.4 时 thirst 损耗 × 2
- **69 self-test 全过**
- 遗留：默认 World seed 下 mountain 占 60%+ 挤掉了 desert，BiomeMap 阈值调整推迟

### 2026-05-24 凌晨 03:35 · S3.8 蓝图覆盖度补全
- 5 个机制让蓝图 line 16-25 都落地
  - **8a** Agent attack（F 键，spear 加 reach+damage）
  - **8b** Wolf 狩猎 rabbit/deer（生态链）
  - **8c** Shelter 防狼（窝内 perception×0.3, speed×0.7）
  - **8d** 抓鱼（Q 键 in river/swamp 同时抓 fish）
  - **8e** Mushroom 中毒 30%
- **67 self-test 全过**（原 64 + 新 3）

### 2026-05-24 凌晨 03:30 · S3.7 训练前置机制
- 用户睡前指示："美化不重要，机制完善优先"
- AI 重读最终蓝图，发现 S3.5 视觉整改违反 line 79 蓝图条款，**推翻**
- 改做 S3.7：8 个机制 + 集成 + 可视化 + self-test 扩展
- **代码量**：约 1100 行新增（include 7 hpp + src 7 cpp + main.cpp 集成）
- **64 个 self-test 全过** (a-h + Observation + Episode 系列)
- 新文件：
  - `include/agent/AgentAction.hpp` (43 行)
  - `include/agent/RewardSystem.hpp` (130 行) + `src/.../cpp` (74)
  - `include/agent/EpisodeManager.hpp` (71 行) + `src/.../cpp` (60)
  - `include/agent/Inventory.hpp` (64 行) + `src/.../cpp` (55)
  - `include/agent/Shelter.hpp` (42 行) + `src/.../cpp` (29)
  - `include/agent/Observation.hpp` (75 行) + `src/.../cpp` (223)
  - `include/world/ResourceSystem.hpp` (74 行) + `src/.../cpp` (127)
- **未来 AI 接力提示**：S4 的 PPO loop 直接消费 ObservationBuilder + AgentAction + RewardSystem + EpisodeManager 这 4 个对象，**机制已经完整**

### 2026-05-24 凌晨（之前会话）
- 立项 deep_protagonist 路线，与 protagonist_ecology 并行
- 写完 AGENTS.md 协作契约（极简版 55 行）
- 工具链确认（MSVC 14.44 + CMake/Ninja + CUDA 12.8）
- 资源预算：CPU 28 并行 / GPU 7GB / RAM 26GB
- **S0 完成**：
  - libtorch 2.11.0 + cu128 下载（2.65GB）+ 解压（5.3GB）
  - CMakeLists.txt + tomlplusplus + spdlog（FetchContent）
  - main.cpp smoke test 跑通（GPU 矩阵乘 40ms / TOML 读取 / 彩色日志）
  - 踩坑：中文 VS 路径 `E:\c语言` 让 vcvars64 挂掉，改用 C:\ 的 BuildTools
  - 踩坑：libtorch 下载中途断 5x，curl `-C -` 续传成功

## 下一步建议

S2 完成（1024m × 1024m 世界 + 6 biome + 3 河流 + 6 洞穴 + 4854 装饰物 + 昼夜循环）。
后台正在从 poly.pizza 下载 Quaternius CC0 资源（48 bundle，~3GB），不阻塞主线。

**进入 S3**：带生物的植被/动物系统：
- 植被：果树/浆果/蘑菇/仙人掌（可采集，按 biome 生长）
- 动物：兔/鹿/狼/鱼/鹰（带状态机 AI，狼追兔追 agent）
- 替换旧 FoodSystem（蓝球作废）
- agent 增加生命系统：饥饿/口渴/体力/体温

## 关键路径

- 项目根：`routes/deep_protagonist/`
- 协作契约：`AGENTS.md`
- 4 份速查：`docs/术语速查.md` / `参数速查.md` / `决策记录.md` / `Q&A速查.md`
- 编译入口：`scripts\configure.ps1` → `scripts\build.ps1`
- 主程序：`build\bin\deep_protagonist.exe`

## S0 验收记录

```
[01:27:13] libtorch version : 2.11.0
[01:27:13] CUDA available   : true / CUDNN: true / Devices: 1
[01:27:13] 100x [1024x1024] matmul on GPU: 40.40 ms (0.40 ms/iter)
[01:27:13] S0 SMOKE TEST PASSED
```

## S1 验收记录

```
[01:50:37] libtorch + CUDA 仍正常
[01:50:37] World: 256x256 cells @ 1m, max_height=30m, caves=3
[01:50:38] Window open. WASD+Mouse to fly, Ctrl=fast, Esc=quit
```

代码量：约 700 行 C++（包含 Heightmap / CaveSystem / Physics / CapsuleAgent / FlyCamera / WorldRenderer / main）

S1 实现内容：
- 程序化分形噪声地形（自实现 value noise + fBm，无第三方依赖）
- 3 个长方体洞穴（procedural generated）
- 胶囊物理：重力 + 地形碰撞 + 洞穴底/顶碰撞 + 摩擦/空阻
- "wander" agent：每 2 秒换一次方向，随便走（神经网络 S2 加）
- raylib 3D 地形 mesh（含按高度的颜色渐变）+ 半透明洞穴可视化
- First-person 飞行相机（WASD+鼠标）+ G 键切换 agent 跟随

S1 踩坑：
- MSVC 不支持 GCC 复合字面量 `(Vector3){...}`，全部改成 `Vector3{...}`
- GitHub TLS 偶发失败，改用顶层项目 `_deps` 已下载源码（FETCHCONTENT_SOURCE_DIR_*）
