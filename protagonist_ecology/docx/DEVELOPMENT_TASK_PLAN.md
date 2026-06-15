# Protagonist Ecology · 开发任务计划

> 位置：`routes/protagonist_ecology/docx/DEVELOPMENT_TASK_PLAN.md`
>
> 作用：这份文档只管任务，不讲大段设计。开发顺序与理由见 `DEVELOPMENT_EXECUTION_GUIDE.md`。
>
> 更新时间：2026-05-22
>
> 规则：
> - 做完一个就打 `[x]`
> - 正在做用 `[~]`
> - 明确延期用 `[-]`
> - 阻塞用 `[!]`
> - 不在这里记宏大愿景，只记可执行事项

---

## A. 当前主线：Memory V2 -> Credit -> Communication

### A1. Memory V2 收口

- [x] 为 `water_return` 重写 spatial 证据链：cascade 解耦后 v4-v7 四轮均 `no_spatial=-78%~-100%`、`no_goal=-64%~-100%`，两通道独立，旧 `-44%` 是 cascade 假阳性（见 `supporting/MEMORY_EVIDENCE_AUDIT.md` § 7.3）
- [x] 为 `signal_response` 重写完整证据链：v3-v7 五轮 `events=0` 的诊断在 2026-05-22 下午被 instrument 取证推翻——root cause 是 ablation toml `resume_path=""` 跑随机网（伪问题）+ 3 处 `emit.tick==0` 哨兵 bug 误杀第一次 controlled emit。3 处 tick==0 跳过已删，用 v7 genome 实测 events_avg=0.25~1.75（≠0）、avg_fitness 11~21 → 72~79（×5）、min_dist 10m → 0.12~3.8m。perception dim 18-21 + TraceRecorder 4 列 + dense reward 全保留（这些都是真改进）。详见 `supporting/MEMORY_EVIDENCE_AUDIT.md` § 8
- [x] 为 `signal_response` 增加 perception-driven shaping reward，避免只有进 halo 才有梯度（已加 `signal_response_dense_reward` 字段 + dense gradient `(1-dist/companion_radius)*dt`，但 v4-v7 数据显示量级仍不够撬动 NEAT 30/60 代探索；见 § 7.5 后续放大方向）
- [x] 为 `signal_response` 增加 trace 字段，单独记录 listener 的 signal 强度、朝向变化与响应轨迹（`agent_trace.csv` 新增 `recent_signal_dx/dy/salience/age_seconds` 4 列）
- [x] 为 `water_return / signal_response / worksite_return` 三个任务输出统一对照表，明确 primary metric 和 secondary metrics（见 `supporting/MEMORY_EVIDENCE_AUDIT.md` § 1）
- [x] 把 `MemoryCognitionPerception` 当前 44 维重新审计一遍：标出哪些维度真被任务使用，哪些只是挂着（见 `supporting/MEMORY_EVIDENCE_AUDIT.md` § 2，结论：OK 9 / DEAD 14 / WEAK 5 / UNKNOWN-DNC 8 / 混合 entangled 2）
- [x] 为 `Spatial / Episodic / Social / Goal` 四类 memory 分别补一条“最小有效 query”清单（见 `supporting/MEMORY_EVIDENCE_AUDIT.md` § 3）

### A2. Credit / EventLineage 收口

