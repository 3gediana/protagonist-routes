# deep_protagonist · PROGRESS

> 最后更新: 2026-06-01 (D-119 / round_22 §4 营养可生存性闸 = **PASS**: settler 觅食+岸边叉鱼 存活 9000 步 / noop 4740 步 protein 饿死 / bcv9-OFF 身份不变存活 / bcv9-ON 4740 步饿死=known-gap 待重训补; 见 §D-119 + 决策记录)
> 历史更新: 2026-05-31 (D-110 C＋ 第二全新 bank 复核 + 脆性死亡诊断 — 1900k/2000k/2100k=90/90/40, 双 bank 合并中位 **85%** 无 0% 世界; 残余 13/60 = 7 夜宿地形+5 出生早狼, 均现有能力内 → 轻量 r8)
> 当前主线: **阶段2 双 bank 确认完成; Stage3 oracle 扩展严格挂起等用户定范围(S3-BC 延伸 vs S4-PPO)**。本轮地基加固: 脆性死亡主因=夜宿地形覆盖深度(7/13, 2100000 同 1000000 型)+出生区早狼(5/13), 均在现有 oracle 能力内 → 轻量 r8(不扩 oracle, 零范围扩散)。全新未训 bank 1600k/1700k/1800k = 80/80/90, nShel 全 1.00, attack 全 0; 诊断: clone 仅做纯定居(r_craft=0/r_kills=0/多屋未展), Stage3 造衣/狩猎/中期建造 需 oracle Layer 扩展(结构性, 待授权)。bc_v9 6-seed bank 跨 seed 中位 90% (700k100/800k100/900k80/1000k60/1100k90/1200k90)；**6/6 seed ≥6/10 且无 0% 世界**; 1000000 脱 0(nShel 0.04→0.92, 死29→8, bldg258→8159); attack 全 seed=0 无跷跷板; 555k sanity 17%→67%
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


## D-111 (R9) — bc_v10 (r8 加固) 验收 = NO-GO，冠军保持 bc_v9
- 停电打断的 bc_v10 全量训练已补跑完（同 362 文件/同超参，loss 单调 1.0263→0.3778，exit=0）。
- 同口径 A/B（10 整局/seed, steps=120000, 随机）：bc_v10 全面退步——训练 bank 中位 90%→65%、全新 bank1 80%→70%、全新 bank2 90%→50%、两全新合并 85%→60%；13 seed 中 10 退/2 进/1 平；1700000 80→10、2100000(r8 主攻) 40→30。attack 全 0。
- 结论：r8「混采夜宿地形+出生早狼、从零重训」对核心定居净负（容量稀释 + 疑早狼退避信号污染）。**bc_v10 不采用、不覆盖 bc_v9**。教训：只做聚焦增量，绝不从零稀释重训。
- 洞察：残余死亡=「会建房但夜里冻死/饿渴死」，r8 用「更多回屋 demo」无效 → 转 Stage3 御寒（造衣/火）。
- 已写 inbox/round_9，等 decision_round_9。

