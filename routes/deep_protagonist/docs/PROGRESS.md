# deep_protagonist · PROGRESS

> 最后更新: 2026-05-31 (D-109 候选 C 全新 bank 泛化复核 — bc_v9 在未训 1600k/1700k/1800k 中位 **80%**, 无 0% 世界, attack=0 → **90% 是真泛化非过拟合 → Stage2 真正完成**)
> 当前主线: **阶段2 真正完成(双 bank 确认) → 进阶段3(诊断先行)**。全新未训 bank 1600k/1700k/1800k = 80/80/90, nShel 全 1.00, attack 全 0; 诊断: clone 仅做纯定居(r_craft=0/r_kills=0/多屋未展), Stage3 造衣/狩猎/中期建造 需 oracle Layer 扩展(结构性, 待授权)。bc_v9 6-seed bank 跨 seed 中位 90% (700k100/800k100/900k80/1000k60/1100k90/1200k90)；**6/6 seed ≥6/10 且无 0% 世界**; 1000000 脱 0(nShel 0.04→0.92, 死29→8, bldg258→8159); attack 全 seed=0 无跷跷板; 555k sanity 17%→67%
> 当前蓝图阶段: S3 settlement 基本闭环(建房+整夜过夜 跨世界一致 6/6); 三层 DAgger(r5狼/r6建造/r7夜宿)均衡; 残留=1000000 仍 3/10 顶固地形子集; PPO 自训练救 camp 已否决
> KPI (D-104 永久, D-105 扩): 留出 seed bank = 700000/800000/900000/**1000000/1100000/1200000** 各 10 局; 看逐 seed + 跨 seed 中位(不看 pooled 均值, 防易世界拉偏); 555000 = 记忆 sanity。DAgger 永不在留出 bank 上采。

## 🗺️ 开发路线图（全自动执行手册 —— 用户不在时照此自己决策，绝不空等）

**默认行为 = 全自动连续跑**：一轮"训练+eval"完，立刻按本图判断阶段+GO，满足进下阶段，不满足照"出问题→自主决策"那列立刻执行再开下一轮，抓住每段空闲时间不停训。**所有岔路口决策已替用户拍好，照做不问**，每轮记账到 `决策记录.md`。**唯一发消息情形=物理阻塞**(连不上机器/chisel断/缺凭证/磁盘满/编译坏)，且边等边干能做的。

**北极星(当前主任务,数周可达)**: 多数 seed 稳定"建房+整夜过夜" 且跨世界一致。进度: bc_v4 6/30 → bc_v6 中位 60% → bc_v7 80% → bc_v8 85%(5/6) → **bc_v9 跨 seed 中位 90%, 6/6 seed 过门槛无 0% 世界 → Stage2 验收门达成**。三层 DAgger 分层攻克三种失败: r5(狼世界逆袭生存)、r6(建造失败/attack 劫持)、**r7(夜宿失败: 会建不会住)**。**诊断先行已成常规**(C/C′ 连两轮纠正错误方向): C(D-107) 推翻渴饿假设→重瞄建造失败; C′(D-108) 謁夜宿是地形泛化缺口(45/60 世界 nShel=1.00, 唯 ~25% 世界会建不住)→r7 直击该地形。结果 1000000 0→60%、nShel 0.04→0.92、attack 保持 0。下一里程碑=Stage3 行为质量打磨(造衣/狩猎/中期建造)。

| 阶段 | 干什么 | GO(自动进下阶段) | 出问题→自主决策(立刻照做,不停,不问用户) |
|---|---|---|---|
| **0 基建**(收尾) | D-103 提速(已完 ~7.2×) + **warm-start 增量**(待做: `--bc-init` load 上轮权重只微调新数据+少量旧数据复习) | 单轮分钟级 且 与从零训练等价 | 退化→自动提旧数据复习到新:旧≈1:1；仍退化→自动回退从零全量重训(提速后也十几分钟),继续不停 |
| **1 DAgger 推达标过多数**(⭐现在) | eval bc_vN→找失败 seed+模式→oracle 生成纠错 demo→聚合→warm-start 训 bc_v(N+1)→再 eval。r4→bc_v6 目标>15/30 | 达标率单调上升 + 夜间在屋率升 | 不升别盲目加数据,按序自动试:①分类失败(白天bldg不够/夜里nShel不够/死亡)②对应改 oracle 决策树相应层重训③仍不升→自动加大失败seed占比+延长录制集数。全程不停,每轮记账 |
| **2 稳健+降死亡** | 自动触发(达标>15/30):多 seed bank 验证泛化 + oracle 生存复位示范压死亡 | ≥2 个没训过 seed bank 达标一致 + 死亡明显降 | 换 seed 崩=过拟合→自动加 demo seed 多样性回阶段1；死亡高→自动加生存复位 demo。照做不停 |
| **3 行为质量打磨** | 自动触发(达标稳健+死亡低):补齐蓝图覆盖(造衣/狩猎/中期建造) + 动作连贯拟真 | 蓝图主线 BC 覆盖达标 + 录像自然 | 行为学不会→自动查 oracle 该行为 demo 数量/质量并补。达标后自动进阶段4(发一条收官汇报但不停) |
| **4 蓝图终极愿景**(自然延续,不需拍板) | 按 `docs/最终蓝图.md`:同一只网络 S4→S6 参数永不重置,渐进扩到 50-60M,把完整生存进程(早期采草造衣/觅食/狩猎→中期建造4房型→后期农耕/饲养/驯服→S6用户实时干预)练到"行为和真实世界一模一样",EWC/旧奖励权重防遗忘 | 每 progression 阶段行为稳定且不退化旧阶段(分项曲线,任一退化自动警报+回退 ckpt) | 月级长跑,每次只推进一个最小子目标(先造衣稳再上建造);遇架构/参数选择按蓝图选最小可行方案记账继续;显存不够缩 batch/分阶段。持续推进不停 |

**当前位置**: 阶段1 逼近 Stage2 (跨 seed 中位 80%, 新 seed 2/3 过, 差 1000000)。oracle 体检=分叉②(oracle 合格, 纯 BC 缺口), 不改 oracle。bc_v7 r5 生存纠错对狼世界大胜且 camp 不退。**下一轮等 decision_round_4 锁靶: 修 1000000 渴饿型难世界的 vital 维持 — 倾向 (A) r6 采渴饿型新 seed(r_bites≈0 但 r_vital 重) 补 vital 纠错, 或 (B) BCTrainer 对 drink/eat 单独加权(现为 16 触发统一权重, 不碰 reward), 或 (C) 先诊断 900k/1000k 死亡上升的动作分布。** 降死亡只走 oracle 生存 demo + 训练侧 oversample/加权, 绝不加直接奖励(D-064)。bc_v7 camp 未退 故暂留作 r6 采样 clone。
**红线(怎么做的约束,不是停的理由)**: 只走 DAgger+oracle 不用 PPO 救 camp / 不为指标给直接奖励 / reward·obs(568锁死)·数据·超参不动(阶段4扩网络是蓝图明许的结构演进) / 提速只走数学等价 / 每批必 commit / 任一行为退化自动警报+回退。

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
| **D-101 BC→PPO 5M finetune** | **trigger-oversample BC (bc_g) + critic-warmup 64 + ent 0.001** | **30/30 集建房** | **32-83 (newseed)** | **0.014-0.033** | **建房 YES / camp NO** |

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

## D-101 BC pipeline + PPO finetune (2026-05-30)

**已实现** (main_train.cpp +163 LOC, 新增 include/policy/BCTrainer.hpp):
1. `--record-demos PATH`: settler 跑时把 (obs[567], cont[3], disc_idx, done) 写入二进制 (header "DPB1")
2. `--policy bc --bc-demos a,b,c --bc-out bc.pt`: 加载 demos → 监督训练 ActorCritic (GRU), 损失 = MSE(cont) + 加权 CE(disc), truncated-BPTT
3. BC 调参 flags: `--bc-epochs --bc-batch --bc-bptt --bc-lr --bc-trigger-weight --bc-min-eplen --bc-max-eplen --bc-build-oversample --bc-build-len`
4. `--deterministic`: PPO eval 用分布 mode (mean cont, argmax disc), 去采样噪声测 clone 真实学到了啥
5. `--demo-no-regen`: 录 demo 时临时关 in-building regen (D-101 demo 多样化, 最终未用)

**demo 录制**: 40 个 settler seed (100100..139100), 各 1 ep, 默认 regen, 录到 runs/demos_d101/s*.bin (39 ep 有效, 1 个 instant-death 丢弃, 346k records)

**BC 调参核心难点 (deposit/camp 失衡)**:
- oracle 行为极简: 走到 auto-seed 工地 (~30 步) → deposit 2 次 (step 31, 34) → 之后 ~8966 步纯 NOOP + 站着不动. 即 deposit 只占 0.02% token.
- 朴素 BC (全 episode + trigger_weight 5) → 收敛到 "always NOOP", bldg=0 不建房.
- 短 window (120-300 步) + 高 weight → 学会 deposit 建房, 但学不会长期 camp (建完就乱走觅食).
- 长 window (1500+) → 学会 camp 但 deposit 又被淹没, 不建房.
- **解法 (build-oversample)**: 每个 episode 额外复制前 50 步 "build 片段" N 次塞进训练集. 全 episode 教 camp, 复制片段教 deposit. `--bc-build-oversample 30 --bc-build-len 50 --bc-trigger-weight 25`.
- 修了一个 batch 分组 bug: shuffle 把 9000 步 full episode 和 50 步 snippet 混进一个 batch → padding 爆炸 (每 epoch 5min→正常 36s). 改为只 shuffle group 顺序, 保持长度同质.

**bc_d101.pt** (os30/w25, 12 epochs, total loss 1.26): deterministic eval (seed 555000, 10 ep) → 每 ep 建 1 房 (houses 1/ep), shelter=+5, collect=+2, milestone=+2, **不再乱走觅食** (collect 动作=0). 仍有 over-deposit (deposit 刷屏) + 轻微 drift 出屋 (bldg~210), 留给 PPO finetune 用 trigger_cost + in-building reward 修正.

**D-101 BC pipeline (完成, 详见上文 + 决策记录.md)**:
✓ 全部实现并跑通 (BC pipeline + trigger-oversample + critic-warmup + ent-coef CLI). 5M finetune 完成, 双墙结论如上.

## 后续路线 (D-101..D-110 蓝图)

**D-101 BC pipeline (✓ 完成 2026-05-30 — 原计划如下, 全部落地; 结论见上文)**:
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