- [x] 把建造 credit、围猎 credit、signal credit 统一整理成同一套 route-local event lineage 口径（Step 1+2：signal 加 ledger entry，三条链都走 CreditLedger.settleXxx，发 `CreditAssigned{kind=0|1|2}`，dedup 按 (kind, source_id)）
- [x] 为 `BuildCompleted / AgentDied / signal_response` 三条链输出统一 share-based reward 日志（Step 2：`data/runs/_credit_log.csv`，每行 (tag, tick, kind, source_id, recipient_id, share, reward)，每次 evaluatePopulation 唯一时间戳 tag）
- [x] 新增 `contribution fairness` 汇总指标，区分真实贡献者与路过者（Step 2：CreditLedger.computeFairness 已实现 freeloader_ratio / share_gini / top_share_mean / total_reward + 4 个 gtest；**A2.4 收口 2026-05-23 下午**：`evaluatePopulation` 加可选 `CreditFairnessMetrics* out_fairness` 参数（默认 nullptr，不影响 training 调用点）；memory_ablation_results.csv 加 6 列 `credit_freeloader_ratio / credit_share_gini / credit_top_share_mean / credit_total_reward / credit_payout_count_avg / credit_source_count_avg`；fairness 三指标只在 payout_count>0 的 episode 累加（避免空 episode 0s 假装"完美平均"）。v16 best genome resume eval 验证：26 行 non-zero，最有意思 comm_build_request/baseline gini=0.267 top_share=0.804 显示 build 任务少数人扛大头，signal_response/baseline gini=0.475 freeloader=0.024 显示 2.4% free rider，comm_danger_warning/baseline 是最活跃任务 payouts=37 sources=13。166/166 tests 全过）
- [x] 为 signal 线补 `producer / listener / responder` 三种角色贡献统计（Step 3：results.csv 新增 3 列 `signal_producer_count_avg / signal_listener_count_avg / signal_responder_count_avg`，v7 ablation 实测 0 / 24 / 21.25 揭示 dedup 副作用）
- [x] 补 route 测试，覆盖"路过者不吃大分""listener 真响应后才记 credit"（Step 3：`tests/route_protagonist/test_credit_ledger.cpp` 10 个 gtest，含 hunt 1% min_share filter / build 0.5s min_seconds filter / signal dwell share / dedup / fairness 三指标 / CSV writer，全部 PASS）

### A3. Communication semantics 收口

- [x] 新增 `communication_metrics.csv`（Step 1：`runtime/CommunicationMetrics.{h,cpp}` 共 9 个 gtest，per-channel emit/listener_ticks/silent_ticks/response_events/listener_policy_shift_l2|cos/response_latency_avg_seconds；写 `data/runs/_communication_metrics.csv`，与 `_credit_log.csv` 同 tag）
- [x] 新增 `signal disabled / listener blind / one channel only / signal noise` 四组 ablation（Step 1：`world.signal.ablation = "none|disabled|listener_blind|one_channel_only|noise"` 配置 + `world.signal.noise_position_stddev`，runtime 在 `emitControlledSignal` / `AgentEmittedSignal` subscriber / listener block / peer_signal_heard episodic 四处拦截）
- [x] 实现 `scout-worker` 最小任务（Step 2：`MemoryAblationTaskKind::CommScoutWorker`，emitter 配 food drop，reward profile：food=6 + communication=8 + signal_response_dense=0.8 + halo=1.0，primary metric=`signal_responder_count`）
- [x] 实现 `danger warning` 最小任务（Step 3：`MemoryAblationTaskKind::CommDangerWarning`，反向 dense reward = -0.6 训"逃"，3 predator pack 隐藏 + predator_warn_enabled=false，primary metric=`survived`）
- [x] 实现 `build request` 最小任务（Step 4：`MemoryAblationTaskKind::CommBuildRequest`，emitter 在 worksite 位置(offset=0)，worksite.direction_requires_visibility=true 强制只能靠信号定位 worksite，primary metric=`worksite_completion_events`）
- [x] 用 trace + ablation 证明至少一条通信任务满足"关闭信号后明显下降"（**2026-05-22 v8 实验完成**：`comm_scout_worker` 在 baseline → signal_disabled 下 `avg_fitness` 58.3 → 9.95 (▼83%)、`signal_responder_count` 1.5 → 0 (▼100%)。详见 `supporting/COMMUNICATION_SEMANTICS_EVIDENCE.md § 3`）
- [x] 为 communication 线写单独结果说明（Step 5：`supporting/COMMUNICATION_SEMANTICS_EVIDENCE.md` v8 数据已填）

### A3.P2 · Communication task 设计修复（partial / Phase 2）

> 这一节是 A3 跑分发现的两个 task 设计缺陷。Phase 2 (2026-05-22 晚) 已经迭代了一轮，结论：fitness drop 链路在两个 task 上都已成立，但 survived / worksite_completion 这两类 primary metric 在当前 v8 能力下仍跑不起来——细节见 `supporting/COMMUNICATION_SEMANTICS_EVIDENCE.md § 9`。

