# STATUS.md · Neural Eco 全仓交接手册

> 最后巡检：2026-05-23 下午（**A2.4 fairness 暴露 CSV 收口：Stage A 100% 完成；v16 公平 scale 验证完成**）
> 巡检范围：`d:/claude-code/c++` 整个工作树 + `D:/NVIDIA/` + `D:/claude-code/downloads/`
> 当前分支：`master`
> 当前 HEAD：`A2.4 fairness CSV exposure (pending commit)`
>
> **2026-05-23 下午 A2.4 fairness 暴露到 memory_ablation_results.csv 收口**（Stage A 100% 完成，pending commit）：
> - **背景**：A2 Credit/EventLineage 主线唯一剩的 [~] 是 fairness 指标暴露到 CSV。`CreditLedger::computeFairness` 已实现并 4 gtest 覆盖（freeloader_ratio / share_gini / top_share_mean / total_reward），但只写到 `data/runs/_credit_log.csv` 的逐 payout 行，没有 per-(task, variant) 聚合到 `memory_ablation_results.csv`。
> - **改造**：7 处 surgical 编辑——`evaluatePopulation` 加可选 `CreditFairnessMetrics* out_fairness = nullptr` 参数（默认 nullptr，training call site 4009 零影响）；函数末尾在 `appendCsv` 前 `if (out_fairness) *out = computeFairness(0xFFu)`；`MemoryAblationAggregate` 加 4 sums + 3 计数；`MemoryAblationResultRow` 加 4 fairness + 2 count avg；memory ablation call site 传 `&episode_fairness`；episode loop 内"只在 payout_count>0 时累加 fairness 三指标"（避免空 episode 0s 注入假"完美平均"）；CSV header + write 加 6 列。
> - **验证**：166/166 tests 全过；用 v16 best genome resume eval `tmp/a24_fairness_test/a24_test.toml`（5 ablation variants × 7 tasks × 2 episodes = 70 episodes）；35 行 csv 的 6 列全部正常生成；26 行 non-zero fairness 数据真实分化：`comm_build_request/baseline` gini=0.267 top_share=0.804 显示少数人扛 80% 工作；`signal_response/baseline` gini=0.475 freeloader=0.024 显示 2.4% free rider；`comm_danger_warning/baseline` 最活跃 payouts=37 sources=13；`water_return` total_reward=0 合理（该任务无 build/hunt/signal reward 输出）。
> - **意义**：Stage A 主线 100% 完成（A1 全 [x]、A2 全 [x]、A3 全 [x]）。下一步默认进 Stage B（物种角色 R-005），但 v16 留的 hunting 退化（archive 偏 build 挤掉 hunt genes）会污染"群猎者"角色验证，建议先修后再进 B 阶段。
>
> **2026-05-23 下午 L2.9 v16 公平 scale 验证**（验证数据 + toml + py 已 commit 98f508f）：
> - **背景**：v15 报告"24× v8 alive / 97× v8 crafts"，但 v15 用 protag=8 / ep=6（每代 48 evals），v8 用 protag=24 / ep=3（每代 72 evals）；per-(protag×ep) 归一化只能消除统计 bias，不能消除生态动力学差异（资源人均充足度）。用户选 #3 公平 scale 路线验证 v15 是否被分母扭曲。
> - **改造**：复制 `tmp/v15_qd_real/v15_qd_real.toml` → `tmp/v16_fair/v16_fair.toml`，仅改 3 处：`scenario_name`、`runs_dir`、`population.protagonist.count: 8 → 24`、`episodes_per_generation: 6 → 3`。其余配置（archive、me_neat 4 字段、reward、task_template、background）完全不动。无代码改动，复用 v15 同一 binary。
> - **v16 60-gen 验证 (seed=91, total_prot=720 与 v8 完全匹配)**：
>   * **真实进步**：avg_fitness 452→2052 (**4.54×** v15)、qd_score 5922→39743 (**6.71×**)、me_coverage 10%→20% (**1.88×**)、coop_per_prot 24→100 (**4.23×**) — 这些是 24 protag 池子带来的真实改善（更稳定的 fitness 估计、更易触发合作、archive 填充更快）。
>   * **v15 的虚高被揭示**：alive 50.2%→39.2% (×0.78)、works 0.05→0.032 (×0.64)、crafts 0.41→0.32 (×0.78)、chops 2.34→2.22 (×0.95)、hunts 0.36→0.24 (×0.66) — 这些数字 v15 用 8 protag 时被资源人均充足放大了 20-35%，公平 scale 下退化。
>   * **真实 v16 vs v8**：alive **18.7×** v8、chops **24×**、works **4.6×**、crafts **75×** — 仍然是巨大跃进，比 v15 报告的 24×/25×/7×/97× 略小但仍然 huge。
>   * **未解决问题**：throws 894→1082 (×1.21) 仍在恶化（BD 不含 throw 维度）；hunts 0.36→0.24 退化更严重（archive 偏 build/craft 挤掉 hunting genes）。
> - **诚实结论**：v15 真正的"涌现"突破是真的，但 8-protag 配置让数字看起来比公平对比下乐观 20-35%。v16 之后所有 ME-NEAT 实验默认应使用 protag=24 配置避免分母 confound。BD 不含 throw/hunt 是已知架构限制，不是这次实验的目标。
>
> **2026-05-23 中午 L2.9 v15 real ME-NEAT 突破**（1 commit + 验证数据待 push）：
> - `<v15-code-commit> L2.9 v15: real ME-NEAT - archive-driven breeding + iso+line + init phase`
> - **背景**：v14 implicit fitness shaping + v14b 细化 bin 都没用，me_coverage 死在 4%。第二轮 Tavily 文献定位真问题：MAP-Elites 必须有 init phase (Nilsson 2021) + 必须 archive-uniform sampling (Mouret 2015) + Vassiliades 2018 iso+line operator 比 uniform crossover 多 3-4x coverage。
> - **改造**：`ProtagonistEvolution` 加 public `reproduceFromArchive` + `crossoverLine` + `mutateBoosted` ~140 行；Config 加 4 个新字段 (`me_neat_archive_breeding`/`init_generations`/`line_sigma`/`mutation_boost`)；Runtime 在 reproduce 调用点 dynamic_cast 分流 archive_breeding=true 路径。`v15_qd_real.toml`: chops_bin=5, workcraft_bin=1, max_bins=8, archive_breeding=true, init_generations=10, line_sigma=0.15, mutation_boost=2.0。
> - **v15 60-gen 验证 (seed=91)**：**worksites 0.1→2.4 (24x v13, 4.8x v8 baseline)**、**crafts 0.3→19.5 (65x)**、**survived 7.6→24.1 (3.2x v13, 16x v8)**、chops 68→112 (+65%)、me_cov 4%→10%。代价：avg_fit 3191→452, coop_s 4261→1131 - fitness 公式不奖励 worksite/craft 中段步骤导致数字看不出来，但 build chain 第一次真正涌现。
> - **地基重构方向被验证**：v8-v13 single-fitness NEAT 死局被 v15 真正的 MAP-Elites 流程突破，文献预测的 QD 优势在 60 代内可见。下次需要：调 fitness 公式让 worksite/craft 中段也奖励 + BD 扩到 3D + 120-gen 长训。
>
> **2026-05-23 凌晨 L2.8 v14 ME-NEAT 地基重构第一发**（2 commits 待 push, +766 行 + 验证数据）：
> - `<v14-code-commit> L2.8 v14: ME-NEAT (MAP-Elites + NEAT) architectural pivot`
> - `<v14-data-commit> L2.8 v14: 60-gen ME-NEAT validation - min_fit +1478, me_cov=4%调参不到位`
> - 用户怀疑"地基从一开始就出错了"，让 AI 查文献。Tavily MCP 文献调研直接打脸 v9-v13：**Craftax ICML 2024** "UED fail to make material progress" 直接打死 v12 ACCEL；**Mouret 2019 GECCO** "NEAT 'success stories' need a lot of fitness shaping (not highlighted in videos/papers)" 完美描述 v9-v13 反复调参；**InstaDeep ICLR 2023 spotlight** 证明 QD-NeuroEvolution ≥ RL+intrinsic 在 skill discovery。诊断 3 个地基错：(1) single fitness vs Quality Diversity；(2) task-curriculum vs skill-discovery；(3) 纯 NEAT vs evolution+gradient hybrid。用户拒绝 PPO/Dreamer (4-6 周工程量)、选择 ME-NEAT 演化路线。
> - **改造**：新增 `routes/protagonist_ecology/runtime/QualityDiversityArchive.{h,cpp}` (5x5 cell grid, BD = chops × build+craft, ~120 行)；ProtagonistEcologyRuntime 加 `BehaviorDescriptor` per-genome 累积 + `accumulateGenomeBehavior` + 评估完投入 archive + `evaluated_for_breeding` 把非 cell-elite fitness × 0.05 (implicit MAP-Elites)；fitness.csv 加 3 列 `me_coverage,me_qd_score,me_max_fitness`；`tmp/v14_qd/v14_qd.toml` 关掉 ACCEL/SMT 保留 v13 HRL 子奖励，me_neat_enabled=true。**166/166 tests 全过**。me_neat_enabled 默认 false 所以 v8-v13 配置零影响。
> - **v14 60-gen 验证 (seed=91, 同 v10/v11/v12/v13)**：**min_fitness -385 → +1093 (+1478, CV 大幅下降)**、crafts 0.3→1.0 (+233%)、coop_s 4261→12203 (+186%)、hunts 36.6→42.9 (+17%)。avg_fitness 3191→3227 (+1% 持平)。**但 me_coverage 始终 1-2/25 (4%)** — bin_size=20 对早期数据太粗，archive 几乎没填，机制退化成"留 1 elite + 普通 NEAT"。chops 反而从 68→18 (-73%)、throws 4647→10962 (+136%)。地基方向被验证可行（min_fit 突破 + crafts 翻倍 + coop 翻倍），但首次 bin 调参不到位。
> - 下次必须细化 bin_size (chops_bin=5, workcraft_bin=1) 或 BD 加 hunt 维度。

