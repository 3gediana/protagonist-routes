# 参数速查

> 项目里出现过的所有参数 → 它是什么、调大调小后果。

## 资源限制类

| 参数 | 当前值 | 含义 | 调大 | 调小 |
|---|---|---|---|---|
| `cpu.parallel_envs` | **28** | 同时模拟几个世界 | 训练快但 CPU 烫 | CPU 凉但训练慢 |
| `cpu.usage_target` | **80%** | CPU 利用率目标 | 系统卡 | 浪费机器 |
| `gpu.vram_budget_mb` | **5120** (5GB) | 训练能占多少显存 | batch 能开大点但风险高 | 安全但浪费 |
| `gpu.vram_max_mb` | **7168** (7GB) | 硬上限，超过就报警 | — | — |
| `ram.usage_target_gb` | **26** | RAM 用多少 | 系统卡 | 浪费 |

## 训练类（S4-pre 已启用，PPO 跑通）

| 参数 | 当前值 | 含义 | 调大 | 调小 |
|---|---|---|---|---|
| `PPOConfig.rollout_steps` | **2048** | 一次收集 N 步再更新 | 单次更新更稳，墙钟更长 | 更新更频繁，方差大 |
| `PPOConfig.epochs` | **4** | 一波 rollout 反复学几遍 | 学到位但可能过拟合 | 学不充分 |
| `PPOConfig.minibatch_size` | **256** | 每次梯度算多大 batch | GPU 占满更快，显存飙 | GPU 闲置 |
| `PPOConfig.lr` | **3e-4** | Adam 学习率 | 跑飞、训崩 | 训得慢 |
| `PPOConfig.gamma` | **0.99** | 未来奖励折现率 | 更有远见 | 更短视 |
| `PPOConfig.lambda_` | **0.95** | GAE 平滑 | 看远但噪声大 | 看近 |
| `PPOConfig.clip_eps` | **0.2** | PPO 一次能跨多大步 | 训不稳 | 学得慢 |
| `PPOConfig.vf_coef` | **0.5** | value loss 权重 | value 学得猛但 policy 受影响 | value 学不准 |
| `PPOConfig.ent_coef` | **0.005** | 探索奖励（D-039 v2：从原 0.01 → 0.001 过激塌缩 → 调回 0.005 中间值）| 探索更多但学习慢 | 早早收敛但可能塌缩 |
| **trigger_cost** (Env) | **0.001/step** | D-039 v4：每按一个离散动作（无论是否成功）扣 0.001（计入 r_vital）。封堵 "craft_spear/wear_clothes 没材料时免费 noop" 漏洞 | 0.005 时 agent 学到全 NOOP 98% | 0 时 agent 一直按免费 noop 键 |
| **thirst_decay_per_min** | **8/min**（默认值，CLI 可覆盖）| D-040→D-041 演化：12 太快 (drink 锁死 32%)，4 太慢 (NOOP 52% / milestone 0)，8 折中 | 大 = drink 重要 | 小 = drink 不重要但 agent 可能乱走 |
| **drive function** | **drive(v) = ((100-v)/100)²** | D-043：取代 satiation_scale。drive(100)=0 → 喝饱了 reward=0，drive(0)=1 → 渴死前 reward 最大。**reward = drive_before - drive_after**（一次 drink 最多 +1） | n=2 (quadratic) 更陡 → 临界时奖励大涨 | n=1 (linear) 更平滑 |
| **food_coef / water_coef** | **6.0**（D-048 蓝图回归）| D-043：eat/drink reward = coef × drive_reduction。3 时 drink 信号被 alive bonus 淹没；6 让从 0→100 总 reward 约 +6（Crafter 量级 ×2）| 大 = 吃喝主导 | 小 = 吃喝没驱动力 |
| **collected** (resource) | **0.5** | D-043 引入；每次 ResourceSystem 收集（草/木/石）+0.5 | 大 = 刷收集 | 小 = 收集无意义 |
| **deposited** (D-055) | **1.0** | D-055：每次 deposit 到 construction site +1.0（vs collect +0.5）。让 PPO 看到 deposit > collect 梯度，6 deposit = +6 一路到 building 完工 | 大 = 容易刷 deposit spam | 小 = PPO 看不到 deposit 比 collect 好 |
| **built_shelter** | **5.0** | D-043 不变；每次 lean-to shelter 或 building 完工 +5。注：building 完工还会 fire first_house milestone（+5 crisis 后 = +5 max） | 大 = 长期建设主导 | 小 = 长 horizon 建造看不到 reward |
| **crisis_scale** | **max_drive>0.7→0.3 / >0.4→0.7 / else→1.0** | D-043 P1：饥渴时 milestone 降权（先求生再发展）。Keramati 2014 "反向 drive 抑制"理论 | 阈值低 = 容易危机 = milestone 难拿 | 阈值高 = 几乎不抑制 |
| **first_X milestone** | **10 个 ×1 = 单局上限 +10**（D-043 改自 +220）| D-043 C1：first_hunt 删除，剩 10 个；每个 +1（Crafter 标准） | 大 = milestone 主导，可能引发刷分行为 | 小 = milestone 学不到 |
| **reward_clip_min / max** | **[-10, +10]** | D-043 P1：per-step total clip。-10 保留死亡完整，+10 覆盖 shelter+craft+milestone 累积。防 PPO advantage 估计被 outlier 毁 | 严 = 信号被截断 | 宽 = outlier 影响大 |
| **state_bonus_coef (Φ 系数)** | **0.005 × Φ/step** | D-043 P2：持有 has_house/clothes/followers/inventory_diversity 的持续奖励。Φ_max≈1.5，每 step max +0.0075，9000 step max +67 | 大 = "有家"主导 | 小 = 没"发展感" |
| `PPOConfig.max_grad_norm` | **0.5** | 梯度裁剪 | 大梯度乱来 | 太死 |
| `PPOConfig.use_cuda` | **true** | 用 GPU | — | CPU 慢 |

