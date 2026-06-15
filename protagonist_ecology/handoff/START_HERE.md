# protagonist_ecology (PE) · START_HERE（接手必读）

> 平行项目。先读 `../deep_protagonist/handoff/START_HERE.md`（主项目 + 连接 + 总诊断），再读这篇。
> PE 撞的墙和 DP **是同一面**（近身狩猎打不到、绝大多数机制空按）。

## 0. 一句话
PE = **群体协同进化**路线：用 NEAT/进化（不是单体 PPO）演化**一群**智能体在同一世界里求生，
看能否涌现出**生态位分化**（顶级捕食者/群猎/伏击/食腐/杂食/大型食草/小型采集 7 个 niche）和**协作/通讯**。
它自带 niche 系统 = 原始的行为档案（≈MAP-Elites）。

## 1. 关键路径
| 东西 | 路径 |
|---|---|
| 项目根 | `D:\claude-code\c++\routes\protagonist_ecology` |
| **进化二进制** | `D:\claude-code\c++\build_ninja_cuda\bin\neural-eco-protagonist-batched.exe`（需 CUDA 在 PATH：`D:\NVIDIA\CUDA\v12.8\bin`） |
| 配置 | `onepot_sD_warm_<seed>.toml`（从 `clone_pe.ps1` 生成） |
| 结果/检查点 | `data\runs\onepot_sD_warm_<seed>\<时间戳>\checkpoint_gen<N>\` |
| 行为指标 CSV | 同上 run 目录里 `protagonist_behaviors.csv` / `evolution.csv` / `fitness.csv` |

### 资产（别删，见 PITFALLS）
- **完成种子 50001–50007**：跑满的，要分析的资产。
- **热启动种子 50021–50024**：本轮从 `checkpoint_gen159` 续跑到 gen ~290（朝 320）。
- 共用热启动基座 = `data/runs/onepot_sD_warm_50002/20260604-115936/checkpoint_gen159`。

## 2. 当前涌现了什么（P0，4 个种子，详见 `../deep_protagonist/handoff/P0_baseline.md`）
1. **"投掷"是唯一真成形的机制**（命中率 ~19.6%）——它们靠远程扔矛/石头吃饭。
2. **近身狩猎死**：hunt 5 万次尝试、成功率 0.002%（和 DP 同一面墙）。砍树/造物也都 5–6 万次≈0 成功。
3. **赢家是"投掷型杂食通才"**：杂食 niche 适应度翻几倍，但 apex/pack/ambush 专精打不出头（被通才 wash out）。
4. **零通讯**：`signal_response ≡ 0`（世界里没有"必须听信号才能完成"的任务）。**零生火、零喝水**。
5. 自带 **MAP-Elites coverage 只有 0.2–0.32**，一半种子还在往下掉 → 行为空间大部分空着。

## 3. 下一步（P1，待用户拍方向；与 DP 共用一份方案）
- 移植 **DP 的两阶段狩猎机制**（D-134）拆 PE 这堵一样的近身墙。
- 把内置 niche 升级成**真 MAP-Elites**（每生态位各留一个 elite，通才不能 wash out 专精）。
- **通讯**：加①信息不对称②非协作不可的任务（需≥2 个体才能放倒的猎物）③回应信号能提升适应度。
- 评估口径：**被填满的生态位数↑ / coverage↑**，不是单个 fitness。
- 完整方案：`../deep_protagonist/handoff/训练方案_全面涌现.md`。

## 4. 操作
| 想干啥 | 跑这个 |
|---|---|
| 启动一个新种子续跑 | `handoff\scripts\launch_seed.ps1 -Seed 50031` |
| 看进度/有没有崩 | `handoff\scripts\monitor.ps1` |
| 出涌现谱（读 CSV） | 把 CSV 拉回 VM，`python analyze_spectrum.py <run目录>` |
| 停训 | 用 DP 的 `stop_and_save.ps1`（它一起停 PE） |

连接方式同 DP，见 `../deep_protagonist/handoff/CONNECT.md`。


---

## ★最终状态 2026-06-07（本号最后一程，收尾）★

### 关键真相更正：不是"狩猎+通讯两堵墙"，是"空按遍布所有机制"

把全部机制的成功率量出来了（seed 50023 gen319，48 个主角）：

| 机制 | 尝试 | 成功 | 成功率 |
|---|---|---|---|
| 投掷 throw | 5115 | 1952 | **38%（唯一在工作的）** |
| 砍树 chop | 36010 | 22 | 0.06% |
| 制造 craft | 30483 | 6 | 0.02% |
| 建造完成 worksite | — | 0 | 0% |
| 狩猎 hunt | 15171 | 0 | 0% |
| 喝水 drink | — | 0 | 0% |
| 通讯响应 signal_response | 48 发 | 0 | 0% |

**根因不是缺激励**（奖励面早已拉满：worksite_completion 150 / joint_hunt 18 / signal_co 1.5 / must_build 生死门），
是**够不着**：砍树/制造按了 3-4 万次几乎全失败（跟 DP 的"空按"同型）。
具体卡点：资源太稀（600×600 世界才 80 棵树，平均间隔 ~67m，主角走不到树边才能砍）+ 工地完成要 8 次搬运，永远凑不够。
但砍树/制造/搬运是有零星成功的（22/6/26），说明链条物理上走得通，只是前置条件太难凑齐。

### 验证过的杠杆（全配置、不重编、不动现有种子）：资源密集化

A/B 新种子 **50025**（从 50002 的 gen159 热启动，**完全不碰在跑的 50021-24**）：
- 树密度 `[world.tree] count` 80 → **240**（3 倍）
- 工地完成门槛 `worksite_unlock_deposit_threshold` 8 → **4**
- 石料门槛 `stone_unlock_deposit_threshold` 6 → **3**
- 配置文件：`onepot_sD_warm_50025.toml`

**结果（仅 5 代就全面抬升，这才是"全面开花"）**：

| 指标 | 基线 g319 | 50025 g164(密集) |
|---|---|---|
| me 档案覆盖 | 4–7/25 | **10/25** |
| 最佳适应度 | 1700–2095 | **7194** |
| 砍树成功 | 22 | 169 |
| 制造成功 | 6 | 19 |
| 搬运 deposits | 26 | 53 |
| 建造完成 | 0 | 1 |
| 狩猎成功 | 0 | 6 |

→ **结论：PE 要"全面开花"，正确方向是提高"够得着性"（资源密度 + 降门槛），不是死磕狩猎/通讯墙。**
下一步（接手）：确认 50025 跑稳后，把这套 density 参数推广到所有种子；或继续抬密度/降门槛探边界。
注意：melee `hunt_radius` 是硬编码构造参数（不在 toml），要进一步降狩猎墙需重编；但资源密集化已能把多数机制抬离地板，优先走这条。

**复现**：`handoff/scripts/launch_seed.ps1`，或直接照 `onepot_sD_warm_50025.toml` 改 density 三参数后启动。