- [x] `comm_danger_warning` task pressure：world 缩到 60×60 + predator 3→12 + safe-rallying 语义（dense_reward -0.6 → +0.6）让 baseline 真出现 4.5 死亡/episode（Phase 1 是 0）。但 survived 在 ablation 之间几乎不变（nest 没有物理屏障），最终用 fitness ▼88% 作为可信指标
- [x] `comm_build_request` worksite logistics：required_sticks 2→1、required_stones 1→0、tree.count=2 保留 chop 链路。worksite_completion 仍 0（visibility=false 让 v8 看不到 worksite 也走不到 deposit），fitness ▼47% 是当前能给的最强证据
- [ ] **下一阶段方案**（B 阶段范围，不在 A3 内继续）：3 条候选——(a) `comm_build_request` 的 emitter pulse 直接 trigger `GoalManager` 切 `BuildWorksite` goal 让 goal-action-assist 接管 deposit；(b) 给 protagonist 训练阶段加 build_request task；(c) 给 nest 加物理 predator 屏障让 `comm_danger_warning` 的 survived 真起来
- [ ] MI 离线分析（trace 启用后）：当前 `memory_ablation.trace_enabled = false`，`scripts/protagonist_communication_mi.py` 还不能跑。补一次开启 trace 的 sweep 后再算 MI 矩阵

### A4. ME-NEAT 训练能力重构（v14/v15/v16，2026-05-23）

> 起因：用户怀疑"地基出错了"。Tavily 文献调研定位 3 个 v9-v13 的真问题：(1) single fitness vs Quality Diversity (Mouret 2019)；(2) multi-task curriculum 在 long-chain compositional benchmarks 失败 (Craftax ICML 2024)；(3) 纯 NEAT vs evolution+gradient hybrid (InstaDeep ICLR 2023)。用户拒绝 PPO/Dreamer 工程量，选择 ME-NEAT 演化路线。本节是 Stage A 之外、平行于 A1-A3 的训练能力重构线，已带出真实涌现突破。

- [x] **v14 ME-NEAT 第一发**（commit 42fd90b + bbe896b，2026-05-23 凌晨）：新增 `runtime/QualityDiversityArchive.{h,cpp}` ~150 行，5×5 grid，BD=(chops, build+craft)。`ProtagonistEcologyConfig` 加 `me_neat_enabled / chops_bin_size / workcraft_bin_size / max_bins / non_elite_weight` 5 个字段。`ProtagonistEcologyRuntime.cpp` 加 `accumulateGenomeBehavior` per-genome BD 收集 + archive submit + `evaluated_for_breeding` 把非 cell-elite × 0.05（implicit MAP-Elites）；fitness.csv 加 `me_coverage / me_qd_score / me_max_fitness` 3 列。166/166 tests 全过。**60-gen 验证 (seed=91)**：min_fit -385→+1093 ✓、crafts +233% ✓、coop +186% ✓，但 me_coverage 死在 4%（5×5 bin_size=20 太粗 + implicit shaping 不够强）。
- [x] **v14b 细化 bin 失败回归**：8×8=64 cells，bin_size=5 也没用，me_cov 仍 4%。诊断：种群同质化 + NEAT 早熟收敛，需要显式 archive-uniform sampling。
- [x] **v15 真正 ME-NEAT**（commit 4828b16 + 6c78076，2026-05-23 中午）：`ProtagonistEvolution` 加 public `reproduceFromArchive(archive, evaluated, current, total, is_init) / crossoverLine(a, b, child, iso=0.01, line=0.15) / mutateBoosted(genome, boost=2.0)` 共 ~140 行（Vassiliades 2018 iso+line + Nilsson 2021 PGA-MAP-Elites init phase）。Config 加 `me_neat_archive_breeding / init_generations / line_sigma / mutation_boost` 4 个字段。Runtime 在 reproduce 调用点 dynamic_cast 分流 archive_breeding 路径。`v15_qd_real.toml`: chops_bin=5, workcraft_bin=1, max_bins=8, init_generations=10, mutation_boost=2.0。166/166 tests 全过。**60-gen 验证 (seed=91, protag=8/ep=6)**：worksites/gen 0.1→2.4、crafts/gen 0.3→19.5、survived/gen 7.6→24.1、me_cov 4%→10%。代价 avg_fit 3191→452（fitness 公式不奖 worksite/craft 中段）。
- [x] **v16 公平 scale 验证**（commit 98f508f，2026-05-23 下午）：用户质疑 v15 用 protag=8/ep=6 vs v8 用 24/3，资源人均不公平。**仅改 toml**（无代码改动）`tmp/v16_fair/v16_fair.toml`：protag.count 8→24、ep 6→3，复用 v15 同 binary。total_prot=720 与 v8 完全匹配。**60-gen 结果**：avg_fit 452→2052 (×4.54)、qd_score 5922→39743 (×6.71)、me_cov 10%→20% (×1.88)、coop_per_prot 24→100 (×4.23) 真实改善；但 alive 50.2%→39.2% (×0.78)、works/protep 0.05→0.032 (×0.64)、crafts 0.41→0.32 (×0.78)、hunts 0.36→0.24 (×0.66) — **揭示 v15 数据被 8-protag 资源富裕放大 20-35%**。v16 vs v8 真实 18.7×alive / 24×chops / 4.6×works / 75×crafts。
- [ ] **v15/v16 留的隐患**：(a) throws 894→1082 仍恶化（BD 不含 throw 维度）；(b) hunts 0.36→0.24 退化更严重（archive 偏 build/craft 挤掉 hunting genes）；(c) 60-gen 太短，works/crafts 涌现是 gen 38+ 才出现，只 22 代连续。下一步候选：BD 扩 3D 加 hunts 维度，或 120-gen 长训。