## 模型类（S4.0 实装：GRU + SpatialMemory）

| 参数 | 当前值 | 含义 |
|---|---|---|
| `OBS_DIM` | **564** | 观察向量维度（S3.10 基础 145 + P0 补料 35 + SpatialMemory 384）|
| `CONT_DIM` | **3** | 连续动作通道（move_x, move_y, yaw_rate）|
| `DISC_DIM` | **16** | 离散动作通道（eat/drink/.../feed_tame）|
| `DISC_CATS` | **17** | D-039 multinomial 头宽度 = 16 trigger + 1 NOOP |
| `NOOP_IDX` | **16** | NOOP（这步什么都不做）的 category index |
| `HIDDEN` | **256** | 主干宽度（encoder + GRU + heads 都用 256）|
| 总参数 | **544,792** | D-039 multinomial 后（每个 categorical 输出多 1 行：(256+1)×1=257，从 544,535 涨到 544,792）|
| `log_std` 初值 | **log(0.5) ≈ -0.69** | 连续动作初始噪声 |
| 主干结构 | Linear(564→256) + ReLU → GRUCell(256→256) → 3 heads | encoder + 记忆 + actor/critic |
| 离散头采样 | **Categorical 17 路** | D-039 改：每步 softmax + multinomial 选 1 个动作（之前是 16 个 Bernoulli 独立按）|

## SpatialMemory 类（S4.0 实装）

| 参数 | 当前值 | 含义 |
|---|---|---|
| `GRID` | **64** | 网格边长（64×64=4096 cells）|
| `CELL_SIZE_M` | **16** | 每格大小，米 |
| 覆盖范围 | **1024×1024 米** | 64×16，正好整个世界 |
| `FEATURES` | **6** | 每格存 visited/water/food/resource/danger/building 6 个 0/1 |
| `WINDOW` | **8** | obs 读 agent 周围 8×8 格 |
| `WINDOW_DIM` | **384** | 8×8×6，加到 obs 末尾 |
| 写入半径 | **VISION_RANGE = 60m** | 步进时只标记 60m 内看见的东西 |
| 重置 | **每 episode** | clear() at reset, 每条命独立重新探索 |

## Environment 类（S4-pre 实装）

| 参数 | 当前值 | 含义 | 调大 | 调小 |
|---|---|---|---|---|
| `EnvConfig.max_episode_seconds` | **300** | 一局最长 5 分钟 | episode 长，rollout 内 episode 数少 | episode 短但更频繁的 reset 信号 |
| `EnvConfig.fixed_dt` | **1/30s ≈ 0.033** | headless 一 tick 多长 | 更快但物理变粗糙 | 更细致但更慢 |
| `EnvConfig.instance_seed_xor` | 0xA9E1 | 给 agent 用的 RNG seed 偏移 | — | — |

## 存档类