## D-112 — Stage3 Level-2a 火（取暖/营地）BC 验收 = NO-GO；火"用法"交 S4 PPO
- ①做啥：实现火世界机制（FireSystem：取火/搭火/续柴/半径取暖挂进 VitalSystem::tick()，动作 index17 不破坏旧 demo 血脉）+ 草裙配方 3 草→1 草 bugfix；加只读死因埋点；oracle 火层；冷 seed 聚焦增量 DAgger（2200110–2200139，30 seed，绝不碰评测 bank）→ 训 bc_v11（火-营地）。
- ②参数：fire warm_radius=6/merge_radius=3/max=8；bc_v11=bc_v9 349 + 15 火文件，8ep/b16/bptt64/lr3e-4，loss 单调 0.96→0.37。
- ③学到：死因埋点推翻"冻死"前提——bc_v9 死亡 **93% 饿/渴死、仅 7% 冻死**；瓶颈 2100000 = 整夜不进屋(nShel0.0) 被"夜间无遮蔽 3× 饿渴惩罚"饿死。火只暖体温、不算遮蔽，治不了这 93%。
- ④坑（关键）：把"夜里搭火留守"教给 BC 会与已赢的"夜里进屋"**在同一决策点互斥**，BC 无法像 oracle 脚本紧条件化 → bc_v11 全线塌方：nShel 0.95→0.15、训练 bank 90%→10%、冷死爆炸 0→20~52/seed（火取暖 45/min 远不如房子，agent 赖火边冻死）。同口径 A/B 排除假退步（bc_v9 复测仍 90%/0冻死）。
- ⑤裁定：decision_round_13 采 **Path A**——火世界机制保留（14/14 单测、sim/机械层无悔、可复用 S4），**火"用法行为"BC 不可行 → defer S4 PPO**（oracle-only 退出判据）；BC 主线回夜宿进屋残差。bc_v11.pt 仅留诊断，冠军仍 bc_v9。

## D-113 (EOD 收尾) — 夜宿主线 bc_v12 验收 = NO-GO（火标签污染），冠军保持 bc_v9
- ①做啥：Path A 夜宿聚焦增量——60 全新 seed(2300100–2300159) 用 bc_v9 跑夜宿 DAgger、选 5 残差文件、基 bc_v9 训 bc_v12(349+5)，过多轴门。
- ②参数：选择器 nShel<0.6 & builds & attack==0 → 命中 5/60（8% 命中证明 bc_v9 夜宿已 ~90% 泛化）；bc_v12 8ep/b16/bptt64/lr3e-4，loss 单调 1.07→0.41。
- ③学到：**夜宿残差杠杆是真的**——瓶颈 seed 2100000 **40→100**、2200102 50→60，证明针对性夜宿纠错能修残差，方向对。
- ④坑（坐实根因）：bc_v12 多轴 eval 中位 **60%** vs bc_v9 90% → NO-GO。退步因 5 个 night demo 是在 oracle 火层开启的 build 下采的，夜困态吐了 **107 个 action17(tend_fire) 标签**（解析 bin 实测 83+6+6+6+6），选择器没筛 fire==0 → bc_v12 又学了火-营地 → 冷死在已赢 seed 复现(700000 cold0→25、1100000 0→36、fire-token14~35)。同病 bc_v11。**教训：Path A 的 night DAgger 必须 fire-free（关 oracle 火层 或 选择器加 fire==0）**。
- ⑤EOD：bc_v12 不采用、不覆盖 bc_v9。清理 runs 22.16→10.61GB(-11.54)。写 inbox/round_14（自归档）。本地 commit 未 push，未开新轮。下次=纠正版 fire-free 夜宿 DAgger 重做（预期复现 2100000 40→100 而无冷死副作用）。