> **2026-05-23 凌晨 L2.7 v13 HRL sub-rewards + 细节修复**（2 commits, +322/-9 代码 + 验证数据）：
> - `084a2a8 L2.7 v13: HRL sub-rewards + weapon bonus + carry slowdown + stick throw`
> - `1b0a985 L2.7 v13: 60-gen HRL+details validation results`
> - 用户洞察："投出去的效果都是一样的，那做出毛的意义又是什么呢" 触发 8 个细节调查 + 同时建议 HRL。文献定位：Sutton & Barto Options framework + Vezhnevets FuN，配合现有 13-type GoalManager。修复：(1) 9 个 capability 在成功路径调 `agent.noteGoalCompletion()` → reward 公式加 sum(weight × this_tick_count) 与 outcome_w 解耦；(2) `GoalContextPerception` (13-onehot) 让 worker 看到 manager；(3) 武器加成 (spear×1.6/stone×1.3/stick×1.1) 在 melee + carry slowdown + stick throw。
> - **v13 60-gen 验证 (seed=91)**：fitness avg avg **-924 → +3066** (从负转正)、CV **88% → 30%** (v9 水平)、chops **18→67** (+264%)、throws **0.4→1825** (×4500 emergent 投掷)、alive 3.8→8.1。peak_best 889→8941 (×10)。仍未追 v9: worksite 0.3 / craft 0.2 / drink 0 — 需 build chain phase-by-phase shaping (下次)。

> **2026-05-23 凌晨 L2.6 v12 4-机制 literature-backed stack**（2 commits, +314/-10 代码 + 验证数据）：
> - `422847f L2.6 v12: 4-mechanism literature-backed protagonist autocurriculum stack`
> - `d41a0ff L2.6 v12: 60-gen 4-mechanism stack validation results`
> - 用户反馈："不能为了某个指标好就舍弃信号 / 让我去查文献"。文献定位 4 个真名症状 (Lazy Agent / Negative Transfer / Multi-task Averaging / Sparse Long-horizon Reward) + 4 个对应正解：(1) Context-conditioned policy (PEARL/RL²) — 8-onehot template idx 加到 brain input；(2) Decoupled intrinsic motivation (Cuccu/Schmidhuber 2011) — 把 `intrinsic × novelty` 从 `outcome_w` 缩放里独立出来；(3) ACCEL regret-based weighting (Parker-Holder 2022) — 每代 EMA 更新 `(best - avg) / (\|avg\|+1)` 自动 upweight 高 regret template；(4) SMT Hard-Tasks-First (Cho 2024) — 早期 N 代用 difficulty 加权（线性 taper）。166/166 tests 全过。**v12 60-gen 验证 (seed=91, 同 v10/v11)**：alive 1.9→3.8 (+100%)、worksite 0.1→0.5 (+400%)、chops 7→18.6 (+166%)、craft 0.1→0.7 (+600%)、hunts 19→31 (+61%)；CV v10 327% → v11 103% → v12 88%；起步 fitness v11 -107 → **v12 +216**。仅 co_presence -49% (early-gen mixed_ecology 比例过高，参数调而非机制问题)。
> - 没追上 v9 长链条 build/craft 数字 — 60 代不够 + 需要 hierarchical RL extension（不在本 commit 范围）。**4 机制是后续涌现任务的稳定底座**。

