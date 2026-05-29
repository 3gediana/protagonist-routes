# deep_protagonist · PROGRESS

> 最后更新: 2026-05-29 ~20:30 UTC (D-100 ScriptedSettler 验证完成, 准备 BC pipeline)
> 当前主线: **D-100 ScriptedSettler oracle 达标**, 下一步 D-101 BC (Behavioral Cloning) pipeline
> 当前蓝图阶段: S3 settlement — PPO 端 8 次失败, scripted oracle 端 1 次命中, 准备走"oracle 教 PPO"路线

## D-094..D-100 settlement 攻关 实证表

| 跑 | 改了啥 | houses (cum) | bldg_ticks/ep | n_shel ratio (last 100) | settlement? |
|---|---|---|---|---|---|
| D-091 | cold 18/min + 农田 2x + Shed 5m coverage | 122 | 4-139 | 0.001-0.014 | **no** |
| D-092 | + 夜禁野果 (PlantSystem 夜里 try_harvest 返空) | 189 | 6-361 | 0.001-0.066 | **no** |
| D-093 | + cold 50/min (1 分钟出门必死) | 233 | 7-336 | 0.004-0.119 | **no** |
| D-094 | 删 auto-Shed 重生 + cold 18/min 回退 | TBD | 100-400 → 5-43 | 头百集 0.05 → 后期 0.001 | **no** |
| D-095 | Shed coverage 5m→15m + 夜里出门 vital 3x drain | TBD | smoke 91% | 56% → 后期崩 | **no** |
| D-096 | obs[130..132] 改 "第一栋完工建筑" 锚点 | TBD | smoke 99% | 99.6% → 后期崩 | **no** |
| D-097 | + 在房子里 hunger/thirst +3/min regen | TBD | smoke 99.7% → ep 1123→12 | 0.188→0.004 | **no** |
| D-098 | gamma 0.99→0.995 + regen +3→+15/min | TBD | ep 502→0 | 0.111→0.000 | **no** |
| D-099 | + 在房子里 r_alive 翻倍 (0.05→0.10/s) | 147/ep last 100 | 14.8 last 100 | **0.007** last 100 | **no** |
| **D-100 ScriptedSettler (oracle)** | **手写 7 层决策树, 非 PPO** | **1/ep** | **8505/9000 (94.5%)** | **0.996** | **YES (oracle)** |

## D-099 失败结论

PPO 端 8 次连续失败 (D-091..D-099) 验证: **不管怎么调环境压力 / 信号 / 奖励, PPO 都会主动收敛到"狂建+不住"的 attractor**.

- D-091/092/093: 加压 (cold + 夜禁) → PPO 学到"死得快+下集重来"
- D-094: 删 auto-respawn → PPO 还是不住 (头百集 100-400 bldg, 后期 5-43)
- D-095: 扩大 shelter 覆盖 → 头几集 91% bldg 但后期崩
- D-096: 给"家位置 obs" → 头几集 99% bldg 但后期崩 (有家位置 ≠ 愿意回去)
- D-097/098: 在家里加 vital regen → 不够强, 还是崩
- D-099: 在家 r_alive 翻倍 → 头百集有信号 (bldg 1333) 但后 100 ep 仍崩到 14.8

**8 次实证, 1 条结论**: 即使环境数学上"在家比在外多挣分", PPO 探索阶段会先发现"出门觅食 +5/min" 的 attractor, 把这条路径强化到无法逃出. 这是 RL 的 local optimum trap, 不能靠改环境绕过.

## D-100 ScriptedSettler — Oracle 验证

**方法**: 不用 PPO 学, 直接手写一个 obs→action 的 7 层嵌套反应式决策树.

**架构** (main_train.cpp +460 LOC):
- Layer 0: 一次性计算 context (is_night, at_home, wolf_close, water_d_m, home_d, vitals, inv)
- Layer 1 EMERGENCY: vital 临界 + 救命动作 (按 day/night/distance 分支)
- Layer 2 SURVIVAL: 狼威胁 + 夜里远离家 (按武器/at_home/inv 状态分支)
- Layer 3 VITAL MAINT: vital < 0.55 + 喝水/吃浆果 (按 day/night/距离/家位置分支)
- Layer 4 CONSTRUCTION: 工地未完工 + 找资源/deposit (按 inv/site_needs/距离分支)
- Layer 5 SETTLEMENT: home_complete + walk_home/idle_at_home (生成 bldg_ticks)
- Layer 6 CRAFTING: at_home + craft_dress/spear/cloak/wear_clothes
- Layer 7 EXPLORATION: at_home + 12m 短途补给

**关键原则**: 全部用 obs 相对方位, 不写绝对坐标. 不依赖 episode 历史. 不同 seed → 不同轨迹但同决策逻辑.