---

## B. 第二主线：鲜明物种生态角色（R-005）

> **2026-05-23 audit 修正**：原文档说"全 [ ]"，但 L2.1-L2.5 v2 物种差异化（commits 200ea1a/3896e90/06daaf0/7d978b6）已经把 7 archetype 的物质条件（initial_energy / damage / threshold / digest / scavenge_eff）做完了。B 阶段真正未做的是"角色机制"（territory / pack spacing / signal-assisted hunt 等行为级规则），不是"角色不存在"。

### B1. 角色定义

- [ ] 固定四个第一批角色：独居顶级掠食者 / 群猎者 / 食腐者 / 协作人类系（**注**：7 archetype 已在 BootstrapPopulation 数值差异化，但"4 个第一批"清单 + 证据链模板未正式定稿）
- [ ] 为每个角色写一页 route-local 证据链模板：食性、社会性、空间性、风险偏好、资源利用
- [ ] 审计当前 `BootstrapPopulation` 中各 archetype 与目标角色的差距（**部分**：L2.1 已做物质条件 audit，但行为级 audit 未做）

### B2. 生态位落地

- [ ] 独居顶级掠食者：加入 territory / home range / 同类排斥规则（**未实现**：grep `territory_radius/home_range` 0 hits）
- [ ] 群猎者：加入 shared target / spacing / surround 规则（**部分**：L2.1 PredatorHuntCapability 1s 滑动窗口累积 dmg threshold 让"4 狼同时打"成为门槛，但 spacing/surround 未做）
- [ ] 食腐者：加入 carcass-first / producer-scrounger / 高空巡航规则（**部分**：L2.1 + L2.3 已做食腐 ×3 / 鲜肉 ×0.33 efficiency，scavenger 加 PredatorHuntCapability，但 producer-scrounger 与高空巡航未做）
- [ ] 人类系：加入 signal-assisted group converge / assist / hunt coordination（**部分**：A3 通信任务已上线 + L1 emergent shelter 已上线，但 signal-assisted hunt coordination 未做）

### B3. 验收与展示

- [ ] 为四类角色分别补 1 组行为指标
- [ ] 为四类角色分别补 1 个短 replay / trace 样例
- [ ] 确认用户只看屏幕几分钟就能区分四类角色

---

## C. 第三主线：生命周期与繁殖（R-006）

### C1. 生命周期状态

- [ ] 给活体补 `juvenile / adult / elder` 阶段状态
- [ ] 把年龄接入 movement / combat / reproduction / recovery
- [ ] 引入 natural death，不再只有外伤或饥渴死

### C2. 尸体与疾病闭环