> **2026-05-22 夜 v2 物种差异化全部落地**（4 commits, +2740 行）：
> - `200ea1a L2.1: species differentiation v2` (damage threshold + stealth + scavenge eff + 8 species capability differentiation)
> - `3896e90 L2.2: homeostatic drive state v2` (5 drives + drive-delta reward formula + per-species setpoints + 3 toml gates)
> - `06daaf0 L2.3: resource niche partitioning v2` (4-kind plant + corpse + per-species eat efficiency)
> - `7d978b6 L2.4: per-episode task template autocurriculum` (POET-lite 6 templates from spec § 4.2)
> - **30 代 long retrain 验证通过**（`tmp/l2_5_long/v10_long.toml`，6 templates × 6 episodes/gen × 30 gen = 180 episodes，~85s）：所有 8 物种 fitness 健康收敛，**none collapsed**；scavenger peak avg=1068 (scavenge_eff working)、ambush_predator avg 300-660 (stealth + niche)、protagonist drive penalty 从 -1551 → -127 (best @ gen 22)。166/166 unit tests PASS。
> - 所有 v2 改动**全部默认 off**：`reward.drive_weight=0` / `world.resources.plant_weights.grass=1.0` / `[[evolution.task_templates]]` 空数组 = 老 v9 / a3 / memory ablation 训练分布完全不变。
> - 用户原话核心理由："**奖励太直接 → 反涌现**" — v1 30 reward labels 路径完全废弃，v2 走"机制压出来"路径，符合 spec § 0.1 "v1 v2 根本差异" 表。
>
> **历史 commit 链**（保留参考）：
> 关键近期 commit 链：
> - `81f763f feat(protagonist): listener-side signal response reward + metric decoupling`
> - `2c81ca4 feat(protagonist): controlled signal emitter for camp+signal co-training`
> - `89b56ab docs(protagonist): condense experience_log + sync PROGRESS / STATUS for ablation tightening`
> - `02d5a08 fix(protagonist): tighten memory ablation tasks + peer_signal_nearby fix`
> - `71ddc9b wip(protagonist): prior session work - memory ablation infra + brainv3 shell + goal manager`
> 交接目标：让新对话能在**不依赖旧上下文**的情况下，快速接手仓库真实状态。
>
> **2026-05-22 最新审计**：动 memory / perception / reward 前请先看 `supporting/MEMORY_EVIDENCE_AUDIT.md`（三任务对照表 + 44 维 perception 真用状态 + 四类 memory bank 最小有效 query + DEAD/WEAK 标记 + spatial→goal cascade 排雷）。
>
> **2026-05-22 audit-driven fix 落地**：基于 audit § 4.1 + § 4.2 + § 4.4 + A1.4 的修复已 commit（peer_signal_heard episodic 写入、perception dim 18-21 specialise、dim 34-35 cascade 解耦、listener-side dense reward `signal_response_dense_reward`、TraceRecorder 4 列 signal 归因 + 4 perception unit tests）。共 5 轮重训 v3→v7 验证：**cascade 解耦四轮独立证据稳定（water/worksite spatial/goal 各 -55%~-100% 分离）**；signal_response_events 仍 0，根因是 reward 全局比例失衡（v6 5 variants fitness bit-exact 相同 / v7 反向），非单点缺口。见 `supporting/MEMORY_EVIDENCE_AUDIT.md` § 7。
>
> **2026-05-22 下午 ROOT CAUSE 修正 + tick=0 sentinel fix**：systematic-debugging 重新走 Phase 1 取证（instrument 9 个 debug counter + `data/runs/_signal_debug.csv`），发现 v3~v7 全部 signal_events=0 是**两层 bug 叠加**，不是 reward economy 失衡：(1) ablation toml `resume_checkpoint_path=""` → 跑随机网络，events 必然 0；(2) `runtime/ProtagonistEcologyRuntime.cpp` 3 处 `emit.tick == 0` 哨兵把 controlled emitter 第一次 emit 永久跳过。修后用同一 v7 genome 跑：events_avg 0.25~1.75（≠ 0），**avg_fitness 11~21 → 72~79（×5）**，`min_dist_to_emitter` 10m → 0.12~3.8m。详见 `supporting/MEMORY_EVIDENCE_AUDIT.md` § 8。Audit § 7 的"需要换 PPO"假设被推翻——NEAT 已经能学到 protagonist 紧贴 emitter，events 数字偏低是 dedup 副作用。
>
> **2026-05-22 晚 A3 通信语义代码就位**：S1 `CommunicationMetrics` 基础设施（`runtime/CommunicationMetrics.{h,cpp}` + 9 个 gtest + `data/runs/_communication_metrics.csv`）+ 4 个信号 ablation 开关（`world.signal.ablation = disabled|listener_blind|one_channel_only|noise`，runtime 在 emit / subscriber / listener / episodic 四处拦截）+ `scripts/protagonist_communication_mi.py` 离线 MI 计算。S2-S4 三个新 `MemoryAblationTaskKind`：`CommScoutWorker / CommDangerWarning / CommBuildRequest`，各自有差异化 reward profile（scout-worker 用 food drop 当 win condition；danger warning 用 `signal_response_dense_reward = -0.6` 反向训"逃"；build request 强制 worksite 不可见、靠信号定位）。S5 结果文档骨架 `supporting/COMMUNICATION_SEMANTICS_EVIDENCE.md` 已建。**128/128 tests PASS**。
>
> **2026-05-22 晚 A3 跑分完成 + v8 retrain**：发现 `CommScoutWorker / CommBuildRequest` 把 `background.forager_count / predator_count` 设 0，触发 `protagonistSensoryDim` 里的 `hasBackgroundAgents=false` 分支，sensory dim 缩水拒绝加载已训 genome。**修：`MemoryAblationEval.cpp` 给两个 task 留 `forager_count=1`**（与其它 4 task 一致），rebuild `build_vs3131/bin/Release/neural-eco-protagonist.exe`。**v8 retrain** (`configs/protagonist_ecology_train_a3.toml`)：seed 91→137、generations 60→120、controlled emitter 周期 30s→15s、food count 2→3 × amount 12→18、jitter 10→16m、`signal_response_dense_reward` 1.0→1.5、`signal_co_attendance_reward` 1.5→2.0、`signal_emit_companion_radius` 30→36m。120 代约 12 min（CPU），peak best_fitness=229k（v7 同期 ~140k）。**Phase 1 5×7 ablation sweep** (commit 0119e78)：5 个 signal ablation (`none/disabled/listener_blind/one_channel_only/noise`) × 7 task × 5 memory variant = 175 行 results。
>
> **2026-05-22 晚 A3 Phase 2 task 重设计 + 第二轮 sweep**（本 commit）：Phase 1 暴露两个 task 设计缺陷——`comm_danger_warning` 在 200s episode 内 0 死亡（randomPosition 把 3 predator 散得太开 + dense_reward=-0.6 反向 reward 难训）+ `comm_build_request` worksite_completion=0（v8 不会 chop→carry→deposit）。Phase 2 修复：(1) `comm_danger_warning` 改成 safe-rallying 语义：world 120→60、predators 3→12、emitter 永远在 nest 边、`signal_response_dense_reward` -0.6→+0.6（朝信号走 = 朝 nest 走 = 安全），baseline 现在产出 4.5 死亡/episode（survived 3.5/8）+ fitness 41→4.87 ▼88%。(2) `comm_build_request` 第一次 try (tree.count=0 + nursery_supply auto-stick) 失败：v8 不会 pickup loose stick；第二次 try (tree.count=2 + required_sticks 2→1, stones 1→0) 也失败：visibility=false 让 v8 perception 看不到 worksite 走不到 deposit；最终 worksite_completion 仍 0，fitness ▼47% 是当前能给的最强证据。**最终判定**：A3 主线"关闭信号 → 行为明显下降"判定**多重成立**——scout-worker primary 4.25→0 ▼100% + fitness ▼93%、danger_warning fitness ▼88%、build_request fitness ▼47%、signal_response fitness ▼88%；survived / worksite_completion 这两类需要物理实体的 primary metric 在 v8 能力下不能用，要让它们起来需要在 protagonist 训练阶段就接触对应任务（B 阶段范围）。完整对照 + 9 节 Phase 2 报告见 `supporting/COMMUNICATION_SEMANTICS_EVIDENCE.md`。