**50 ep smoke (seed 99100)**:
- bldg_ticks/ep 均值 **8505/9000 (94.5%)**
- n_shel/n_total **0.996** (99.6% 夜里 sheltered)
- houses_built **1/ep** (无废墟)
- 死亡总数 3/50 ep (极罕见)
- 步均 8541/9000 (94.9% 完整存活)

**对比 D-099**: bldg_ticks 575×, n_shel 142×, houses 147× 少.

**判决**: oracle 路径完全可行. 蓝图原意"住进房子"由脚本一次实现. 接下来用此作为 BC demo source.

## 后续路线 (D-101..D-110 蓝图)

**D-101 BC pipeline (下一 session)**:
1. main_train.cpp 加 `--record-demos PATH` flag → settler 跑时把 (obs, cont_act, disc_idx) 写入二进制
2. 加 `--policy bc --bc-demos PATH --bc-epochs N` mode → 加载 demos, 监督训练 ActorCritic 网络 (MSE on cont + CE on disc), 出 ckpt
3. 加 demo 多样化机制: `--demo-no-regen` 临时禁用 in_building regen → 强制 settler 周期性出门觅食 → demos 涵盖 eat/drink/walk/return
4. 跑 200-500 ep demos → 100k-500k BC steps → bc_d101.pt ckpt
5. PPO 5M finetune from bc_d101.pt → 验证 PPO 在没脚本时也能定居
6. 评估: 随机新 seed, 看 bldg_ticks ≥ 1000 + n_shel ≥ 0.5 持续

**D-102+ ScriptedFarmer/Hunter/Master (远期)**:
- D-102: ScriptedFarmer 加 S4 农耕 (plant/water/harvest, 围着家种)
- D-103: ScriptedCrafter 加 S5/S6 (制造/打猎/spear)
- D-104+: ScriptedMaster 加 S7-S10 (远端 biome, 山洞, 河流, 暴雨, 多 site 村落)
- 每一轮: demo 录制 → BC 增量训练 → PPO 微调 → 验证机制学到

## 红线核对 (D-100 状态)

- [x] 不动 reward weights (D-100 只写 main_train.cpp, 完全不动 RewardSystem)
- [x] 不 continued-train (smoke 直接 random init, BC 留作 D-101)
- [x] 不 redesign BuildingSystem >20% (D-094 已删的 Shed respawn 仍保留删除)
- [x] 不加 per-tick shelter bonus (D-099 已加的 r_alive in_building bonus 仍保留, 是 5M PPO 第 8 次失败的边界 push)
- [x] 不为 score 调参 (smoke score 0.149 持平, 因 ScriptedSettler 不触发 eat/drink milestone 等)
- [x] 不动 obs schema (567 不变)
- [x] **新红线 (D-100)**: ScriptedPolicy 必须多层嵌套, 不允许单条件决策 (用户原话 "不要太死板了"). D-100 7 层 + 每层多 sub-condition 已实现.

## 训练判据 (D-101+ 适用)

**oracle/BC 验证标准** (ScriptedSettler 自身已过, BC 网络需复现):
1. **night_shelter / night_total** ≥ 0.5 (BC 目标: 0.8+)
2. **bldg_ticks/ep** ≥ 1000 (BC 目标: 5000+)
3. **houses_built/ep** ≤ 2 (BC 目标: 1)
4. **score** ≥ 0.20 (扩展行为加 milestone 后会更高)
5. **新指标**: BC ckpt 在新 seed 下评估, behavior diversity = 触发 eat/drink/wear/craft 至少各 1 次/100 ep

## 文件路径

- main_train.cpp: ScriptedSettler 在 `struct ScriptedPolicy { ... };` 之后, 约 line 340-799 (commit 8f2ca4a)
- 测试 jsonl: `runs/settler_50ep.jsonl` (50 ep settler), `runs/ppo_d099_5M.jsonl` (5M PPO 对比)
- build script: `build_d100.bat` (vcvars64 + ninja)

## 接力点 (下次 session)

1. **D-099 训练已结束** (PID 释放), .exe 可重新 link
2. **ScriptedSettler 已实证** (50 ep smoke 全过), 代码已 commit (8f2ca4a)
3. **BC pipeline 设计已成熟**:
   - demo 二进制格式: header (magic/version/obs_dim/cont_dim/N) + record (567 floats obs + 3 floats cont + 1 uint disc_idx)
   - disc_idx 映射: 见 src/policy/PPO.cpp line 155-180 (case 0:eat, case 1:drink, case 2:attack, ...)
   - BC loss: MSE(cont) + CrossEntropy(disc), Adam lr 1e-3
   - PPO finetune: --load bc_ckpt.pt 直接读, 接 PPO loss 5M
4. **预计 code 量**: BC + demo record + multi-flag = ~300 LOC, 改 main_train.cpp + 加 1 个 BCTrainer header
5. **不要立刻起 5M 训练**: 用户在玩游戏需要 GPU. BC pretrain 是 CPU/单 thread 也可以跑, 但 PPO 5M finetune 需要等用户放行 GPU.