## D-114 (round_15) — Path A 关火层 fire-free 夜宿 → bc_v13 多轴门 = NO-GO（全新 bank 真退），冠军保持 bc_v9；火污染病根治
- ①做啥：执行 decision_round_14。先验 Path B（旧 demo 加 fire 过滤）= 死路（5 个旧 night bin 解析 action17=107 全污染）→ 走 Path A：给 oracle 加 `--oracle-no-fire` 门控 3 个火分支（Layer 1c/2a'/3.6）、重编 EXIT0、smoke 验 fire-free demo 的 action17==0 且 clone 仍建房+夜宿（无火拐杖）→ 同 5 残差 seed 重采 fire-free（5 个新 bin action17 全 0）→ 训前硬断言 action17==0 才放行 → 基 bc_v9 聚焦增量训 **bc_v13**（349+5=354，8ep/b16/bptt64/lr3e-4/os20，loss 1.0114→0.3587，EXIT0）。
- ②同口径 A/B（同 session 同 exe 新跑 9 seed，10 局/seed，attack 双方 0）：训练 bank 中位 80=80（1000000 +20、余平）；全新 bank 中位 **90→80**（1700000 80→70、1900000 100→80、2000000 90→100）；残差 **2100000 30→80**（nShel0.30→0.78、食/冷死29→0）、2200102 50→50。**全 9 seed fire=0、attack=0、cold=0**（bc_v9 在 2000000 有 cold=3）。
- ③噪声复核（30 局）：1700000 80→75、1900000 86→75 → 全新退是真实非噪声；机理=夜宿"回家"偏置挤压白天觅食 → food-death↑（1900000 food死 7→29）。注意 bc_v13 的 nShel 不降反升，pass% 降仅因 food-death 拖累。
- ④多轴门：训练 bank ✅、全新 bank ❌（关键阻挡）、残差 ⚠️半（2100000 大胜、2200102 平）、attack==0 ✅、零火污染 ✅。**bc_v13 = NO-GO，不采用、不覆盖 bc_v9**（v10/v11/v12/v13 四连 NO-GO）。
- ⑤学到（关键）：与 v11/v12 根本不同——`--oracle-no-fire`+训前 action17==0 断言这条工程链**彻底根治火污染病**（全 9 seed fire=0/cold=0），且残差杠杆**干净复现**（2100000 30→80 无任何火/冷副作用）。新暴露的是 BC 全局聚焦增量的**专化/泛化权衡**（夜宿偏置→全新 bank 觅食代价），非 Goodhart、非污染。
- ⑥下一步候选（写入 inbox/round_15）：A=缩小/锐化增量（只留 ≤2~3 个最硬 nShel≈0.30 bin + 下调 night oversample）重训 bc_v14（推荐，留胜去损）；B=补少量白天觅食 demo 反向平衡；C=夜宿残差也收编 S4 PPO。工程链已现成可复用。
- ⑦收尾：冠军 bc_v9 完好未覆盖（已校验 .pt + 06:46 时戳）。清理：压缩 bc_v13_train.log 290.9MB→366行摘要、删 night_ff_smoke/eval 日志。D 盘 free 200.0→200.35GB（红线 ≥200 已守）。本地 commit（`deep_protagonist:` 前缀、不 push）。写 inbox/round_15，盯 outbox 下一条决策。


## D-116 / round_16 — Path A（缩小+夜宿降权）bc_v14 NO-GO，触发 §4 硬止损线 → 转 S4 PPO
- **Path A 实现**：新增 night 专属降权通道 CLI `--bc-low-os N` + `--bc-low-os-demos path,...`（BCTrainer 按文件设 eff_trig_os_，base 权重不动只降夜宿；smoke 验证 night groups 1254 == 全局 os8 vs os20 的 3134）。
- **bc_v14** = bc_v9 的 349 @os20 + 2 最硬 fire-free 夜宿 bin（2300148 nShel0.296 / 2300105 nShel0.300）@low-os8；8ep/b16/bptt64/lr3e-4/trig_w15；训前 action17==0 硬断言通过；loss 0.908→0.320；EXIT0。
- **多轴门 vs bc_v9（同一新编译 exe，9 seed×10ep + 1700/1900 ~16-17ep 复核）**：
  - 训练 bank 中位 v14 70 < bc_v9 80（900000 nShel0.90→0.58）→ 退。
  - 全新 bank 中位 v14 80 < bc_v9 90；**1700000 崩盘**：复核 pass 80→6、nShel0.86→0.43、food-death 33→60（≈2×）；1900000 food 7→22（≈3×）、cold 0→9。
  - 残差正向但弱：2100000 30→50（+20，< v13 +50）、2200102 50→60（+10）。
  - fire=0 / atk=0 全 9 seed 两模型 → ✅ 火链稳。