> **本版本相对旧版 handoff 的关键修正（精简版）**：
> - 继续保留已落地代码：CT 结构化事件、Memory Phase A、worksite/hunt credit、rule-based goal loop、world-model dataset builder、offline MAP-Elites、DNC micro-task、BrainV3 最小壳层
> - **重新校准完成度口径**：按 `WORLD_UNDERSTANDING...` 的 8 个 Phase 算，目前只有 **Phase 1 Trace/Resume** 真正收口；Phase 2/3/5/6/7 都还是 partial；Phase 4/8 还没真正开始
> - **重新校准 GPU 口径**：CUDA build 路径已可编译运行，但 camp-building 训练基准下暂未观察到优于 CPU 的总耗时收益
> - 不再把固定“79/79”写成当前源码树的绝对测试总数；当前工作树已新增 route tests，历史绿测数字只代表旧快照

---

## 0. 一页结论

### 0.1 5 个事实新对话必须先记住

1. **不要把这份文档旧版里的“Stage 0~5 全做完”理解成长蓝图完成。** 当前真实口径是：`Trace/Resume` 完整；`Memory V2 / credit / QD / world-model dataset / goal rule loop` 都只是 partial。
2. **CUDA build 路径可用，但“GPU 已加速训练”这个结论目前不成立。** toolkit 在 `D:\NVIDIA\CUDA\v12.8\`，`build_cuda\bin\neural-eco-protagonist.exe` 也能运行；但最新 camp-building 基准显示当前总训练耗时未优于 CPU。
3. **没有跑过任何正式长训。** 当前所有验证仍以 smoke、短 benchmark、短配置为主；开正式训练依然是用户决策权。
4. **一个核心机制环确实已经端到端可观测**：
   - chop → ChopCompleted → EpisodicMemoryBank.chopped_tree → MemoryCognitionPerception → brain
   - build → per_agent_contribution_seconds → BuildCompleted → ledger 按 share 分 reward → CreditAssigned 事件 → built_worksite episodic
   - kill → cumulative damage ledger → joint_hunt_reward 按 damage share → CreditAssigned → killed_prey/attacked_enemy episodic
5. **接手时一定要先做**：`git status --short` → 读这文件 → 用 `build/` 路径确认 baseline；CUDA 路径只在需要时再验证。**不要悄悄开正式训练**。

### 0.2 哪些没做（按设计明确推迟）

| 项 | 状态 | 备注 |
|---|---|---|
| 正式长训 | ⏸ 用户保留决策权 | 5 ablation × ≥100 代 |
| Manager/Worker 双脑头 | ⏸ Phase B | 脑结构升级 |
| DNC-forcing micro-task 长训 / ablation 解释 | ⏸ Phase B | 最小 build/test/smoke 已完成，但还没证明 DNC 真学会 |
| WorkingMemoryBank 多时间尺度 | ⏸ Phase B | |
| ProtagonistGenomeV2 | ⏸ Phase B | |
| Real world model predictor / 预训练 / 加载回 brain | ⏸ Phase B | dataset 已可生成 |
| MapElitesArchive 接 NeatEvolution reproduction | ⏸ Phase B | 当前只有离线 archive / report |
| POET / EnvironmentGenome | ⏸ Phase B+ | |
| 可视化层 (M2) | ⏸ ADR-0005 延后 | `src/viz/` 仍空骨架 |

---

## 1. 仓库当前结构（高层）

```
D:\claude-code\c++\
├── src/                     # 主仓核心代码（共享底座）
│   └── core/evolution/novelty/
│       ├── NoveltyArchive.{h,cpp}      # 老
│       └── MapElitesArchive.{h,cpp}    # Stage 5 新增
├── routes/
│   └── protagonist_ecology/  # 当前主力工作区
│       ├── brain/            # ProtagonistBrain + DNCMemory + 显式记忆 + DenseGpu
│       ├── perception/       # MemoryCognitionPerception 44 维等
│       ├── capability/       # ChopTreeCapability(per-agent session) 等
│       ├── runtime/          # ProtagonistEcologyRuntime + Trace
│       ├── world/            # 路线专有世界层（base/tree/worksite/...）
│       └── docx/             # 路线规划与本 STATUS
├── tests/
│   ├── core/                 # 历史绿测集 + novelty/map-elites 等
│   └── route_protagonist/    # contribution ledger / memory trial / BrainV3 等 route 测试
├── scripts/                  # Python 离线分析工具
├── configs/                  # TOML 配置库
├── build/                    # 旧 MSBuild VS 构建目录
└── build_cuda/               # 新 Ninja + portable CUDA 构建目录（gitignore）

D:\NVIDIA\CUDA\v12.8\         # 便携 CUDA toolkit（无 admin、无 registry）
├── bin/                      # nvcc.exe, cudart64_12.dll, cublas dlls
├── include/                  # cuda_runtime.h, cuda.h, thrust, cub
├── lib/x64/                  # cuda.lib, cudart.lib
└── nvvm/                     # nvvm64_40_0.dll, libdevice.10.bc