| 参数 | 当前值 | 含义 |
|---|---|---|
| `checkpoint.interval_minutes` | 5 | 多久存一次 |
| `checkpoint.interval_steps` | 100000 | 多少步存一次（取早） |
| `checkpoint.keep_recent` | 10 | 保留最近几份 |
| `checkpoint.keep_best` | 3 | 保留历史最好几份 |
| `checkpoint.save_on_signal` | true | Ctrl+C 时强制存盘 |

## 环境类（S1 已启用）

| 参数 | 当前值 | 含义 | 调大 | 调小 |
|---|---|---|---|---|
| `world.heightmap_cells_x/y` | 256 | 高度图分辨率（一个小块多大） | 地形更精细，渲染慢 | 粗糙但快 |
| `world.cell_scale` | 1.0 | 一个小块代表多少米 | 更大世界 | 更小世界 |
| `world.max_height` | 30.0 | 最高山多高（米） | 山更陡、更高 | 平地 |
| `world.terrain_frequency` | 0.005 | 噪声频率（地形特征大小） | 地形更碎更乱 | 地形更平滑 |
| `world.terrain_octaves` | 5 | 噪声层数 | 更多细节小坑 | 更光滑 |
| `world.terrain_persistence` | 0.5 | 细节衰减 | 细节越多越粗糙 | 越平滑 |
| `world.cave_count` | 3 | 洞穴数量 | 更多洞 | 更少洞 |
| `world.seed` | 12345 | 世界生成种子 | 换 seed 换地图 | — |

## 食物类（S2.1 已启用，硬编码暂未配置化）

| 参数 | 当前值 | 含义 |
|---|---|---|
| `food.count` | 30（hardcoded） | 同时存在的食物数（吃完会重生） |
| `food.eat_radius` | 0.6m | 撞到多近算吃 |
| `food.vision_range` | 64m | 感官归一化距离 |
| `food.closest_k` | 5 | 感官里"看到最近几个食物" |

## RewardSystem（S3.7b 启用）

| 参数 | 默认值 | 调大 | 调小 |
|---|---|---|---|
| `reward.alive_step` | +0.05/秒 | agent 更倾向"什么也不做就活着"（可能学懒） | "活下去"奖励小 → agent 不重视长生存 |
| `reward.food_coef` | 3.0 × drive_red | 吃东西更值钱 → agent 暴食 | 不爱吃东西，容易饿死（D-043：改自 per_food_kcal）|
| `reward.water_coef` | 3.0 × drive_red | 喝水更值钱 → 一直泡河里 | 不爱喝，容易渴死（D-043：改自 per_water_ml）|
| `reward.wolf_killed` | +5 | 鼓励主动打狼 | 装死流，不会还手 |
| `reward.wolf_bite_dmg` | -1/dmg pt | 太怕狼，绕远路 | 不怕狼，硬冲 |
| `reward.death` | -10（D-043 改自 -100）| "宁可饿死也别死给狼"（学奇葩自杀策略） | 死多次也无所谓 |
| `reward.wolf_killed` | +1（D-043 改自 +5）| 偶尔会撩狼试试 | 完全不主动接近狼 |
| `reward.milestone` | +1（D-043 改自 +20）| 大 = milestone hunting | 小 = milestone 学不到 |
| `reward.state_bonus_coef` | 0.005 | 大 = 持有状态主导（房子奴隶）| 小 = 没"发展感" |
| `reward.vital_decay_coef` | 0.00005 | 状态扣得更狠，agent 更紧张地维护 | 状态可放任不管 |
| `reward.collected` | +0.5 | 鼓励无脑捡 | 懒得捡 |
| `reward.built_shelter` | +5 | 主动建窝 | 不爱建窝 |
| `reward.crafted_tool` | +3 | 主动造工具 | 不爱造工具 |

## EpisodeManager（S3.7c）

| 参数 | 默认值 | 调大 | 调小 |
|---|---|---|---|
| `episode.max_seconds` | 300（5 分钟） | 一局更长，agent 学到更长链条但训练慢 | 一局短，反馈快但学不到长期策略 |
| `episode.spawn_x/y` | 世界中心 | — | — |
| `episode.spawn_clearance` | 5m | 出生悬空 5 米后落到地面 | — |

## ResourceSystem（S3.7e）

| 参数 | 默认值 | 调大 | 调小 |
|---|---|---|---|
| `resource.wood_target_count` | 600 | 干柴更多，森林到处可捡 | 稀缺，需要走更远 |
| `resource.stone_target_count` | 400 | 石头更多 | 稀缺 |
| `resource.grass_target_count` | 800 | 长草遍地 | 稀缺 |
| `resource.wood_regrow_seconds` | 90s | 拾后再生快 → 资源不会枯竭 | 慢 → 必须探索新区 |
| `resource.stone_regrow_seconds` | 180s | — | — |
| `resource.grass_regrow_seconds` | 30s | — | — |