- **判 NO-GO**；满足 decision_round_15 §4 触发条件（全新 bank food-death 仍软退/崩盘）→ **§4 硬止损线触发**：BC 无法无代价外科式修夜宿残差，停止再造 bc_v15 系列 BC 变体 → 转候选 C（夜宿残差交 S4 PPO，from bc_v9 暖启动 + potential-based shaping）。冠军保持 **bc_v9（未覆盖）**；bc_v10–v14 全 NO-GO。
- **阶段切换**：Stage2 BC 主线（夜宿进屋残差）→ S4 PPO；按 decision_round_15 §6 例外已 ping 用户。
- **磁盘红线告警**：本路线已清全部过期数据（旧火污染 night_dagger、过期 demos_d101 1.9GB、bc_v10/11/12、训练/eval 日志），D free 仍跌至 196.2GB（<200），持续下跌源自共享 D 盘其它 route/决策者，非本路线可独自解决。
- commit 本地（前缀 deep_protagonist:，不 push）。


## D-116 / round_16 收尾 — decision_round_16：§4 确认，采纳 C（夜宿残差→S4 PPO）；Step1 固化完成
- 决策者确认 **§4 触发**：v13（5 bin os20）软退 5–11pt、v14（2 bin os8）反而 1700000 崩盘 −74pt + 训练 bank 退 → **减小扰动非单调改善** = BC 全局克隆在夜宿残差上的**结构性专化/泛化权衡**；v10–v14 五连 NO-GO 已证伪 BC 路径。**BC 夜宿进屋主线收尾。**
- **Step1 固化（非破坏，已做）**：
  - **bc_v9 锁为最终 BC 部署冠军，永不覆盖**；bc_v10–v14 全部归档 NO-GO（bc_v9.pt 在盘未动；v10/11/12.pt 已为腾盘删除，v13/v14.pt 留盘作 NO-GO 记录）。
  - 可复用资产固化记档：`--oracle-no-fire` fire-free 采集链 + 训前 action17==0 硬断言（EXIT9 护栏）+ 9-seed 多轴门 harness（eval_gate.ps1 / eval_parse.py）+ night 专属降权通道（`--bc-low-os` / `--bc-low-os-demos`，base 不动只降夜宿）。
  - 夜宿进屋残差记为 **known-gap**；fire-free night demos（runs/night_dagger_ff，action17==0）留存作 S4 PPO 的 warmstart / 参考轨迹。