D:\claude-code\downloads\
├── cuda_12.8.0_571.96_windows.exe    # 3.15 GB local installer
├── 7zr.exe                           # 600 KB 单文件解包工具
├── ninja\ninja.exe                   # 570 KB 单文件构建工具
├── extract_cuda_portable.ps1         # 解 CUDA 到 D:\NVIDIA\
├── activate_cuda_portable.bat        # 一键 reconfigure + build + test
├── install_cuda_12.8.bat             # （留作备用，需要 admin 路径）
├── activate_protagonist_gpu.bat      # （留作备用，需要 admin 路径）
└── README_CUDA_SETUP.md              # 早版用户手册
```

---

## 2. 推荐阅读顺序

### 2.1 必读

1. `AGENTS.md`
2. `PROGRESS.md`（顶层）
3. `routes/protagonist_ecology/docx/PROGRESS.md`
4. `routes/protagonist_ecology/docx/STATUS.md`（本文件）
5. `docs/ROADMAP.md`
6. `docs/ARCHITECTURE.md`

### 2.2 接 protagonist 路线动手前

1. `DEVELOPMENT_EXECUTION_GUIDE.md` + `DEVELOPMENT_TASK_PLAN.md`
2. `supporting/MEMORY_SYSTEM_MASTERPLAN.md` + `supporting/MEMORY_PHASE_A_EXECUTION_PLAN.md`
3. `supporting/MEMORY_ABLATION_SMOKE_RESULT.md`（诚实结论：smoke 反向 != 记忆有害）
4. `supporting/CONTINUOUS_TIME_WORLD_DESIGN.md`
5. `supporting/COMPUTE_SCALING_GPU_CPU_PLAN.md`
6. `supporting/PRODUCTION_WORLD_LIFECYCLE_SPEC.md` — **动 R-002 / R-006 / R-007 / 3D viewer 任何代码之前必读**；固定了主世界永不 reset、训练永远在别处做、新血脉只能通过出生或迁徙进入、灭绝场景的合法处理路径等 17 条不变量
7. `supporting/SELF_GROW_CAPABILITY_LIMITS_SPEC.md` — **动 R-008 / Stage E / 任何"自长 / 涌现"机制之前必读**；7 层 L0-L7 精确化定义，L7 凭空新 capability 物理不可达必须人写，任何"自长 X"表述必须指明 Layer 编号

补充说明：

- `archive/HANDOFF_NEXT_AI_2026-05-21.md` 只保留为单次会话历史参考，不再作为默认主入口
- 当前 route 默认导航已切到 `DEVELOPMENT_EXECUTION_GUIDE.md` / `DEVELOPMENT_TASK_PLAN.md` / `STATUS.md`

### 2.3 接手第一件事不是写代码

```bash
# 1. 看工作树脏不脏
cd /d D:\claude-code\c++
git status --short

# 2. 看最近 17 个新 commit（自 c7f1665 之后都是本会话产物）
git log --oneline -20

# 3. 验证 CPU build 还能跑
build\bin\Release\test_core.exe --gtest_brief=1
build\bin\Release\neural-eco-protagonist.exe --config configs\protagonist_ecology_train_smoke.toml