## Inventory & Shelter（S3.7f, S3.7g）

| 参数 | 默认值 | 含义 |
|---|---|---|
| `inventory.capacity` | 3 | 背包格子数。蓝图固定 0-3 件 |
| `shelter.cost_wood` | 1 | 建窝消耗 |
| `shelter.cost_grass` | 1 | 建窝消耗 |
| `shelter.radius` | 3m | 站在窝几米内算"被庇护"（体温回归） |

## 动物夜行（S3.7h）

| 参数 | 默认值 | 含义 |
|---|---|---|
| `wolf.night_perception_boost` | +50% | 夜深时狼能看多远（白天 40m → 夜深 60m） |
| `wolf.night_speed_boost` | +20% | 夜深时狼追多快（白天 6.5 → 夜 7.8 m/s）|

## Observation（S3.7d → S3.10e，PPO 输入）

| 参数 | 当前值 | 含义 |
|---|---|---|
| `obs.SIZE` | **145** (S3.10 扩展) | 观察向量总维度（PPO 网络输入大小） |
| `obs.K_PLANTS` | 4 | 看到附近 4 棵成熟植物 |
| `obs.K_ANIMALS` | 4 | 看到附近 4 只活动物 |
| `obs.K_RESOURCES` | 3 | 看到附近 3 个资源 |
| `obs.TERRAIN_PATCH` | 5×5 | 脚下 5×5 格地形高度差（每格 10m）|
| `obs.VISION_RANGE` | 60m | 距离归一化基准 |
| `obs.S3.10新增` | +21 维 | progress 阶段 (3) + 进度 (3) + 最近 building (3) + 后期物品 (4) + 衣着 (3) + 最近 farm (4) + 跟随者 (1) |

## ProgressTracker（S3.10a 阶段解锁）

| 参数 | 默认值 | 调大 | 调小 |
|---|---|---|---|
| `mid_collect_threshold` | 30 资源 | 中期解锁更慢，前期更扎实 | 中期解锁更快，前期不够练习 |
| `mid_explore_threshold` | 500m | 鼓励更远探索 | 缩在原地也能解锁 |
| `late_house_threshold` | 1 房子 | 后期更迟解锁 | 一座房就转后期 |

## BuildingSystem（S3.10a 真实建造）

| 房型 | Wood | Stone | Grass | 覆盖半径 |
|---|---|---|---|---|
| Shed | 4 | 0 | 2 | 3.5m |
| WoodHouse | 8 | 2 | 4 | 4.5m |
| StoneHouse | 6 | 8 | 2 | 5.5m |
| BigHouse | 12 | 10 | 6 | 7.0m |

## Wardrobe / Clothing（S3.10b）

| 服饰 | 材料 | 抗冷 buff |
|---|---|---|
| GrassDress | 3 Grass | +10% (cold_resist 0.10) |
| FurCloak | 2 Fur | +30% (cold_resist 0.30) |

## FarmingSystem（S3.10c）

| 参数 | 默认值 | 调大 | 调小 |
|---|---|---|---|
| `growth_per_second` | 1/120 = 8.3‰/s | 成熟更快 | 更慢 |
| `water_drain_per_second` | 1/60 = 16.7‰/s | 水更快干 | 一次浇水持久 |
| `water_step` | 0.5 | 一次浇水多 | 必须勤浇 |

## TameSystem（S3.10d）

| 参数 | 默认值 | 含义 |
|---|---|---|
| `trust_step_grass` | +0.10 | rabbit/deer 喂草每次 |
| `trust_step_fur` | +0.15 | wolf 喂兽皮每次 |
| `follow_threshold` | 0.5 | trust 达此值变成 follower |
| `follow_speed` | 4.0 m/s | follower 跟随速度 |