- **Step2（待用户发令后展开）**：S4 PPO smoke，from bc_v9 暖启动（critic 重初始化）+ 夜宿走 **potential-based reward shaping**（F=γΦ(s')−Φ(s)，Φ=夜间在屋内/近庇护势函数，policy-invariant，Ng+1999）；**严禁**对 nShel/进屋/食物直接加奖（Goodhart）。smoke 验：不退已赢 bank / 残差方向正确 / fire=atk=0 cold 不回升 / 全新 bank food-death ≤ bc_v9（v13·v14 双栽的轴）。
  - 决策者已 ping 用户给 C（现在投 S4 PPO 修夜宿）vs B（先冻结夜宿为 known-gap、S4 PPO 待整体就绪统一修）的取舍；执行者 Step1 已固化完成，**S4 PPO 具体投入按用户对 C/B 的发令再启**（遵用户"模型运行需统一发号施令"红线）。
- 磁盘：DP 侧已清全部过期数据，D free 196GB；决策者核实持续下跌主因为另一 route（protagonist_ecology）在途 event_trace 累积，距 30GB 硬停线极远，非紧急，DP 照常推进。


## D-119 / round_22 §4 — 营养可生存性闸 = PASS（觅食+岸边叉鱼；冠军身份保全；bcv9-ON gap 记账）
- **根因坐实（用户诊断成立）**：bc_v9 用 **collect** 觅食/采建材，营养回填却挂在 eat(植物→vitamin)/attack(肉→protein)/drink-fish(鱼→protein) —— 取食动作与回填动作未对齐 → 营养ON 必饿死。这是**预期的"未重训缺口"**（待 §3a 暖启动 + settler-demo 重训补），非接线 bug（动作枚举 encode/decode 一一对应已核）。
- **§4 结构性修复（不做奖励 shaping，承稳态驱动）**：
  1. settler oracle Layer 3N 重写：`try_forage`（觅食→vitamin）+ `try_protein`（早期 craft 长矛 → 岸边叉鱼 → 持矛追近陆猎兜底）。
  2. Environment：drink/near_water 判定 2→6m（地形岸坡把 CapsuleAgent 卡在水心 ~5.7m，2m 物理够不着）；fish 叉击 reach 2→6m（岸边叉鱼，鱼仅 1.5m/s 且困在水中→可靠 protein 源，对照陆猎 6-7m/s 逃逸）。
  3. 出生水域旁重定位 10 条鱼（70m 内扫 River/Swamp 格、按距排序、铺最近 6 格）。**鱼不入 agent 观测**（k_nearest 明确 fish/eagle out-of-scope）→ 不扰 PPO obs（避开 D-067 prey 密度回归灾难）；陆猎 rabbit/deer 不动。
  4. AnimalSystem 加 `animals_mut()` 可变访问器（建图期重定位鱼用）。
- **§4 四臂闸结果（decay 40/40, seed 2100000, 9000 步）**：
  | 臂 | 步数 | 死亡 | protein/vit | 行为 |
  |---|---|---|---|---|
  | settler ON | 9000 | 0 ✓ | 5/20 存活 | eat=524 drink=568（觅食+叉鱼维持 protein ~0.35–0.44）|
  | noop ON | 4740 | protein | 0/0 | 惰性→饿死（营养成约束 ✓）|
  | bcv9 OFF | 9000 | 0 ✓ | 100/100 | collect=96 建房夜宿 attack=0（身份不变；鱼重定位/6m drink 未扰动 ✓）|
  | bcv9 ON | 4740 | protein | 0/0 | 行为同 OFF(collect=96)→饿死（**known-gap 记账** ✓）|
- **多 seed 复核（40/40）**：营养非杀手（surviving 局 deaths_protein=0，少至 4 次叉鱼即维持 protein≈43）；少量非营养死（出生早狼/夜宿地形，满 bar 死，预存非新增）。
- **裁定**：§4 PASS —— 世界**凭技能可活**(settler) + **营养成真实约束**(noop 死) + **冠军身份保全**(bcv9-OFF 不变) + **bcv9-ON gap 已记账**(待重训)。下一步=在此营养世界用 settler demo + §3a(567→569) 暖启动训 **s4ppo_nutrition_v1**（attack 已解屏蔽 mask_fire_only；承 decision_round_20/21 知识：稳态驱动、不叠竞争势函数、KL 只守基本功）。
- **红线核对**：营养只回条不发奖（稳态）✓；bc_v9/c1 未动 ✓；鱼不入 obs 保 PPO 身份 ✓；本地 commit 不 push ✓。

## D-120 / round_23 prep — settler 64-seed 鲁棒性扫 + s4ppo_nutrition_v1 的 BC demo 语料
- **64-seed 复核（settler, decay40, 9000步, ep0）**：clean 存活 **28/64 = 44%**；失败死因拆分 = **bite(早狼) 16 / cold(夜宿暴露) 16 / protein 4 / vitamin 0**。
  - 量化坐实 D-119 定性结论：**营养已基本不再是杀手**（protein 仅 4、vitamin 0；§4 修复有效）；残余死亡=**正交的捕食者+寒冷**（预存 oracle 鲁棒性缺口，与 §4 无关，正是 PPO 重训该补的）。
  - 注：train.exe 单次跑多 episode（死则重生续跑），上表按 ep0；--record-demos 按 --steps 固定写满记录，故文件等大≠存活，须用 metrics 的 steps/r_death 辨真伪。
- **BC demo 语料就绪**：从 28 个 clean-survivor seed（ep0 满 9000 步、death=0）录 settler demos → demos_s4/（28 files, 551.8MB）。**仅取存活局**避免 BC 模仿「觅食→中途饿死/被咬死」的坏轨迹。
- **下一步（GPU 待 eco 长跑腾出后）**：从 bc_v9(567→569 暖启动) 在这 28 条 demo 上 BC 预训 → PPO 微调（mask_fire_only，attack 已解屏）。目标=用 alive-reward 学会**捕食者规避 + 寒冷时进屋时机**，把跨 seed 存活从 oracle 的 44% 抬上去。
- 红线核对：仅录存活 demo 不改 env/不发奖 ✓；bc_v9/c1 未动 ✓；本地不 push ✓；D free 201.6GB ✓。

## D-121 (2026-06-03) Stage W1 天气耦合上线 + 起训
- 世界机制：天气系统（晴⇄雨）+ 雨浇火 / 湿增寒 / 雨补水；obs 569→571→**573**（+雨强 +湿度，append-only 零填充热启动）。
- 验证：单测 13/13；自测 obs 断言全过；s5_v2.pt 热启动手术 571→573 OK，无 NaN；n-envs8+weather ~2900 步/秒。
- 训练：D-121 weather ON + hunt-shaping 0.05，从 s5_v2.pt 热启动，n-envs 8，48M 步（~4.7h），WMI 独立进程（断 SSH 不停），save runs\w1_v1.pt。
- 磁盘：D 212G，单一覆盖式 w1_v1.pt（+.opt）。
- 已知项：自测 river/animal/yaw/wardrobe 4 条为既有 stale 失败，与 W1 无关。

## D-122 一锅炖 I/O 锁死 + RUNG 1（生火→烤肉+照明）
- **PHASE A 一锅炖（最后一次改维）**：动作口 18→23（预留 cook/mine/craft_axe/craft_pickaxe/build_monument），obs 573→582（+9 科技感知，先填 0）。新行列热启动零填充；--allow-* 课程门 + PPO -inf mask 控制解锁。
- **RUNG 1 机制**：FireSystem::consume_fuel_near（烤肉因果代价，烧最近 lit fire 燃料）；Environment cook 处理(idx18)：lit fire + raw_meat>0 + 烧 15s → 1 生→1 熟 + 首次里程碑；eat 优先吃熟肉(45 kcal/×1.3 蛋白)；夜里 lit fire 照明可采集；obs 573-575 填实信号；raw/cooked 独立计数器封顶 5。
- **验证**：编译过；存档手术日志确认 573→582 / 18→23 零填充；7.2万步 --allow-cook 跑 8 命零崩零错、老行为完好；`--cook-selftest` 端到端 10/10 PASS（含 2 个因果负例：无肉/无火皆 no-op）。
- **起训**：build_d122 exe，--load w1_v7.pt --weather --allow-cook + v7 超参，n-envs8 seed24 → runs\r1_cook_s24.pt（PID 30892）。起跑 ep4-6 满9000步 R+40~60 score~0.31，无退化无 OOM。监控：盯 cook OK 出现 + 旧指标对比 v7。
- **PARK**：山洞可视化（web3d 查看器，与训练无关）。**LATER**：按蓝图逐条审 PE 补齐被阉割部分。

## D-122 RUNG 2-5 机制全部落地 + Boss 改定时（代码 ready，未起新训练阶段）
- **RUNG 2 挖矿**：进洞 + 矿点旁 + 耗体力才挖到矿；`--mine-selftest` OVERALL PASS（含 3 个因果负例：无矿点/不在洞里/无体力皆 no-op）。
- **RUNG 3 石器**：1木+1矿→石斧(砍树更快+1木)/石镐(挖矿-50%体力+1矿)；`--tools-selftest` 15/15 PASS。
- **RUNG 4 建造升级**：tier-gate 谓词接进真 place_blueprint——门关=v7；门开 Shed/WoodHouse 免费、StoneHouse 需镐、BigHouse 需斧+镐；`--tier-gate-selftest` 16/16 PASS（含端到端 place 接线证明）。
- **RUNG 5 巨炉+Boss（定时版）**：用户拍板"Boss 固定时间来，不是建好炉子才来"。Boss 每命 t=180s 必来寒潮(60s)；巨炉=护盾(搬10矿建成→8m 内 60温/分光环)，须赶在截止前建好才扛得住，否则靠火+遮蔽；obs581 改成倒计时钟。`--monument-selftest` 14/14 PASS（建好但没到点→Boss不来 / 到点没炉→Boss照来 / 光环扛过整场→终极里程碑）。
- **整体**：build_d122_v2 独立编译 BUILD_OK；**5 关自测全 PASS**；RUNG1 训练(PID32388)全程未断、score 0.284 稳、新动作口恒 0、无退化。
- **磁盘/红线**：网络维未动(23/582 锁死)、未重置未删旧奖励、本地不 push。
- **下一步**：RUNG1 训练继续挂跑等 cook 冒头；冒头后按因果链解锁 RUNG2…（解锁是世界端开关，不再改网络维）。
- **PARK**：山洞 web3d 可视化。**LATER**：按蓝图逐条审 PE 补阉割部分。

## D-122 RUNG1 cook 续训 c2（2026-06-03）
- **首轮结束**：r1_cook_s24 跑满 48M 步/9231 回合自然结束(非崩)，存终值 ckpt；cook 始终 0(agent 不打猎 kills=0→无生肉)。
- **续训 c2**：AI 自决守"别停"——`--load runs\r1_cook_s24.pt`(已 582/23→无 surgery，加载 .opt→真续训带动量)，超参全沿用，另存 r1_cook_s24_c2.*。起跑 PID28640、update#137、ep6 score0.253(未塌)、ent~3.0、kl<0.05、stderr 空。
- **mark(E) 同会话交付**：PE 领地标记系统真实现(TerritoryMarkLayer 衰减/合并/容量/族归属 + mark 动作能量闸门 + evict 扩到"基地或自标记领地" + 感知读自有地/外族威胁)；两 exe 重编零错，`--action-selftest` 12/12 PASS、`--kin`/`--social` 无退化；未接入训练(权重清零待激活)。
- **下一步**：盯 c2 cook 是否冒头；若仍 0，待用户拍板是否追加"打猎得肉"小奖励(加项不删旧)。

## D-122 RUNG1 cook 打猎激励（hunt-nudge run，2026-06-03）
- **问题**：cook 跑满 48M 仍 0（agent 不打猎→无生肉）。
- **解法（守铁律）**：扩展 D-043 P2 状态持有势能 φ——加"手里有口粮"(raw 0.05/u 封顶0.15、cooked 0.18/u 封顶0.90)，cook_enabled 闸门→cook 关时 v7 字节一致。非直奖(防 D-064/065 刷分)，封顶在 ±10 clip 内。
- **验证**：build_d122_v2 独立编译 BUILD_OK；`--cook-selftest` 9/9 PASS；从 r1_cook_s24.pt 真续训 hunt-nudge run(PID17940)起跑 score0.253 未塌、老本事在、kills 待显效。
- **下一步**：盯 kills>0 / cook OK 首现；若仍不动再调 φ 权重或 RUNG1 课程。

## D-122 系统机制闭环审计 + 寒夜稀缺 + 精确溯源惩罚（2026-06-03）
- **审计**：站原始人视角看 5 条链，结论"链闭经济开"——基准线吊床(摘果子+窝棚回血=永生零风险)→每级科技白加负担、cook 天生没需求(杀了当场吃饱)。账本 docs/机制闭环审计_账本.md。
- **修复(只加不删、cook_enabled 闸门、v7 字节一致、备份 .bak_econ_20260603)**：①寒夜窝棚不再回饥饿(逼囤熟肉过冬)；②凸的按指标溯源惩罚 event_vital_danger(coef 0.02，平方亏空，精确信用分配)。
- **验证**：build_d122_v2 编译 OK；`--self-test` 两条 vital_danger 单测 PASS(另 4 条 river/animal/wardrobe/observation FAIL=pre-existing 无关)；`--cook-selftest` 9/9 PASS。
- **切血脉(本轮收尾)**：杀旧 hunt-nudge(PID19956)、留旧经济对照 `_c2`(PID7984)做 A/B；从 r1_cook_s24.pt 真续训起修复版 `_e`(PID32556，critic-warmup64)，起跑 score0.255~0.264 未塌、老本事在、含新溯源惩罚。
- **下一步**：盯 `_e` kills>0 / cook OK 首现 + 跟 `_c2` 差异。用户指示跑完这轮收手。

## D-122 修复版 `_e` 收尾：寒夜稀缺单独不足以诱发打猎（2026-06-04，用户选 A 提前收手）
- **结果**：`_e`(PID32556) 跑到 PPO #8137 / ~700 回合，**kills 全程 0**、score 0.257→**0.22 横住**(稳 ~6000 更新)、回合变短(早死)、网络健康(ent 3.0~3.7/kl 0.013~0.053 未塌)。`_c2`(PID7984,旧经济对照) 同停。终值 ckpt 各 48.7MB 已保。
- **诊断**：僵局=压力造出来了但探索没逼出打猎；hunt→cook 第一环(kills>0)始终没点火。结论：**单靠制造稀缺不够**，得再给打猎探索引导。不是网络坏。
- **用户决策**：A=现在收手(省 4~6h 跑满 48M)。
- **下一步（提前预警仪表盘）**：①打猎动作 advantage 均值+符号(单调预判趋势)；②`--forced-hunt-probe` 短诊断分清探索不够 vs 激励不够；③时钟加速/回合缩短压时间尺度。目标：后续机制改动"几分钟先看方向再决定跑不跑满"。

## D-122 提前预警仪表盘落地 + D盘清理 + SF 设计稿（2026-06-03，收尾）
- **提前预警仪表盘(已落地，commit 44c6646)**：`PPO::mean_adv_for_action` 纯函数 + `UpdateStats` 4 字段 + 两处日志打 `hunt_adv/cook_adv` + `--self-test` 加 6 条(含负例)。两 exe 重编零报错、adv-watch 6/6 PASS。真实样例：`PPO update #1 ... hunt_adv=+0.000(n=0) cook_adv=+0.000(n=0)`(n=0 印证此血脉起步没打猎)。
- **D盘清理(释放 ~250MB)**：删 smoke 一次性临时(smoke_ew_DELETEME.pt/.opt~153MB)+294 个旧 console 日志/过期实验 jsonl(187.66MB)+跑 prune_runs.ps1 清过期 ppo_dXXX(62.7MB)。**金标全保**：r1_cook_s24/_e/_c2(+.opt) 核验 OK、本血脉 4 个 jsonl 留存、gold d061/d065/d076 留。新增维护脚本 scripts/cleanup_logs.ps1。
- **Successor Features 设计稿(docs/Successor_Features_设计.md，未实现，留下一个号)**：φ=复用 12 个 StepReward 分项(存未乘权重值)、ψ 头=并排 value 的 Linear(1408→12，首版 detached 主干零扰动冠军)、TD 规则 ψ(s)←φ+γ(1-done)ψ(s′)、6 条自测(含 2 负例)、非线性烤进 φ、与仪表盘+探针配合做"换配方离线秒筛 hunt_adv 符号"。带 9 步实现清单。
- **用户指示**：跑完这轮收手；本轮起不再起新训练活。