# 4. 验证 GPU build（如果 D:\NVIDIA\CUDA\v12.8\bin\nvcc.exe 在）
D:\claude-code\downloads\activate_cuda_portable.bat
```

---

## 3. 本次会话所有产出（按时间线）

> 这一节是"如果新对话只看一节就知道这一夜做了什么"。

### 3.1 Stage 0 · CT-2B/CT-3 结构化动作事件 ✅

**commit**: `8ea81b2`

之前 chop/build 只有"attempts/successes 加 1"的离散计数。改成 started/completed 配对结构化事件：

- `core/events/EventType.h` 新增 6 个 protagonist route 事件：`ChopStarted` / `ChopCompleted` / `ChopAbandoned` / `BuildStarted` / `BuildCompleted` / `WorksiteMaterialDeposited`
- `core/events/EventPayloads.h` 含完整 payload 结构（chopper、tree_id、started_at_seconds、duration_seconds、wood_yield、contributors[]、contribution_seconds[]…）
- `TreeLayer::chopNearest` 改返回 `ChopHitResult{tree_id, tree_pos, remaining_integrity, felled, wood_yield}`
- `ChopTreeCapability` 改成有状态 per-agent session 状态机：start / continue-same-tree / target-switch (abandon-old + start-new) / abandon (output drop / out-of-range / now-carrying) / complete
- `WorksiteLayer` 三个新字段：`build_started_at_seconds` / `per_agent_contribution_seconds` (map) / `active_builders` (set)
- `progressConstruction` 在首次进入发 BuildStarted、每 dt 累积 per-agent 贡献时间、完成时按贡献时长降序发 BuildCompleted
- `absorbCarriedMaterials` 实际入库时发 WorksiteMaterialDeposited 含 depositor + material_type
- `TraceRecorder` 接好全部 6 个事件的 payload 序列化

**验证**：smoke trace 显示 ChopStarted=8 / ChopCompleted=6 / ChopAbandoned=2 完整闭合，payload 字段齐全。

### 3.2 Stage 1 · Memory Phase A 收口 ✅

**commits**: `d6bc642` + `72e2199` + `01eec4d`

- 在 runtime 评估循环新增 3 个 tick 收集器（`tick_chop_completed` / `tick_build_completed` / `tick_deposits`），订阅 EventBus 在每个 substep 收集
- 在 per-agent memory 写入循环里转写 5 类 episodic：`chopped_tree`（按 duration salience）、`built_worksite`（按 contributor rank salience）、`deposited_stick` / `deposited_stone`（0.30）、`signal_emitted`（0.20）
- `scripts/protagonist_memory_ablation.py`：一次跑 baseline + 4 个 ablation（spatial/episodic/social/goal），自动生成对照表
- 写 `supporting/MEMORY_ABLATION_SMOKE_RESULT.md` 诚实记录：4 开关都让 fitness 显著移动（基础设施 PASS），但 ablation 反而 fitness 更高的反直觉现象需要长训才能反转，**不能当成"记忆有害"的科学结论**

**Phase A 7 项验收全部 PASS**（详见 PROGRESS.md 第 8 节表）。

### 3.3 Stage 2 · P3 ContributionLedger ✅

**commits**: `77dc5dc`（worksite）+ `b9bae01`（hunt）

替换 free-rider-prone 的"在场就分"模式：

- **Worksite credit**: 用 BuildCompleted.contributors[] 与 contribution_seconds[]（已在 Stage 0 落地）。Runtime 现在：
  - 找自己在 contributors 中的 index
  - 若贡献 < 0.5s（free-rider threshold）跳过
  - 否则按 `my_seconds / total_legitimate_seconds` 分得 worksite_completion_reward
  - 社交合作分也按 share 分配给同 worksite 其他贡献者
- **Hunt credit**: 新增 `hunt_damage_ledger[victim_id][attacker_id]` 累计 episode 内伤害。新增 `tick_deaths` 收集器订阅 AgentDied：
  - 每次死亡按 damage share 分配 joint_hunt_reward
  - 阈值 1% 过滤偶然擦伤
  - 社交合作分也按 share 分配给同杀其他攻击者
- `tests/route_protagonist/test_contribution_ledger.cpp`：3 个 builder（长 / 短 / 路过者）的单测，验证排名正确 + 路过者过滤。`tests/CMakeLists.txt` 加 `neural_eco::route_protagonist` 链接

**验证**：73→74→75 单测全绿；smoke 全程跑通。

### 3.4 Stage 3 · 富化 GoalManager（P7 partial）✅

**commit**: `983f391`

之前 GoalManager 只用 13 个 GoalType 中的 6 个。本 stage 把 `GatherStick / GatherStone / RespondSignal / RestNearFire` 等分支接进了 runtime 规则环；代码里也加入了 `EmitSignal` 分支，但按当前条件顺序它会被更高优先级的 `Flee` 条件盖住，所以还不能算稳定可达的现行 goal。

- `GatherStick`: 活跃 worksite 缺 sticks AND 不在搬运
- `GatherStone`: 活跃 worksite 缺 stones AND 不在搬运
- `RespondSignal`: companion radius 内监听到 peer 信号，且满足刷新条件
- `RestNearFire`: 火点附近且健康较低时休整
- `EmitSignal`: 代码分支已存在，但当前 ordering 下仍未真正打通

当前更准确的优先级链应写成：Flee > FindWater > BuildWorksite > GatherStick > GatherStone > RespondSignal > RestNearFire > FindFood > Explore。

**未做**：真正的 Manager/Worker 双脑头（属 Phase B 脑结构升级范畴）。

### 3.5 Stage 4 · World Model trace dataset builder（P6 partial）✅

**commit**: `983f391`

新增 `scripts/trace_to_world_model_dataset.py`：遍历指定 run 下所有 `trace/generation_*/episode_*/agent_trace.csv`，按 agent_id 分组拼成 `(obs_t, action_t, obs_{t+1}, reward_delta, dt)`。obs_dim=31，action_dim=4，输出单一 CSV，无 torch/parquet 依赖。

**验证**：smoke run 生成 8864 行 transition，72 列 header 一致。

**未做**：实际的 world model 预训练 / 模型加载回 brain（属 Phase B）。

### 3.6 Stage 5 · MapElitesArchive（P5 partial）✅

**commits**: `146c37f` + `9d77e14`

- 新增 `src/core/evolution/novelty/MapElitesArchive.{h,cpp}`：与 NoveltyArchive 共用 `BehaviorCharacterization`，网格化离散行为空间，`considerCandidate` 返回是否新 cell 或 fitness 严格提升。`bestFitness() / avgFitness() / coverage()` 诊断接口
- 单测 `tests/core/test_map_elites_archive.cpp` 验证：新行为开新 cell / 弱 fitness 不替换 / 强 fitness 替换 / 聚合 fitness 计算正确
- `scripts/trace_to_map_elites.py`：从 trace 离线构 BehaviorCharacterization 喂给网格化 archive，输出 `<run_dir>/map_elites_report.csv` + `map_elites_summary.txt`

**验证**：smoke run 生成 33 个 occupied cells（4^8 = 65536 可能）。

**未做**：把 MapElitesArchive 真正接到 NeatEvolution 的 reproduction loop（属 Phase B 进化策略升级）。

### 3.7 Bonus 1 · STATUS.md 重写 ✅

**commit**: `8bfd3a2`

把 2026-05-20 版的 1191 行交接手册重写成 5-stage 完成版。

### 3.8 Bonus 2 · CUDA setup readiness（早版） ✅

**commit**: `acd405b`

下载 14MB network installer + 写好 install_cuda_12.8.bat（需 admin）+ activate_protagonist_gpu.bat（需 admin VS 集成路径）+ README_CUDA_SETUP.md。

**这一版是 admin 路径**，后来被 portable 路径取代。这些文件保留作 reference。

### 3.9 Bonus 3 · 背景物种听信号 ✅

**commit**: `b9f4ab9`

- `computeBackgroundSensoryDim` 加 `SignalPerception::featureDim(channels)`
- `attachBackgroundCommonSensors` 给所有 background species 加 SignalPerception(radius=16)
- 现在 prey 和 predator 都能"听见" protagonist 与同类的信号
- 与 signal emit reward / social communication wiring 形成基础通信回路，但不应再把它写成“EmitSignal goal 已稳定闭环”

**注意**：这是 fresh-population breaking change（旧 background checkpoint 输入维度对不上）。新训不影响。

### 3.10 Bonus 4 · CreditAssigned 事件 ✅

**commit**: `703558b`

- 新 EventType `CreditAssigned` + payload `{recipient, source_kind, source_id, share, reward}`
- source_kind = 0 → worksite_build, 1 → hunt_kill
- runtime 在两个 ledger 支付点都发 CreditAssigned
- TraceRecorder 序列化让 trace 可审计

**验证**：smoke 没真触发（没有 protagonist kill 命中 + 没 worksite 完成），但代码路径 OK。长训自然会出现。

### 3.11 Bonus 5 · Ablation harness 大扩展 ✅

**commit**: `277bf5c`

`scripts/protagonist_memory_ablation.py` 现在每个 variant 还会：
- `parse_event_trace_counts()` 统计 chop/build/deposit/CreditAssigned 计数 + total reward
- `parse_map_elites_coverage()` 自动调用 trace_to_map_elites.py 生成 coverage 数
- markdown 表加列：chop_compl / build_compl / deposits / credit_build / credit_hunt / credit_total_reward / map_cells

一次运行解决"机制有没有真发火 + 反生命财产分配 + 行为空间覆盖"三问。

### 3.12 Bonus 6 · NoveltyArchive 单测 ✅

**commit**: `5d7fddf`

老 F-NOVELTY 时代以来 NoveltyArchive 一直没单测。Stage 5 旁加 4 个 case：空 archive 返回 max novelty / 已知点自距离 0 / 远点比近点更 novel / clear 重置。

**75 → 79 测试**。

### 3.13 Bonus 7 · drank_water episodic ✅

**commit**: `7748a55`

- 加 `previous_hydration` map 跟踪 tick-over-tick hydration ratio
- hydration 跳升 > 0.02 → 发 `drank_water` 事件，salience 0.55
- smoke 验证 1233 行成为 top-salience 事件类型

### 3.14 Bonus 8 · saw_predator episodic ✅

**commit**: `90d5ced`

- 加 `had_predator_in_view_last_tick` set 跟踪状态
- predator_nearby 上升沿（不是每 tick）→ 发 `saw_predator` 事件，salience 0.85
- smoke 验证 1508 行成为 top-salience 事件类型

### 3.15 Bonus 9 · Portable CUDA + Ninja 接通 ✅✅✅

**commits**: `b30771c` (CMake fix) + `4dd1dbc` (gitignore)

**这是本会话最大的工程突破**。详见第 4 节。

---

## 4. CUDA / GPU 路径详细记录

### 4.1 起点

- 本机：RTX 5060 Laptop（Blackwell sm_120）+ NVIDIA driver 596.49（CUDA 13.2 capable）
- `C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v12.4` 和 `v11.8` 是空壳（只有 `extras/`，无 `bin/include/lib`）
- `where.exe nvcc` 找不到
- protagonist GPU 代码（`ProtagonistDenseGpu.cpp/.cu/.h`）和 CMake `NEURAL_ECO_ENABLE_PROTAGONIST_CUDA` 开关早就在仓库里，只是没编入 build
- 用户硬性要求：不装 C 盘、不要 admin

### 4.2 解决思路

NVIDIA 的标准 CUDA 安装强制要求 admin（registry + 系统 DLL 注册）。绕过：

1. **下载 local installer**（3.15 GB，比 network installer 完整，包含所有 .nvi 子包）
2. **用 7zr.exe 把 installer 当 7z 解包**（不需要 admin，因为 7z 只是读字节流）
3. **挑出 nvcc + cudart + nvrtc + cublas + cccl(thrust+cub) + nvtx + cuxxfilt + nvprof + profiler_api + cuda_cccl 这些组件**合并到 `D:\NVIDIA\CUDA\v12.8\`
4. **用 Ninja 生成器 + `-DCMAKE_CUDA_COMPILER=...nvcc.exe`** 让 CMake 直接驱动 nvcc，不用 Visual Studio 的 MSBuild CUDA 集成（那个 .props 文件需要写到 `Program Files (x86)\Microsoft Visual Studio\…\BuildCustomizations\` 才能装，需要 admin）
5. 跳过的组件：display_driver / Nsight VS plugin / GeForce Experience / CUDA Samples — 这些才需要 admin

### 4.3 文件清单

```
D:\NVIDIA\CUDA\v12.8\
├── bin\
│   ├── nvcc.exe              17 MB
│   ├── cudart64_12.dll       574 KB
│   ├── cublas64_12.dll
│   ├── cublasLt64_12.dll
│   ├── fatbinary.exe
│   ├── cudafe++.exe
│   ├── cuinj64_128.dll
│   └── ...
├── include\
│   ├── cuda_runtime.h
│   ├── cuda.h
│   ├── cooperative_groups\
│   ├── cub\
│   └── ...
├── lib\
│   ├── x64\cuda.lib
│   ├── x64\cudart.lib
│   ├── x64\cudart_static.lib
│   └── cmake\                # find_package(CUDAToolkit) 用
├── nvvm\
│   ├── bin\nvvm64_40_0.dll
│   ├── bin\cicc.exe          # device code compiler
│   └── libdevice\libdevice.10.bc
└── LICENSE