## 天气参数（D-121，需 --weather 开启）
| 参数 | 默认 | 作用 | 调大 | 调小 |
|---|---|---|---|---|
| --weather | 关 | 总开关，开了才会下雨 | — | — |
| --weather-prob | 0.30 | 每分钟从晴转雨的概率 | 雨更频繁 | 更少下雨 |
| --weather-dur | 90s | 一场雨平均多长 | 雨下更久 | 变阵雨 |
| rain_fire_douse_rate | 6/s | 雨把火的燃料额外烧掉多快 | 火灭更快 | 雨浇不灭 |
| wet_cold_gain | 1.0 | 湿透时掉体温倍率峰值(冷速×(1+gain×湿度)) | 淋雨更危险 | 淋雨无所谓 |
| wet_gain_per_s | 0.30 | 淋雨变湿速度(~3s 湿透) | 一下就湿 | 难淋湿 |
| wet_dry_per_s | 0.10 | 躲雨后变干速度(~10s 干) | 干得快 | 一直湿 |
| --ppo-hunt-shaping | 0 | 陆地狩猎链引导奖励系数(PBRS,不改动作空间) | 更鼓励狩猎链 | 关闭引导 |

## D-122 新增（RUNG 1 烤肉/照明）
- **--allow-cook**：总开关。开 = 打开烤肉/照明世界机制 + 解锁网络里"烤肉"动作口（否则被屏蔽永远选不到）。关（默认）= 世界与 v7 字节一致。纯开关。
- **--cook-selftest**：跑一遍烤肉机制端到端自测后退出（验证用，不训练）。
- **COOK_FUEL_COST=15s**：每烤一次肉烧掉的火燃料（秒）。调大=烤肉更费火（要更勤喂火）；调小过小会让"火=免费"失去因果代价。
- **RAW_MEAT_CAP / COOKED_MEAT_CAP=5**：生肉/熟肉各自囤货上限。调大=囤更多口粮；调小=逼它现烤现吃。不占 3 格背包（独立计数器）。
- **熟肉回饿=45**（生鹿肉=40）、蛋白×1.3：熟肉比生肉更顶饿更补蛋白，是"打猎→生火→烤"绕路的回报。
- **obs 573/574/575**：附近有无可烤的火(0/1)、生肉存量比、熟肉存量比。582 维里 576..581 仍留 0 给后面 RUNG。

## D-122 新增（RUNG 2-5：挖矿/石器/建造升级/巨炉+Boss）
- **--allow-mine / --allow-craft-axe / --allow-craft-pickaxe / --allow-monument**：各关的世界机制总开关 + 解锁对应网络动作口；默认全关=与 v7 字节一致。
- **--mine-selftest / --tools-selftest / --tier-gate-selftest / --monument-selftest**：各关端到端自测后退出（验证用、不训练）。
- **MINE_REACH**：到矿点多近才挖得到。调大=好挖；调小=必须贴脸。
- **石镐省体力 -50% / +1矿，石斧 +1木**：石器的真实回报（绕路造工具的收益）。
- **tier_gate_enabled**：建造升级门。关=任何房子随便建(v7)；开=石屋需石镐、大屋需斧+镐。Shed/木屋永远免费。
- **MONUMENT_GOAL=10**：搬几趟矿建成巨炉。调大=工程更久(截止压力更大)。
- **MONUMENT_REACH=3m / MONUMENT_WARM_RADIUS=8m / MONUMENT_WARM_RATE=60温/分**：搬料距离 / 建成后取暖光环半径 / 光环回温速度。
- **BOSS_ARRIVAL_S=180s**：Boss 固定降临时间(每条命到点必来，跟建没建炉子无关)。调小=更早考验/留给建炉的时间更短。
- **BOSS_DURATION_S=60s / BOSS_COLD_DRAIN=60温/分**：寒潮持续时长 / 寒潮额外掉温速度。活过整场发终极里程碑(2×)。
- **obs 576-581**：留给 RUNG2-5 的科技感知槽(矿/工具/巨炉进度/boss 倒计时钟)。581=boss_threat：到点前 0→0.5 爬(催它建炉)、寒潮中 0.5→1。

## D-122 系统审计修复（cook_enabled 闸门内，v7 字节一致）
- **寒夜不回饥饿**（Environment.cpp L775 suppress_hunger_regen）：cook_enabled 且夜晚时，窝棚不再每分回 15 饥饿（仍回渴、仍挡 3x 夜寒）。作用=打破"躺着摘果子永生"、逼囤熟肉。关 cook→照常回饥饿（旧世界不变）。
- **vital_danger_coef=0.02**（RewardSystem）：凸的按指标溯源惩罚强度。每指标亏空((100-v)/100)^2 求和×0.02×dt 取负。单个指标垮到~10→约 -1/分；全部舒适→≈0。调大=对"放任某指标下滑"罚得更狠/更早预警，但太大会盖过正奖、压探索。只在 cook_enabled 触发。