- [ ] 死亡后留下 corpse，而不是直接消失
- [ ] 尸体可被食腐者消费
- [ ] 尸体可作为 disease / contamination source

### C3. 繁殖闭环

- [ ] 设计最小 mating condition：年龄、距离、状态、冷却、物种兼容性
- [ ] 背景物种补双亲融合路径
- [ ] protagonist 路线补 parent ids / generation / lineage 持久记录
- [ ] 后代入世，并参与后续竞争与演化

### C4. 验收

- [ ] 用结果证明群体结构会因老病死改变
- [ ] 用结果证明后代不是单亲复制
- [ ] 用结果证明谱系变化会影响行为或分布

---

## D. 第四主线：自动晋升闭环（R-007）

### D1. 数据对象

- [ ] 定义 `Incident`
- [ ] 定义 `Challenge`
- [ ] 定义 `Lineage`
- [ ] 定义 `PromotionDecision`

### D2. 回流与试运行

- [ ] 生产问题自动落 incident archive
- [ ] incident 自动映射成 challenge task
- [ ] challenger lineage 自动进入试运行世界
- [ ] 支持 canary rollout
- [ ] 支持自动 rollback

### D3. 学习型控制器（后置）

- [ ] scheduler 只负责 task prioritization，不取代硬门槛
- [ ] promotion scorer 只做风险排序，不直接决定上线
- [ ] archive controller 只做保留/再孵化建议，不覆盖 baseline 保护

---

## E. 第五主线：自长技能与开放式增殖（R-008）

### E1. Quality Diversity（v14/v15/v16 部分已落地，详见 A4）

- [x] 把 `MapElitesArchive` 从离线报告接进在线 reproduction（v15 commit 4828b16：`QualityDiversityArchive` + `ProtagonistEvolution::reproduceFromArchive` archive-uniform parent sampling + iso+line crossover + init phase 全链已通；v16 公平 scale 验证 me_coverage 20%）
- [x] 明确 behavior descriptor（v15 BD = (chops_bin=5, workcraft_bin=1)，8×8=64 cells；**已知架构限制**：BD 不含 throw/hunt 维度，导致 v15/v16 throws 失控 + hunts 退化，下一步候选扩 3D BD）
- [x] 把 archive coverage 纳入训练报告（fitness.csv 加 3 列 `me_coverage / me_qd_score / me_max_fitness`，runtime LOG_INFO 每代输出 me_cov）

### E2. World Model

- [ ] 先用现有 trace dataset builder 训练最小 predictor
- [ ] 产出 world model eval report
- [ ] 证明 latent feature 对 eval 有增益

### E3. Goal System

- [ ] 在 rule-based goal loop 稳定后，再设计 manager/worker 双头
- [ ] 补 goal progress metrics
- [ ] 做 goal ablation

### E4. 自长机制放大器

- [ ] 确认哪些能力适合通过 richer environment 自动长出，而不是人工先写死
- [ ] 为 R-008 设计 speciation / archive / diversity 保留策略
- [ ] 再评估是否进入 POET / environment genome

---

## F. 展示与正式长训（最后做）

- [-] 正式 overnight / multi-day 长训（在 A-D 未收口前不启动）
- [-] 3D replay panel（在机制链未收口前不启动）
- [-] live production world 正式上线（在自动晋升/回滚未完成前不启动）

---

## G. 文档与清理

- [x] 新建 `DEVELOPMENT_EXECUTION_GUIDE.md`
- [x] 新建 `DEVELOPMENT_TASK_PLAN.md`
- [x] 更新 `AGENTS.md` 的 route 文档导航，加入两份新文档并降级旧模块路线文档
- [x] 更新 route `PROGRESS.md` 顶部状态与下一步建议，改为引用新开发文档
- [x] 更新顶层 `PROGRESS.md` 最近会话摘要与下一步建议
- [x] 清理 `docx/` 下明显垃圾文件：`NUL`、异常字符空文件、误生成的空目录/脏文件
- [x] 将 `HANDOFF_NEXT_AI_2026-05-21.md` 归档或删除，避免继续作为主入口
- [x] 将 `MODULE_DEVELOPMENT_PLAN.md` 从主导航中降级为历史参考，避免与新路线抢口径