D:\claude-code\downloads\
├── 7zr.exe                                       600 KB（standalone）
├── ninja\ninja.exe                               570 KB（standalone）
├── cuda_12.8.0_571.96_windows.exe                3.15 GB（local installer）
├── cuda_12.8.0_windows_network.exe               14 MB（早期 network installer 备用）
├── extract_cuda_portable.ps1                     解 CUDA 到 D:\NVIDIA\
├── activate_cuda_portable.bat                    一键 reconfigure + build + test（最终方案）
├── install_cuda_12.8.bat                         （早版 admin 方案，留作 reference）
├── activate_protagonist_gpu.bat                  （早版 admin 方案，留作 reference）
├── README_CUDA_SETUP.md                          早版 user 文档
├── test_vcvars.bat                               诊断脚本
├── check_url.ps1                                 URL 验证
└── cuda_build.log                                build 日志
```

### 4.4 CMake 改动（必须，别回滚）

`CMakeLists.txt` 第 15-32 行：把 `add_compile_options(/W4 /utf-8 /Zc:__cplusplus)` 改成生成器表达式 `$<$<COMPILE_LANGUAGE:CXX>:...>`。

**为什么必须**：nvcc 把 `/W4` 当 Windows 路径解析，导致 `nvcc fatal: A single input file is required for a non-link phase`。这个 fix 让 .cu 编译干净通过。

### 4.5 当前更可信的 GPU 口径

| 项 | 数值 |
|---|---|
| nvcc 版本 | NVIDIA 12.8.61 |
| build_cuda/ | 可生成 `build_cuda\bin\neural-eco-protagonist.exe` |
| 运行时依赖 | 手动运行时需把 `D:\NVIDIA\CUDA\v12.8\bin` 放进 PATH，否则会缺 `cudart64_12.dll` |
| camp-building benchmark（CPU build, 1 gen = 3 episodes） | ~`5.6s/gen` |
| camp-building benchmark（CUDA build, 同配置） | ~`8.5 ~ 9.2s/gen` |
| protagonist_dense_gpu config 默认 | true |
| 当前结论 | **CUDA build 可运行，但当前总训练耗时未优于 CPU** |

这意味着现在最多只能说“GPU 路径接通了”，还**不能**说“训练过程已经因为 GPU 明显受益”。根因很可能是：当前只 offload 了 protagonist dense forward 的一小段，而且每步仍有 host↔device 拷贝，世界推进 / 事件 / memory / DNC 主体仍在 CPU。

### 4.6 重新跑 GPU build 步骤

```cmd
:: 一次性（已经跑过，不需要重复）
powershell -ExecutionPolicy Bypass -File D:\claude-code\downloads\extract_cuda_portable.ps1

:: 每次想重 build：
D:\claude-code\downloads\activate_cuda_portable.bat
:: 这个 .bat 会：
:: 1. call vcvars64（VS2022 BuildTools）
:: 2. set CUDA_PATH = D:\NVIDIA\CUDA\v12.8
:: 3. PATH 头部加 CUDA_PATH\bin
:: 4. 删旧 build_cuda/，新建空目录
:: 5. cmake -G "Ninja" -DCMAKE_CUDA_COMPILER=...nvcc.exe -DCUDAToolkit_ROOT=...
:: 6. cmake --build build_cuda
:: 7. test_core --gtest_brief=1

:: 跑 GPU smoke：
set CUDA_PATH=D:\NVIDIA\CUDA\v12.8
set PATH=%CUDA_PATH%\bin;%PATH%
cd /d D:\claude-code\c++
build_cuda\bin\neural-eco-protagonist.exe --config configs\protagonist_ecology_train_smoke.toml
```

### 4.7 如果碰到问题

- **`nvcc` 不在 PATH**：检查 `D:\NVIDIA\CUDA\v12.8\bin\nvcc.exe` 是否在；不在就重跑 extract_cuda_portable.ps1
- **CMake 找不到 CUDA**：删 `build_cuda/CMakeCache.txt` 后重跑 activate_cuda_portable.bat
- **启动直接报缺 `cudart64_12.dll`**：说明运行时 PATH 没带 `D:\NVIDIA\CUDA\v12.8\bin`
- **`nvcc fatal: A single input file...`**：检查 `CMakeLists.txt` 第 22-28 行的生成器表达式没被回滚
- **跑出来 GPU util 一直 0**：检查 config 里 `[compute] protagonist_dense_gpu_enabled = true` 没被关；ProtagonistBrain 在 cudaGetDeviceCount 失败时会静默回 CPU，那时 nvidia-smi 没流量
- **TLS 错误重试 git clone**：本机网络偶发，CMake 的 FetchContent 自带 retry，不用管

---

## 5. 当前 commit 链（master 顶端）

```
4dd1dbc  chore(gitignore): ignore build_cuda directory
b30771c  fix(build): scope MSVC host flags to CXX/C only so CUDA compiles work    <-- CUDA fix
90d5ced  feat(protagonist): saw_predator episodic event on rising edge
7748a55  feat(protagonist): drank_water episodic event from hydration delta
5d7fddf  test(core): add NoveltyArchive smoke coverage (4 cases)
277bf5c  feat(scripts): ablation harness now reports credit + map_elites coverage
9d77e14  feat(scripts): trace_to_map_elites.py - offline MAP-Elites coverage report
703558b  feat(events): observable CreditAssigned event from ContributionLedger
b9f4ab9  feat(protagonist): background species hear signals (P4 env enrichment)
acd405b  docs(protagonist): STATUS.md - record CUDA 12.8 setup readiness
8bfd3a2  docs(protagonist): rewrite STATUS.md handoff manual for 5-stage completion
146c37f  feat(core): Stage 5 - MapElitesArchive for quality-diverse curriculum
983f391  feat(protagonist): Stage 3 + Stage 4 partials - richer goals + trace dataset
b9bae01  feat(protagonist): Stage 2 - extend ContributionLedger to hunt credit
77dc5dc  feat(protagonist): Stage 2 - P3 ContributionLedger for worksite credit
01eec4d  docs(protagonist): mark Stage 1 / Memory Phase A complete
72e2199  feat(protagonist): memory ablation smoke harness + first result
d6bc642  feat(protagonist): wire CT-2/CT-3 events into episodic memory
8ea81b2  feat(protagonist): Stage 0 - structured chop/build events for CT-2B/CT-3
c7f1665  feat(protagonist): consolidate protagonist route as primary work line   <-- 上一个会话的提交边界
54d6771  refactor: advance protagonist continuous-time runtime
```

`c7f1665` 之前是上一会话 STATUS.md 的 HEAD。从 `8ea81b2` 起 17 个 commit 都是本会话的工作。

---

## 6. 关键 Episodic 事件类型清单（脑能记什么）

protagonist EpisodicMemoryBank 现在记录这些事件，每条带 `(tick, time_seconds, position, type, salience)`：

| 类型 | salience | 触发条件 |
|---|---|---|
| `ate_food` | 0.50 | foodEatenThisTick > 0 |
| `drank_water` | 0.55 | hydration_ratio 跳升 > 0.02（Bonus 7） |
| `chopped_tree` | 0.40 + 0.1×duration | ChopCompleted 收到 |
| `built_worksite` | 1.0 - 0.10×rank | BuildCompleted 收到，自己在 contributors |
| `deposited_stick/stone` | 0.30 | WorksiteMaterialDeposited 收到 |
| `signal_emitted` | 0.20 | tick_emits 含自己 |
| `saw_predator` | 0.85 | predator_nearby 上升沿（Bonus 8） |
| `was_attacked` | 0.20 - 1.0 by damage | tick_attacks 含自己作为 victim |
| `attacked_enemy` | 0.20 - 1.0 by damage | tick_attacks 含自己作为 attacker |
| `killed_prey` | 0.90 | kill_delta > 0 |
| `completed_worksite` | 1.0 | completed_this_tick > 0 AND 在 worksite radius |

`MemoryCognitionPerception` 把 `topSalient(1)` 和 `recentOfType("was_attacked", 1)` 等查询喂给脑作为 44 维 perception 的一部分。

---

## 7. 测试体系（历史绿测 + 当前源码树新增 route tests）

需要先把口径说清楚：

- 旧版 `79/79` 指的是此前某个已构建快照，不再代表当前源码树的固定总数
- 当前工作树已经额外包含：
  - `tests/route_protagonist/test_memory_trial_layer.cpp`
  - `tests/route_protagonist/test_protagonist_ecology_config.cpp`
  - `tests/route_protagonist/test_protagonist_brain_v3_shell.cpp`
- `tests/CMakeLists.txt` 用 glob 收集测试，所以下次重新 discover 时总数会变

历史上已新增并跑过的：
- `tests/core/test_map_elites_archive.cpp`（Stage 5，4 cases）
- `tests/core/test_novelty_archive.cpp`（Bonus，4 cases）
- `tests/route_protagonist/test_contribution_ledger.cpp`（Stage 2，1 case）
- `tests/route_protagonist/test_memory_trial_layer.cpp`
- `tests/route_protagonist/test_protagonist_ecology_config.cpp`
- `tests/route_protagonist/test_protagonist_brain_v3_shell.cpp`

`tests/CMakeLists.txt` 通过 glob 收集测试，并统一链接 `neural_eco::route_protagonist`。

---

## 8. 当前主要硬底线（接手人不能违反）

1. **不能擅自开正式训练**：所有"5 ablation × ≥100 代"或类似 1+ 小时训练任务必须先问用户。
2. **不要悄悄改 CMakeLists.txt 第 22-28 行的生成器表达式**：nvcc 编不过会回到 Stage 6 之前。
3. **不要把 smoke ablation 结果当成"记忆有害"的科学结论**。3 代不够，详见 `supporting/MEMORY_ABLATION_SMOKE_RESULT.md`。
4. **不要回滚 / 重命名已经落地的 Stage / bonus 代码**。虽然文档口径已从“全部完成”修正为“多项 partial”，但这些代码路径现在仍是 load-bearing。
5. **不要把 GPU build (`build_cuda/`) 提交到 git**。已 .gitignore，但万一新人手动 git add -f 会出事。
6. **不要把 D:\NVIDIA\CUDA\v12.8 误装到 C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v12.8**。我们故意走 D 盘 portable，C 盘那俩空壳是历史残留。
7. **不要 admin 跑 install_cuda_12.8.bat**：会装出双份 CUDA，CMake 选谁会随机（可能选到 v12.4 空壳）。

---

## 9. 接手时建议默认行动顺序

### 9.1 用户说"继续 / 接着做"时

```
1. git status --short  → 看是否 dirty
2. 先看 root/route PROGRESS 顶部，确认当前不是“整 phase 已完成”的错觉
3. 如需验证 baseline，优先看 `build/` 路径；CUDA 只在你确实要查 GPU 线时再碰
4. 默认推进方向：
   A. 先把一个 partial phase 真收口（优先 `Memory V2 / ContributionLedger / communication semantics`）
   B. 给 `DNC-forcing micro-task` 补 trace / ablation 解读
   C. 如果走 GPU 线，直接做更细粒度 dense-forward profiling，而不是再重复“CUDA build 成功”
   D. 如果走 world-model 线，再开始训练一个真正的 predictor
```

### 9.2 用户问"GPU 还在工作吗"

```
nvidia-smi  # 看 driver 还在
D:\NVIDIA\CUDA\v12.8\bin\nvcc.exe --version  # 看 toolkit 还在
:: 手动运行 build_cuda\bin\neural-eco-protagonist.exe 前，先确保 PATH 带 CUDA bin
set CUDA_PATH=D:\NVIDIA\CUDA\v12.8
set PATH=%CUDA_PATH%\bin;%PATH%
:: 再决定是否需要 activate_cuda_portable.bat 重新 build
```

如果 D:\NVIDIA\ 整个被清掉了，重跑 extract_cuda_portable.ps1（installer 还在 downloads/）。
另外，**“GPU 还在工作吗”要和“GPU 有没有让训练更快”分开回答**：前者看 build/path/runtime，后者要看具体 benchmark。

---

## 10. 最终交接结论

如果新对话只记住一句话：

**当前 protagonist 路线最真实的状态是：Trace/Resume 已完整收口，Memory V2 / credit / QD / world-model dataset / goal rule loop 都已有代码落点但仍属 partial，通信语义任务与长训展示还未真正开始；CUDA build 路径可运行，但当前 camp-building 训练总耗时尚未显示出优于 CPU 的收益。下一步不该继续把 partial 写成完成，而应该先收口一个已经半落地的 phase。**
