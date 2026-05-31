# Protagonist Ecology · 路线进度追踪

> 这份文档只记�?`routes/protagonist_ecology/` 这个当前项目自己的问题、解决记录和下一步�?
>
> 顶层 `PROGRESS.md` 继续记录整个仓库总进度；这里记录主角生态路线的长期研究工程推进�?

---

## 2026-05-31 ���ٵ��飨���ۣ���ȫ�ܸ�ȫ��Ч������ = ���̴߳��У�

���񣺱���ѵ������/������/������䣬ֻ����������ȥ���û��۲� GPU 40-50%��CPU ~60% û��������Ԥ������� smoke��δ���ܡ�

**ͬ seed 8 ���� smoke��seed 50001��ʵ�⣺**
- batched_dense��`protagonist_batched_dense_enabled=true`����gen0-5 ʵ��Ȼ�����Լ 40%���� gen0 best fitness 8764 vs ���� 2907�������ƫ��Զ�� run-to-run ������-> **���**�����ĵ�/commit 69ea2fa ���� -15% �����֣������� 3 ��ʵ��Ϊ׼��
- `protagonist_dense_gpu_enabled` on/off������ǰ�� ~22-24s/�����ٶ���ͬ��wash����
- �ر� `/arch:AVX2`��scoped �� neural_eco_route_protagonist target������ wash��AVX2 + dense_gpu=false Ҳ wash��**�� CMake �Ķ��ѻ���**�������棬���� tree �ɾ�����

**����ʵ�� CPU ռ�ã�cpu_sample.ps1����** ѵ����������ǰ��׶�ֻ�� **1.09 / 32 ��** ~ �������̡߳�24 �����ǵ� per-agent ����ǰ����**����**����ģ�`agent_decision_threads=15` ��û������������������������ʱ��� GPU/CPU/AVX2 ȫ������----ƿ��������������**ȱ����**�������ý����û�������"��Դû����"��

**ȷ����˵����** ���߱��� run-to-run ��ȷ����ͬ seed ���� gen0 ����΢����������Ŵ�-> "�������"ֻ�ܰ�ͳ��һ�� + harmony 7/7 �ж�����������λ��ͬ��

**�����ĸܸˣ������¸��� / ��Ԥ��ʱ������** �� 24 ����������� per-agent ǰ�������̵���ˣ���ȫ���۲� -> ������ -> ͳһд�أ����������䡢��ѧ�ȼۡ������Ҳ��������ò��л���**�������� AgentLayer ���Ȳ�**�����ڱ� route������ deep_protagonist ���ã�����Ĵ��� + �ر� + ���� harmony������Ԥ�㲻�㣬δ����Ԥ�����������߳� -> ��˿ɴ�ӽ�������������ǰ����٣�����������"7 ��"���������ĵ���

---



## 顶部状�?

| �?| �?|
|---|---|
| 最后更�?| 2026-05-30 上午晚些 (UTC)�?*第四�?run s63810 �?gen 79 PURPOSEFUL 第三次同 seed 复现�?5/25 streak**：按 §0 铁律 6"一直跑"接的�?4 跑（seed 63810，gen79-only tail）跑完，gen 79 headless_score **0.244 �?PURPOSEFUL**，和 s81527(0.245)/s92641(0.285) 同型。dominant_goal_frac **0.729**（四轮最高）、predator_push **0.701**（四轮最强）、food_pull 0.482、velocity_autocorr 0.770、goal_switches 7.58（低）。`MainWorldHost end-of-run sim_time=800s tick_calls=800`，stage 4c snapshot 219 alive�?*harmony 7/7 �?25/25 streak**（C5 total=139、C6 qd=50994、C3 peak=1954.5s、C7 无灭绝）�?*�?seed gen79 汇�?0.245/0.285/0.244（三个干净 run �?PURPOSEFUL�? 0.354（跨 seed 唯一 MIXED）→ 3/4 干净 run PURPOSEFUL，结论加固：PURPOSEFUL 不是无头苍蝇**。注：本会话期间 Devin VM 系统重启，run �?WMI 脱离 SSH 照常跑完。trace 已删，产�?traj_s63810_gen79.json。commit 待写�?*接上一�?* 第二 seed 交叉验证 �?gen 79 PURPOSEFUL 复现，�? #1 trajectory honesty 彻底闭环：用全新 seed 92641 同样�?gen 0/3/79，headless 0.149�?.232�?*0.285 三代�?PURPOSEFUL**，与 s81527 同型（dom_goal_frac 0.64�?.69、predator_push 0.39�?.65、explore 53%�?5% / emit_signal 18%�?9%）。harmony 7/7 �?**24/24 streak**（C6 qd=67154）。三 seed gen79 汇�?0.245/0.285/0.354（仅�?seed 那条 MIXED 且刚过线），**2/3 干净 run PURPOSEFUL，结论稳健：PURPOSEFUL 不是无头苍蝇**；唯一 seed 敏感�?goal_switches。food_eaten s92641 gen79 2/24 吃到非恒零，确认�?bug，未�?reward。trace 已删 D free ~231 GB，产�?traj_3seed_verdict.md。commit 待写�?*接上一�?* �?seed gen 0/3/79 复核 �?gen 79 改判 PURPOSEFUL�?.245），food_eaten 疑点决起：用同一 seed 81527 一次抓 gen 0/3/79（`trace.max_generations=4` + `tail_generations=1`），做干净�?run 对比替掉上一条的�?seed 弱对比。harmony 7/7 �?**23/23 streak**（C5=152、C6 qd=54101、C3=3131.5s）。同 seed headless_score 0.181�?.195�?*0.245 三代�?PURPOSEFUL**（都 < 0.35）；上条 MIXED(0.354) 是跨 seed artifact。dominant_goal_frac 0.62�?.69（越来越专注主目标）、predator_push 0.42�?.66（天敌规避变强）、velocity_autocorr 0.90�?.79（从死直线变反应�?好事）�?*food_eaten 疑点决起**：同 seed food_eaten sum gen0=1.8/gen3=10.2/gen79=3.6（非恒零�?/24 agent 吃到），上次�?seed gen79=0 是单 episode 采样 artifact �?bug，未�?reward。trace 已删，产�?tmp/traj_s81527_*。commit 待写�?*接上一�?* gen 79 trajectory verdict 拿到 �?MIXED �?purposeful�?无头苍蝇"被证伪：80-gen tail-mode run s71359 跑完（gen 79，`MainWorldHost end-of-run sim_time=800s tick_calls=800`，harmony 7/7 ALL PASS �?22/22 streak；C5 worksites 151、C6 me_cov peak 0.440 qd 83887、C3 coop peak 3337s）。trajectory_analyze gen 79 ep0�?4 brained）：headless_score=0.354 �?verdict **MIXED**（刚�?0.35 阈值），但反应性指标强：food_pull=0.54 / predator_push=0.69 / dominant_goal_frac=0.61。跨 seed 趋势（gen0 s58193→gen79 s71359，仅趋势参考）：explore 43%�?2%、emit_signal 29%�?0%、bbox 缩到 1/4、velocity_autocorr 0.94�?.66 = 从乱逛转向信号协调。诚实保留：gen79 ep0 food_eaten_final=0。trace 已删�?56 MB），分析产物�?tmp/traj_gen79_ep0.json + tmp/traj_compare_0_79.md + tmp/traj_compare_3_79.md�?*接上一�?* Trajectory-analysis pivot：从"再跑一�?切到"看看脑子到底在干�?：用户在 stage 4b/4c 21/21 streak 之后明确叫停继续�?seed 累积，要求做轨迹层面的诚实评估—�?我敢打包票它们的轨迹画出来肯定是无头苍蝇，你完全给我调好了再让我�?。本会话围绕这条做了三件事：(1) **TraceRecorder.tail_generations** (`d081fd0`) �?新增配置字段 `trace.tail_generations`，让 TraceRecorder 可以专门捕获最�?N 代（而非�?N 代），配合磁盘纪律只留训练完成后�?gen 79 真实脑况�?2) **Python 轨迹分析工具�?* (`scripts/protagonist_trajectory_analyze.py` + `protagonist_trajectory_compare.py` + `inspect_trace.py`) �?读取 `agent_trace.csv`（每 tick �?agent 的位�?目标/社交/血�?经验事件），�?20+ 维度�?per-agent 指标：velocity_autocorr（方向一致性）、direction_persistence（目标追随度）、dominant_goal_frac（目标承诺度）、goal_switches（行为切换率）、visited_cells（空间探索）、path_length（累积位移）、radius_of_gyration（空间分布），并新增 **reactive purposefulness** 双指标（food_pull_fraction = 食物在感知范围内时朝食物移动的比例，predator_push_fraction = 检测到捕食者时加速逃逸的比例）—�?不只�?动得有规�?还看"动得有反�?。聚合输�?headless_score �?[0,1] + 三档 verdict (PURPOSEFUL / MIXED / HEADLESS_FLIES)�?3) **gen 0 vs gen 3 已分析、gen 79 没拿�?* �?�?Run 10 (s58193) 数据上跑出：gen 0 mean velocity_autocorr=0.943 / direction_persistence=0.947 / dominant_goal_frac=0.581 / goal_switches=5.5；gen 3 同维度变�?0.824 / 0.816 / 0.631 / 5.9。headless_score gen 0=0.164、gen 3=0.236�?*两代都判 PURPOSEFUL**（虽然这只是必要条件不充分）�?*反直觉发�?*：用户预�?无头苍蝇"�?gen 3 阶段已被初步证伪——agent 不是越练越乱，是 vel_autocorr 主动�?0.94 降到 0.82 = 不再走死板直线开始响应环境，dominant_goal_frac 涨了说明对单一目标承诺度增加，path_length 633�?78 说明实际位移变多。但 gen 79�?0 代训完后）数据没拿到——s71359 tail-mode run 跑到 gen 38 时被用户叫停（他要玩游戏�?GPU）�?*下一会话首要任务**：补一�?80-gen tail-mode run �?gen 79 trace，跑 trajectory_analyze �?gen 0/3/79 三点曲线是上升、平台还是下降，下个 verdict�?*磁盘纪律**：执�?"只留上一�?，删�?s58193 �?1.5 GB trace dir �?60gen_2d_curriculum (149 MB)；s71359 tail run 输出删除（partial）；�?routes/protagonist_ecology/data/runs 只有 6 �?stage4b run（每�?~40 MB CSV+snapshot，s58193 因留 checkpoint 略大�?trace 删完），D �?free ~250 GB�?*Run 10 (s58193) 7/7 PASS �?commit** `1f4ce28` �?21/21 streak（live-agent 时代第十次独立验证）。本会话 commit 链：`1f4ce28` (Run 10) + `d081fd0` (trajectory pivot)�?*未达�?*�?2/22 streak（s71359 没跑完）、gen 79 trajectory verdict、Layer 7 viewer 不动（用户明�?先不要可视化"）�?|
| 上一更新 | 2026-05-29 凌晨 (UTC)�?*Layer 6 R3 stage 4b 实装 + 第十二跑 80-gen 7/7 PASS �?12/12 不败 streak 进入 live-agent 时代**：本会话新增 `protagonist_ecology: Layer 6 R3 stage 4b live agent seeding (borderland->main_world)` (`f1ad366`) �?MainWorldHost 新增 `seedAgent(archetype, genome, lineage_id, config)`，在 ProtagonistEcologyRuntime �?`promoteBorderlandToMain` 之后自动把每一批新�?genome 实体化成�?Agent 注入永不 reset �?World2D。用 `BootstrapPopulation` typed 工厂 + `stampLifecycleAndLineage` + AgentLayer::addAgent 链路，宿主自�?`mt19937` (seeded from `random_seed + 0x4D575F31`) 保证�?run 可复�?spawn 抖动�?*第十二跑 (seed=19283, baseline-v3 + stage 4b on, 22:30�?0:41 UTC, 131 min)**：`MainWorldHost end-of-run: cumulative sim_time=800.000s tick_calls=800` 第五次实证永�?reset 800 tick �?本次额外验证 **永不 reset 主世界容�?213 个并发活 agent**（最�?alive_now=213/320 seeded = 67% 存活）�?*8 �?b2m promote 全部 seed 成功** (gen 9/19/29/39/49/59/69/79，每�?8 protag + 32 bg = 40 agent)，累计死亡曲�?0% �?0% �?4% �?12% �?15% �?22% �?28% �?33% �?LifecycleLayer 老化 + 物种互动的健康生态动力学（不�?bug）。gen 79 protagonist avg=3213 / best=12309，gen profile 显示 world_non_brain 主导 95% �?runtime 随活体数线�?scale，没崩没 OOM。harmony 7/7 ALL PASS：C5 worksites total=134、C6 me_cov peak=0.400 qd=60025、C3 coop peak=2533.8s（介�?seed=7777 �?3073s �?seed=13579 �?2164s 之间，跨 seed 在合�?band 内）、C7 无灭绝。第十三�?(PID 55268, seed=37241, baseline-v3 + stage 4b, 00:47 UTC 起跑) 进行中，预计 ~02:30 UTC 跑完。前 11 跑战果完整保留——第十一�?baseline-v2 seed=24680 (70/60678/1739s)、第十轮 structural seed=13579 (142/62362/2164s)、第九跑 curriculum seed=8888 (149/55381/2493s)、第八跑 curriculum seed=7777 (148/88450/3073s)，加本次 stage 4b seed=19283 (134/60025/2533s) 形成�?5 �?toml/seed × 800 tick 永不 reset × 5 次独立验证�?*关键设计澄清**（用户与 AI 对答）：MainWorldHost 是�?*训练�?* 的持久生态」而非「训练完后的部署目标」——spec invariant #1 设计本意就是主世界从 gen 0 开跑、跨整个训练永不 reset、agent 通过 borderland→main promote �?10 代滚动进来，模拟生态演替而非 train-then-deploy。「训练后冻结快照」这一直觉将通过 Layer 6 R3 stage 4c (end-of-training main_world 冻结快照) 单独实现，作�?viewer/replay 的载体。test_core 225/225 PASS 维持（test_main_world_host �?seedCount 初始 0 断言）�?*下一会话候�?*�?a) stage 4c snapshot 实装�?b) MainWorld 性能护栏 (population cap if alive > 280)�?c) NSGA-II Pareto 替换 sandbox→borderland top-K 选拔�?d) Layer 7 Godot viewer MVP（消�?stage 4c snapshot）。） |
| 当前阶段 | **Layer 1-5 完整 + Layer 6 R2 stage 3 + Layer 6 R3 stage 3 commit A-O + Stone bootstrap commit 1/2/3/4 �?done + TraceRecorder disk-budget cap (`243054b`) + 六轮 60-gen 全部 7/7 PASS**�?*六轮 AB 对照** (toml / 时长 / C5 / C6 / gen59-fit)：curriculum-v1 (190min, 88, 0.480, -) / structural-v1 (179min, 110, 0.440, -) / structural-v2 +cap (75min, **113**, 0.400, -) / curriculum-v2 +cap (84min, 109, 0.400, 1786) / throw-v2 +cap+penalty (**42min**, **34�?*, **0.320�?*, 2352) / **baseline-v2 clean+cap** (**44min**, 40, **0.440**, **3223**)�?*关键发现**�?1) cap 跑快 2.4 倍；(2) **6 轮全 7/7 PASS** �?5 �?config (clean baseline / curriculum / structural / structural+mut / throw_penalty)�?3) throw_penalty 修后呈现 "PASS 但低�? 而非 fix �?worksites=0 全崩，证�?**Stone Bootstrap commit 4 fix �?NurseryProgression 普遍根因修复，reward shaping 是另外独立的优化空间**�?4) clean baseline gen 59 protagonist avg=**3223** 是六轮最�?�?�?curriculum 压力�?protagonist 自由发展 fitness 更强�?5) graduate pipeline �?me_cov / fitness �?worksite 完成度（trade-off 健康）。Stone Bootstrap commit 4 (`fb2c9c5`) + cap (`243054b`)。Layer 7 R3 step 1 trace 完整 stage 2 viewer 待决策。Layer 8 R1 Lamarckian 落地�?|
| 当前项目优先�?| **Stone bootstrap commit 1-4 �?done + 六轮 60-gen 7/7 PASS (curriculum-v1/v2 + structural-v1/v2 + throw-v2 + baseline-v2) + disk-budget cap `243054b` + fix 通用�?5 config 全验�?*。下一步：(a) **�?#3 main world continuous tick** (FINAL_BLUEPRINT 最大遗留模�? �?主世界永�?+ 自己 reproduce 的核心机制，预计 TripleWorldRuntime �?mainWorldTick() + 持久化保�?+ 主世�?reproduction loop 多个 commit�?b) **Layer 7 R3 stage 2 Godot viewer** �?trace 流已经完整，差一个能可视化展示的前端�?c) 若有 throw_penalty 优化想法 �?单独立项（不再混�?graduate 实验）。用户路线图"1�?�?�?�?"�?*#1+#2 完整闭环且六轮验�?*，正式进�?#3 main world 节点�?|
| 最新用户新增问�?| 用户要求 "跑a3实验看看效果 �?重训 �?看着�?�?*Phase 1 (commit 0119e78)**：v8 retrain（seed 137�?20 gen、emitter 周期 15s、jitter 16m、dense_reward 1.5、训�?12 min、peak best=229k�? 5×7 ablation sweep。`comm_scout_worker` 强证�?fitness 58.3�?.95 �?3%、responders 1.5�? �?00%；`comm_danger_warning` survived �?8/8（task pressure 不足）；`comm_build_request` worksite=0（logistics 链断）�?*Phase 2 (�?commit)**：修两个有问题的 task 设计——`comm_danger_warning` �?safe-rallying 语义�?0×60 地图�?2 predators、emitter �?nest、dense_reward +0.6）让 baseline 出现 4.5 死亡/episode + fitness �?8%；`comm_build_request` required_sticks 2�?, stones 1�? �?worksite_completion �?0（visibility=false �?v8 看不到方向），fitness �?7%。结论：fitness drop 链路�?4 个通信 task 上多重成立，A3 主线判定多重通过；survived / worksite_completion 这两�?primary metric �?v8 当前能力上限，需要在 protagonist 训练阶段就接触这两个 task（推�?B 阶段）。完整对照见 `supporting/COMMUNICATION_SEMANTICS_EVIDENCE.md § 3-9`�?|
| 最新本体论决议 | 用户提出"如果出问题要回流训练，活体是不是会集体消失？"——已固化�?`supporting/PRODUCTION_WORLD_LIFECYCLE_SPEC.md`。核心三句话：主世界永不 reset / 训练永远在别处做 / 新血脉只能通过出生或迁徙进入主世界。这�?spec �?R-002 / R-006 / R-007 / 3D viewer 四条线的共同地基�?|
| Phase 3 收口�?026-05-22 晚） | 三处真实修复 + 一�?hack 占位 + MI 分析�?*真实修复**�?1) `worksite.preactivate` �?eval 任务�?worksite 一开局就激活，`comm_build_request` �?`worksite_completion_events` 从全 0 拉到 baseline 0.75-1.0，`spatial / goal` ablation 仍降�?0.5（链路解耦正确）�?2) `GoalActionAssistCapability` carrying 时把"最�?peer_signal_heard"作为 active worksite �?fallback 返航 cue（实测主路径未触发，�?spatial-blind 的合�?fallback，单测正负例都锁了）�?3) **TraceRecorder �?3 �?env-state 输入** (`nearest_food_dist / predator_nearby_count / worksite_distance`)，配�?`scripts/protagonist_communication_mi.py` 跑出**第一份定量语义涌现证�?*——`comm_danger_warning / baseline` �?channel-0 active �?`predator_nearby` MI = **0.36 bits**，是 `food_nearby (0.14)` �?2.6 倍，�?`comm_scout_worker / comm_build_request` 同组�?9-15 倍；信号关闭�?MI 全部归零�?|
| **B 阶段 L1 落地 + v9 retrain�?026-05-22 夜）** | **替换�?Phase 3 �?`nest_predator_damage` hack**。机制底座：(1) `WorksiteLayer::enforcePredatorObstacles()` —�?完工 worksite 形成 collidable 圆，predator 走进去被推回边缘、protagonist 不阻挡（FINAL_STATE_VISION § 3.7 L1）；(2) `BaseLayer.nest_predator_damage_per_second` 字段/配置/tick/Factory/MemoryAblationEval 全清�?3) `AgentSocialPerception` kDim 8�?0，新�?`peer_count` (out[8]) + `predator_count` (out[9]) �?brain 自己决策 fight-back / flee�?4) `ProtagonistHuntCapability` 删除 `target->isPredator() continue`，主角现在能打狼（NN 自己决策）；(5) `TrainingParams.shelter_camp_reward` 0.06 + `shelter_camp_predator_radius` 12.0 —�?主角站在完工 worksite �?+ 12m 内有 predator 才给 reward（防 idle camping）；6 �?EmergentShelterL1 unit tests 全过 (135/135 total)�?*v9 retrain 完成**�?20 代，seed 137，peak fitness 265k @ gen 46�?*�?6% over v8**）；v9 ablation eval 关键数据：comm_danger_warning baseline survived **5.0�?.5 �?0%**（hack 撤了反而更高），protagonist_attack **0�?52.5 / episode**（反击行为涌现），但 completed_worksites=0（v9 学到的是直接打而非"建墙挡狼"，因 eval `worksite.count=0`）；memory ablation 链路保持（no_episodic 6.5�?.25 �?.25, no_spatial 6.5�?.0, no_social 6.5�?.0）；Peak fitness 265k vs v8 229k�?*还没到的**：v9 没学�?"防微杜渐建墙再用" 的曲线流程；下一�?B 阶段 L2 = �?comm_danger_warning eval �?worksite preactivate + 训练 task 设计专门�?predator 浪涌�?protagonist 先建墙�?|
| **L2 G1 batched brain forward + GPU 验证�?026-05-22 夜，commit 69ea2fa�?* | **三阶�?brain forward + per-frame batched dense matmul**。改 8 个文件，�?5 个新文件（含 1 �?.cu kernel），�?+1366 �?/ -1 行。`IBrain.h` �?`IBatchableBrain` 可选接口（prepareInputs / denseSlot / finalizeOutputs），`Agent` �?`decideUntilDense / decideAfterDense`，`AgentLayer` �?`BatchedDecisionHook` + 4 阶段 tick；`ProtagonistBrain` 实现 batchable 三段（memory bank tick / encoder / DNC step �?per-agent，只�?dense matmul 抽出�?batch）；`BatchedDenseGpu.cu` 是新�?batched kernel（blockIdx.y = agent，blockIdx.x = unit，CPU `forwardCpu` 作为 fallback + 测试参考）；`ProtagonistBatchedDenseRunner` �?N �?brain �?slot 收成 agent-major flat buffer 走单�?GPU forward；`ProtagonistEcologyRuntime` �?3 �?createBootstrapWorld 点装 hook（init / memory ablation / 训练 episode pool）。Config: `compute.protagonist_batched_dense_enabled` (默认 off) + `protagonist_batched_dense_force_cpu` (test 开�?�? 个新数值对�?unit test（CPU batched vs per-agent forward ε<1e-5�? 138/138 全过�?*端到�?GPU 验证通过**（RTX 5060，CUDA 12.8）：单线�?10 �?baseline gen_total~30s brain%=60% �?GPU batched gen_total~26s brain%=52%�?*brain time 30%↓，总训�?15%�?*；fitness 一致（GPU avg best 62804 vs CPU batched 61576，process-RNG noise 内）�?|
| **L2.6 v12 4-机制 literature-backed 替换 v10/v11 临时调参�?026-05-23 凌晨，commit 422847f + d41a0ff�?* | **用户的核心反馈：v10/v11 反复"调参 / �?task"是错的方向，�?AI 先查文献再说**。文献调研后定位�?4 个真名症�?+ 4 个对应正解：(1) **Lazy Agent Problem** (Sunehag et al.) �?protagonist 学会"躺平"�?2) **Negative Transfer / Gradient Conflict** (Yu PCGrad, Cho SMT 2024) �?6 �?task_template 互相干扰�?3) **Multi-task averaging** (Schaul Ray-interference 2019) �?跨任务平均推向保守；(4) **Sparse Long-horizon Reward** �?worksite/craft 链条 outcome 信号�?drive penalty 压扁�?*4 �?literature-grounded 修复**全部 commit�?1) Step 1 = **Context-conditioned policy** (Rakelly PEARL 2019, Duan RL² 2016) �?`TaskContextPerception` 新增 8-onehot 输入，让单一神经网络�?task-specific 行为而非"�?task"�?2) Step 2 = **Decoupled intrinsic motivation** (Cuccu/Schmidhuber 2011 "Intrinsically Motivated NeuroEvolution") �?`intrinsic_weight × novelty_reward` 不再�?`outcome_weight` 缩到 0.05× 而是直接乘；(3) Step 3 = **ACCEL regret-based weighting** (Parker-Holder NeurIPS 2022) �?每代 EMA 更新 `(best - avg) / (\|avg\|+1)` 作为 template_regret，高 regret = "agent ALMOST solves it" �?自动 upweight�?4) Step 4 = **SMT Hard-Tasks-First** (Cho ICML 2024) �?早期 N 代用声明 difficulty 加权（线�?taper），强迫网络先碰难任务避免简单任�?掩盖"复杂任务。改 8 文件 +314/-10 行（新增 `TaskContextPerception.{h,cpp}` + `Agent::task_template_idx_` 字段 + Runtime `computeEffectiveTemplateWeights` + `selectTaskTemplateDynamic` + EMA update）�?*166/166 tests 全过**�?*v12 60-gen 验证 (seed=91，与 v10/v11 同配�?**：alive 1.9�?.8 (+100%)、worksite 0.1�?.5 (+400%)、chops 7�?8.6 (+166%)、craft 0.1�?.7 (+600%)、hunts 19�?1 (+61%)；best_fitness CV v10 327% �?v11 103% �?**v12 88%**�?*v12 起步 +216 vs v11 起步 -107**（context cue + decoupled novelty 让网络从一开始就有方向）。仅 co_presence 反�?-49%（hard-tasks-first 早期�?mixed_ecology 8-protag 推到前台，是参数调而非机制问题）。仍未追�?v9 的长链条 build/craft 数字 �?那是 60 代不�?+ 需�?hierarchical RL extension（不在本 commit 范围）�?*结论�? 机制是后续涌现任务的稳定底座**�?|
| **v2 物种差异�?L2.1-L2.4 全部落地�?026-05-22 夜，4 commits�?* | �?SPECIES_DIFFERENTIATION_SPEC.md �?4 层架构执行，�?v1 "30 �?reward 标签" 完全转向 v2 "机制压出�?，符合用户原�?奖励太直�?�?反涌�?�?*L2.1 物质条件分化** (commit 200ea1a)：Agent �?6 字段 (damage_threshold / threat_damage_window / stealth_state / digest_satiety / scavenge_efficiency / is_in_cave) + 4 方法（accumulateThreatDamage / clearThreatDamageWindow 等）；PredatorHuntCapability 实现 1s 滑动窗口 prey-side 累积器（**单狼 dmg 4 永远过不�?LargeHerbivore threshold 12，必�?4 狼同时打才解�?kill burst**�? �?stealth 第一�?×2 伤害 + apex �?prey kill �?60s digest_satiety + scavenger 食腐 ×3 / 鲜肉 ×0.33；新�?StealthCapability�?s 静止入隐�? CaveHideCapability（terrain 查询）；AgentSocialPerception �?stealth + cave 隐身过滤�? 物种 capability 数值差异化（狼 dmg 10�?、scavenger �?PredatorHuntCapability output_dim 6�?）�?4 个新单测全过�?*L2.2 drive state 系统** (commit 3896e90)：Agent �?5 �?drive (hunger/thirst/fatigue/fear/loneliness) + DriveSetpoints；DriveTickCapability 自动更新 4 �?drive，thirst �?runtime �?SurvivalStatusLayer 读；reward = drive_weight × driveSetpointPenalty() + outcome_weight × �?v9 reward�? �?toml gate (`reward.drive_weight`, `reward.outcome_weight`, `reward.drive_thirst_from_hydration`)；setpoints �?spec § 2.1 物种特化（apex/leopard loneliness=0.6 / pack=0）�? 个新单测全过。Smoke 5 �?protagonist avg fitness -1455 �?-46（drives 自动恢复�?setpoint）�?*L2.3 资源 niche** (commit 06daaf0)：ResourceLayer �?ResourceKind 枚举 (Grass/Browse/Fruit/Meat) + PlantSpawnWeights ctor + sampleKind() 抽样；Agent �?EatEfficiencyByKind (4 floats)；EatCapability �?kind 缩放 energy_gain�? 物种各自 efficiency �?(LargeHerbivore grass=1/browse=0.5/fruit=0.3/meat=0; Scavenger meat=1/fruit=0.5; Omnivore fruit=1/grass=0.6/browse=0.6/meat=0.4)；toml `[world.resources.plant_weights]` 暴露三个权重�? 个新单测全过�?*L2.4 task template 自适应课程** (commit 7d978b6)：ProtagonistEcologyConfig �?TaskTemplate 结构 + `[[evolution.task_templates]]` toml 解析；selectTaskTemplate (gen, ep, seed) 决定性加权采样；applyTaskTemplate �?episode bootstrap �?mutate route + episode_params�? �?spec § 4.2 模板 (solo_hunt_easy / solo_hunt_predator / group_vs_pack / proactive_build / wave_attack / mixed_ecology) 完整实例 toml �?tmp/l2_2_smoke/l2_4_curriculum.toml�?66/166 测试全过�?*整体小结**：v2 全套 L2.1-L2.4 �?+2740 �?/ -32 行。每个阶段都�?toml gate 默认 off，老训练（v9 / a3 / memory ablation）行为完全保留。下一�?L2.5 = 长训验证 + 文档收口�?|

---

## 已完�?

### Layer 1-4 (FINAL_BLUEPRINT.md 蓝图�?026-05-23 ~ 2026-05-24)

�?`docx/FINAL_BLUEPRINT.md` �?Layer 1-8 依赖树，已落�?1-4�?

| Layer | 内容 | 数据 | Commit |
|---|---|---|---|
| 1 | 体温 R-001 + 600x600 + Hunger threshold | alive 0.45�?.79 | a0669f6, 7a098a6 |
| 2 | WeatherLayer 6 type Markov + �?�?�?集成 | avg_fit 990�?972 | 202186b |
| 3 | Rabbit 物种 + 9 物种生态闭�?| avg_fit 1972�?685 (gen 25 best 12390) | e3d2b8b |
| 4 | LifecycleStage + DeathCause + lineage_id + PopulationLayer | OldAge/Disease 路径 + 4 in-world births 验证 | 148b6d8 |

**Layer 4 核心设计**�?
- Agent �?`lineage_id_/parent_lineage_a/b_/birth_generation_/archetype_tag_/genome_archive_`
- `PopulationLayer.tick`: phase1 cooldown/OldAge/Elder mortality, phase3 同物�?adult 配对
- `makeChildFromParents`: clone better-fit parent NeatGenome �?noise weight �?makeNeatSpeciesAgent
- 性能：每�?in-world birth ~100ms（NeatBrain reconstruction），Layer 8 优化

### 1. protagonist 当前项目最终愿�?

- 已写：`supporting/FINAL_STATE_VISION.md`
- 目标：原始聚落沙盒世界，不是随机点生态箱�?
- 关键观感：采集、搬运、堆放、回巢、围火、建造、围猎、通信、代际进步�?

### 2. protagonist 开发执行与任务计划

- 已写：`DEVELOPMENT_EXECUTION_GUIDE.md`
- 已写：`DEVELOPMENT_TASK_PLAN.md`
- 目标：把长蓝图、用户需求和真实代码揉成一条默认执行路线，并把任务与设计分离�?
- 当前文档地位：`archive/MODULE_DEVELOPMENT_PLAN.md` 降级�?*历史参�?*，不再作为默认主导航�?

### 3. 世界理解与复杂行为长期研究工程方�?

- 已写：`supporting/WORLD_UNDERSTANDING_RESEARCH_AND_ENGINEERING_PLAN.md`
- 已列出核心瓶颈：
  - 记忆结构�?
  - 缺世界模�?
  - reward 偏行为计�?
  - 合作信用分配�?
  - 通信没有稳定语义
  - 课程不够开放式
  - �?replay / trace
  - 缺完�?checkpoint resume
  - 缺层级目�?
  - 缺长期大规模实验制度
 - 已定优先级：
  1. Trace / Resume
  2. Memory V2 专门记忆�?
  3. EventLineage / ContributionLedger
  4. 通信语义任务
  5. Quality Diversity / MAP-Elites
  6. POET-like 开放式课程
  7. World Model
  8. Hierarchical Goal System
  9. 长训 + 3D replay panel

### 4. 记忆系统总设计与 Phase A 接线方案

- 已写：`supporting/MEMORY_SYSTEM_MASTERPLAN.md`
- 已写：`supporting/MEMORY_PHASE_A_EXECUTION_PLAN.md`
- 已完成对当前 protagonist Memory V2 的实况审计：
  - `MemoryBank` 是真闭环，但仍是无结构短期记�?
  - `SpatialMemoryBank` 已拆�?`water / food / tree / stone / stick / danger / base / fire / worksite` 9 个独立语义槽，runtime 写入已覆盖这些主要对象类�?
  - `EpisodicMemoryBank` 已接入结构化 `chopped_tree / built_worksite / deposited_* / signal_emitted / drank_water / saw_predator` 等事件，并通过 `MemoryCognitionPerception` 做基础 readback；但 query 深度仍浅
  - `SocialMemoryBank` 已不只是 sighting：cooperation / communication / hostility 已有真实写入�?readback，但还没有通信语义任务与专门指�?
  - `GoalMemory` 已由 runtime 里的 rule-based 选择逻辑持续刷新，不再长期停在默�?`Explore`；但还不�?Manager/Worker 层级系统
  - `MemoryCognitionPerception` 已扩�?44 维，并纳�?DNC 摘要
- 已形成路线级结论�?
  - 不能只把 memory 做大，主行为控制脑也必须同步变大
  - 先把现有 Memory V2 真接活，再做 `ProtagonistBrainV3`
  - 后续 protagonist dense brain �?mixed GPU，世界逻辑和事件系统继续深�?CPU 并行
- 已写：`supporting/COMPUTE_SCALING_GPU_CPU_PLAN.md`
- 已明确后续算力承接顺序：
  - 先做记忆系统真闭环与主角脑结构化扩容
  - CPU 先从 episode 级并行升级到 agent decision / batch �?
  - GPU 首批只承�?protagonist dense cognition 与后�?world model，不做全项目 GPU 重构
- 已把 Phase A 实施顺序固定为：
  1. `SpatialMemoryBank` 语义拆槽
  2. `MemoryCognitionPerception` 扩维
  3. 最�?`GoalManager` 闭环
  4. `EpisodicMemory` 扩事件与查询
  5. `SocialMemory` �?cooperation / hostility / communication
  6. `memory ablation eval`

### 4. Phase 1 · Trace / Resume 基础设施

- 已实现：`runtime/TraceRecorder.h` / `runtime/TraceRecorder.cpp`
- 已打通训�?episode �?trace 输出�?
  - `event_trace.jsonl`
  - `agent_trace.csv`
  - `world_trace.csv`
  - `replay_manifest.json`
- 已实�?protagonist checkpoint manifest：`checkpoint_manifest.json`
- 已新�?machine-readable checkpoint state：`checkpoint_state.toml`
- 已打�?`runtime.resume_checkpoint_path`，可�?`checkpoint_genN/` 恢复�?
  - protagonist population
  - background species NEAT populations
  - background species innovation tracker
  - protagonist/background evolution RNG state
  - background species speciation state
  - resume generation index
- checkpoint 现在改为 post-reproduction 保存�?
  - `checkpoint_state.toml` 记录 `evaluated_generation + resume_generation`
  - resume 直接�?`generation + 1` 继续，不再重复评估已保存那一�?
- 已补 checkpoint best genome continuity：`best_genome_overall.bin / best_genome_latest.bin`
- 已完�?smoke trace 输出验证
- 已完�?smoke resume 验证
- 已完�?single-thread full-vs-resume 对照验证：`checkpoint_gen1` 恢复后只�?`generation=2`，且 `fitness.csv` 与完整直跑结果一�?
- 当前已知边界：route 多线程训练本身仍不是�?run 严格确定性的，所以“精确轨迹对照”以单线�?smoke 为准；这不再�?checkpoint 缺口，而是训练并行确定性问�?

### 5. 连续时间世界前置规划

- 已写：`supporting/CONTINUOUS_TIME_WORLD_DESIGN.md`
- 已完成一次面向工程落地的外部资料调研，重点覆盖：
  - 固定上限子步�?/ semi-fixed timestep
  - CTRNN 连续状态积�?
  - event-driven multi-agent decision process
  - SMDP / temporally extended action
- 已明确后续采�?**hybrid continuous-time** 路线�?
  - 世界真实时间改为 `seconds`
  - 运行时内部做子步积分
  - 事件同时记录 `time_seconds` + `step_index`
  - 动作、reward、trace 改按持续时间表达

### 6. CT-0 · 时间接口兼容�?

- 已新�?`SimTimeSeconds`
- 已为 `Event` 增加 `timeSeconds()`
- 已为 `IWorld` / `World2D` 增加 `currentTimeSeconds()`
- `World2D` 已维�?`time_seconds_`，并�?`tick(dt)` 中持续推�?
- `TickStarted` / `TickEnded` 事件已同时带 tick 与秒级时�?
- `TraceRecorder` 已输出双时间戳：
  - `event_trace.jsonl` 增加 `time_seconds`
  - `agent_trace.csv` 增加 `time_seconds`
  - `world_trace.csv` 增加 `time_seconds`
- 已完成重新构建与 protagonist smoke 验证

### 7. Stage 0 · CT-2B/CT-3 结构化动作事件（2026-05-21�?

新对话规划下�?Stage 0 收口。目的：�?chop/build 等持续动作以**结构�?started/completed 事件�?*的形式被记录，Memory Phase A 才有真正可消费的因果素材�?

**已落�?*�?

- core EventType 新增 6 �?protagonist route 事件：`ChopStarted` / `ChopCompleted` / `ChopAbandoned` / `BuildStarted` / `BuildCompleted` / `WorksiteMaterialDeposited`，含 EventType.h + EventType.cpp + EventPayloads.h
- `TreeLayer::chopNearest` 改返�?`ChopHitResult{tree_id, tree_pos, remaining_integrity, felled, wood_yield}`，让 capability 不必二次查询就能获得归因
- `ChopTreeCapability` 改成有状态的 per-agent session 状态机：start / continue-same-tree / target-switch (abandon-old + start-new) / abandon (output drop / out-of-range / now-carrying) / complete；每�?agent 自己一份实例，串行 apply 无需�?
- `WorksiteLayer` 增加三个 CT 字段：`build_started_at_seconds` / `per_agent_contribution_seconds` (map) / `active_builders` (set)；`progressConstruction` 现在做：首次进入�?BuildStarted、每 dt 累积 per-agent 贡献时间、完成时按贡献时长降序生�?contributors 列表�?BuildCompleted
- `absorbCarriedMaterials` 每次实际入库�?`WorksiteMaterialDeposited`，带 depositor + material_type
- `TraceRecorder` 已为全部 6 个事件接�?payload 序列化，`event_trace.jsonl` 可看到完整字段（�?`{"chopper":8,"tree_id":5,"started_at_seconds":2.5,"duration_seconds":2.5,"wood_yield":3}`�?
- Reward 时间积分口径审计完成：survival/proximity/social 等已�?`* dt_seconds`，combat/death/signal/joint_hunt �?per-event one-shot，food 通过连续 EatCapability �?plant/meatFoodThisTick 自然�?dt 累积�?*没有发现任何 per_tick artifact**

**已验�?*�?

- Release 构建通过（`neural-eco-protagonist` + `test_core`�?
- 73/73 单测通过
- protagonist smoke 跑通，event_trace.jsonl 显示 ChopStarted=8 / ChopCompleted=6 / ChopAbandoned=2 完整闭合，payload 字段齐全

**意义**�?

- 之前�?chop/build 只能体现为离散计数（"chop_attempts/successes 加一"）。现在每一个完�?chop 都是一个带起止时间 + 时长 + 树木归因的结构化事件，Memory Phase A �?EpisodicMemoryBank 可以直接 push 这种事件
- Worksite �?`per_agent_contribution_seconds` �?P3 ContributionLedger 的现成数据源——以前是"在场就分"，现在是"按陪伴时长分"，路过者天然吃不到�?
- BuildCompleted 已经按贡献时长降序排�?contributors 列表，下游消�?0 成本

**已主动延�?*：CT-Step 4（EventQueue / DueAction）当前没有真实消费者，决定推到 Stage 1（Memory Phase A �?goal expiry / episodic decay 会真正需要它），避免过度工程

### 8. Stage 1 · Memory Phase A 收口�?026-05-21�?

�?`supporting/MEMORY_PHASE_A_EXECUTION_PLAN.md` 完成 Phase A �?6 个验收项。在大重�?commit `c7f1665` 中很�?Phase A 部件其实已经接好了，本会话主要做以下三件事：

**已落地（本会话补完）**�?

- �?Stage 0 �?6 个结构化事件接进 EpisodicMemoryBank：在 runtime 评估循环新增 3 �?tick 收集器（`tick_chop_completed` / `tick_build_completed` / `tick_deposits`），通过 EventBus 订阅在每�?substep 收集，然后在已有�?per-agent memory 写入循环里转写成 `chopped_tree / built_worksite / deposited_stick / deposited_stone / signal_emitted` �?5 �?episodic 事件，含 salience 模型（chop 按时长，build 按贡献排名）
- 增加 TraceRecorder 对全�?6 个新事件�?payload 序列化，�?`event_trace.jsonl` 字段完整（之前只能看�?`{"type":"ChopCompleted"}` 没有 chopper / duration / wood_yield�?
- 新增 memory ablation smoke harness：`scripts/protagonist_memory_ablation.py` 一次跑�?baseline + 4 �?ablation（spatial/episodic/social/goal），自动生成对照表并保留各自 trace 目录，验�?4 �?`population.protagonist.*_enabled` 开关都可读、可执行、可观测
- �?`supporting/MEMORY_ABLATION_SMOKE_RESULT.md` 诚实记录首轮结果�? 开关都�?fitness 和行为指标显著移动（基础设施验收 PASS），�?ablation 反�?fitness 更高的反直觉现象需要长训才能反转，**不能当成"记忆有害"的科学结�?*

**Phase A 验收清单（全 PASS�?*�?

| 验收�?| 状�?| 证据 |
|---|---|---|
| Spatial / Episodic / Social / Goal 都进入真实决策闭�?| �?| `MemoryCognitionPerception` 44 维全部从 4 �?banks 读出 |
| `MemoryCognitionPerception` 扩到能承�?Memory V2 | �?| 9 �?spatial �?16 �?+ episodic 6 �?+ social 8 �?+ goal 4 �?+ DNC 8 �?|
| goal 不再是默�?`Explore` 常驻 | �?| rule-based GoalManager: Flee/FindWater/BuildWorksite/RestNearFire/FindFood/Explore |
| social 分数不再长期�?0 | �?| smoke trace: social_best_coop 1595/9000 行非零；comm 1597 行；hostility 162 �?|
| spatial 通道不再混槽 | �?| 9 个独�?cell 字段（water/food/tree/stone/stick/danger/base/fire/worksite）独立写入与查询 |
| 一组最�?memory ablation 对照结果 | �?| 5 变体 smoke �?+ smoke 结果文档 |
| smoke / train continues to work | �?| 73/73 tests + protagonist smoke 全程�?|

**已主动延后到 Phase B**（不�?Phase A 必需）：

- DNC-forcing micro-task（设计一个只有用 DNC 检索才能拿 reward 的环境）
- `WorkingMemoryBank` 多时间尺度化
- `ProtagonistGenomeV2` 编码更大脑结�?

### 9. mixed compute 第一轮工程化

- 已把 `Agent::step()` 拆成 `decide / publishDecisionEvent / applyCapabilities` 三相，保证重计算与世界写入分�?
- 已给 `AgentLayer` 新增持久�?decision executor，支�?route 通过 `compute.agent_decision_threads` �?per-world agent decision 并行
- 已把 protagonist route training 的外�?`EvaluationPool` 与内�?decision threads 联动，避�?episode 并行�?agent 并行无脑相乘造成 CPU 过度超卖
- 已新�?route �?compute 配置�?
  - `compute.protagonist_dense_gpu_enabled`
  - `compute.agent_decision_threads`
- 已给 `ProtagonistBrain` 接入可�?`ProtagonistDenseGpu` backend，并�?CMake 中加入可�?CUDA `.cu` 编译路径
- 已修�?route runtime �?protagonist brain probe 输入维度统计，统一复用 `protagonistSensoryDim()`，避免参数量低报
- 已完�?`neural-eco-protagonist` / `neural-eco` Release 重编�?`protagonist_ecology_bootstrap.toml` 非训�?preview 验证
- 当前环境结论（已按最新基准重标定）：
  - 普�?`build/` 仍是 CPU fallback，`CMAKE_CUDA_COMPILER=NOTFOUND`
  - `build_cuda/` �?`D:\NVIDIA\CUDA\v12.8\bin\nvcc.exe` 构建，可执行文件在补�?CUDA bin �?PATH 后可运行
  - �?camp-building 1 generation 基准里，当前 CUDA build **没有带来训练总耗时优势**，所以现阶段只能说“CUDA build 路径可运行”，不能说“GPU 已经显著加速训练�?

### 10. fixed-task memory ablation runtime 已修好并完成 full run�?026-05-21�?

- 已确认当�?fixed-task memory ablation 不再只是“入口可运行”，而是已经完成�?
  - 真实 genome/checkpoint 加载
  - 1-episode traceprobe
  - 4-episode fixed run
- 第一�?traceprobe 发现�?
  - `water_return` 已修�?
  - `worksite_return` 仍为 0
- 根因不是 `WorksiteLayer` 本体坏掉，而是�?
  - `GoalActionAssistCapability` �?agent 直接 pickup stick/stone
  - �?`DropCapability` 还能在同 tick 把刚拿到的材料立刻丢�?
  - `ThrowCarriedObjectCapability` 也会打断 stone logistics
  - `WorksiteReturn` 任务还允�?nursery logistics 干扰材料流向
- 已落地修法：
  - `GoalActionAssistCapability` �?`GatherStick / GatherStone / BuildWorksite` 下携带材料时优先�?remembered worksite，没�?worksite 再退�?remembered base
  - 新增 `GoalAwareDropCapability`，在 logistics/build goal 下不让普�?drop 路径�?stick/stone 立刻放掉
  - `ThrowCarriedObjectCapability` �?logistics/build goal 下抑�?stone throw
  - `MemoryAblationEval.cpp` �?`WorksiteReturn` 关闭 `nursery.enabled`
- 已完�?focused 回归测试、重新编译、traceprobe �?full run 验证
- 4-episode full run 关键结果（`data/runs/protagonist_memory_ablation_fixed/20260521-212657`）：
  - `water_return`：baseline / `no_episodic` / `no_social` = `162`，`no_spatial` = `67.5`，`no_goal` = `0`
  - `worksite_return`：baseline / `no_episodic` / `no_social` = `0.5`，`no_spatial` / `no_goal` = `0`
  - `signal_response`：baseline = `15.5`，`no_goal` = `0`，其余变体仍�?`12.25~13`
  - `danger_avoidance`：`no_episodic` 明显变差（`survived 8 -> 6.25`�?
- 当前判断�?
  - recent worksite bug 已真实修�?
  - `no_goal` 仍是最稳定的强 ablation
  - `no_spatial` �?`worksite_return` 上很干净，但�?`water_return` / `signal_response` 上还不够�?
  - 后续重点应转向解释与任务 tightening，而不是再证明入口能不能跑

### 11. 指标说明单页已补齐（2026-05-21�?

- 已逐个核对当前真实存在的指标输出与口径�?
  - `fitness.csv`
  - `evolution.csv`
  - `protagonist_behaviors.csv`
  - `memory_ablation_results.csv`
  - `module_metrics.csv`
  - `advanced_stats.csv`
  - `emergence_audit.csv`
  - `comprehensive/*.csv`
- 已新增可直接双击打开的单页说明：`supporting/METRICS_FIELD_GUIDE.html`
- 页面内容已按模块整理�?
  - 训练主指�?
  - 主角行为指标
  - 记忆 / DNC / Ablation
  - 生态模块指�?
  - 研究型统�?
  - 关键参数与实战判读模�?
- 这次特别澄清了几类最容易误读的字段：
  - `signal_emits` vs `signal_emit_events_avg`
  - `completed_worksites` vs `worksite_completion_events`
  - DNC 摘要字段“只能辅助解释，不等于记忆已有效帮助任务�?

### 12. memory ablation 任务 tightening + peer_signal_nearby bug�?026-05-21�?

commit `02d5a08`，针对前一�?full run 留下的两个分离弱�?

- **water_return**：旧 `spawn_radius=26` �?`kWaterSenseRadius=36` 内永远可�?+ `hydration_decay` 单位�?absolute/s 不是 ratio/s。改 spawn=42、initial=0.65、decay=0.6 �?baseline drank=207.5 / no_spatial=90.25（~56% drop, 7/8 干涸）。SpatialMemory 因果证据干净�?
- **signal_response**：改 emit 间隔 6s�?0s、emitter 拉近�?nest 边缘、response 半径 10�? / window 30s�?5s。同时修了一个真 bug �?`peer_signal_nearby` 只看�?tick emit�?0s 间隔�?GoalManager 几乎永远错过，listener 不会�?RespondSignal goal。改成扫 `last_signal_emit` + window 过滤�?events 从全 0 �?baseline 1.5 / no_spatial 1.75。fitness 维度 baseline 7.95 / no_spatial 4.37（~55% 残余）�?
- **遗留**：signal_response events 维度仍弱�?*根因不是任务，是 fixed genome 训练时没�?signal_response 奖励 �?policy OOD**。water spatial 分离干净是因�?worksite_return 训练里用�?spatial，能力部分泛化；signal 能力 task-specific，无法泛化�?

下一步：要么重训�?signal_response 奖励�?genome，要么在 ablation summary �?signal_response 标记�?"OOD evaluation, fitness-level separation only"�?

### 13. Production World Lifecycle Spec 落地�?026-05-22�?

用户提出关键的本体论质疑：如果主世界要支�?R-007 自动晋升闭环，那"出问�?是不是会导致活体集体消失、训练完再集体回来？这与 R-002 持续运行 + R-006 个体级生命周期天然冲突�?

**结论已固化为 spec**：`supporting/PRODUCTION_WORLD_LIFECYCLE_SPEC.md`

核心立场�?

- **主世界永�?reset**
- **训练永远在别处做**
- **新血脉只能通过出生或迁徙进入主世界**

物理图景�?

- 三层世界并存：训练沙盒（用户不可见、episode 化）/ 边境试运行世界（次要可见、连续运行）/ 主生产世界（主要可见、永�?reset�?
- 个体进入主世界只有两条合法路径：父母在主世界繁殖（主路径�?+ 从边境迁徙过来（辅路径，单次 1-3 个、地图边缘出现）
- 个体离开主世界只有四条合法路径：老死 / 病死 / 战死 / 长期生理压力�?
- incident �?challenge �?训练 �?边境 �?迁徙 �?主世界路�?A 复用，构成完整闭�?
- 任何"集体消失 / 集体出现 / 主世�?reset"都是被工程上禁止的非法路�?

工程意义�?

- 这份 spec �?R-002 / R-006 / R-007 / 3D viewer 四条需求的共同地基
- 任何与本 spec 不变量冲突的设计不允许进入实现阶�?
- 当前代码距离 spec 目标态很远，主世�?/ 边境世界 / 迁徙入口 / incident 录制 / 谱系持久化都还没开�?
- 已在 spec § 11 写出后续 W1~W6 工作顺序，对�?`DEVELOPMENT_EXECUTION_GUIDE.md` Stage C / D / E

**灭绝边界补丁（同日扩展）**：用户追�?种群没来得及繁衍就灭绝怎么�?，已�?spec 新增 § 4.4 灭绝场景处理 + § 7 新增 7 条灭绝相关不变量（共 17 条）。覆盖：

- 4.4.1 迁徙者灭绝（最常见，设计上接受，可重试，连�?N 次失败升�?incident�?
- 4.4.2 主世界某 lineage 灭绝（真实事件，可从 archive fork 重训，但重训版是�?`ancestral_archive_id` 的新 lineage，必须走 frontier �?migration 重新引入�?
- 4.4.3 整个物种灭绝（关�?incident，主世界不立即填空，留一�?day-night + 季节窗口的生态反应时间）
- 4.4.4 主角物种全灭绝（戏剧高光，主世界仍然连续运行，不黑屏，不"训练中请等待"，archive 里的�?protagonist 通过 frontier 重新孵化�?

补丁后的核心立场�?*灭绝是合法的世界叙事，不是失败状态；archive 永久保留但不能直接复活；任何灭绝都不会让主世�?reset�?*

### 14. R-008 自长机制精确�?spec 落地�?026-05-22�?

用户追问"如果不给喝水 capability，agent 能不能进化出喝水？文献支持到哪？" 经查 Baker 2019 hide-and-seek / Wang 2023 Voyager / Sims 1994 / DIAYN / Option-Critic / Lazaridou-Foerster emergent communication 等核心文献，结论�?*任何文献都没有证�?agent 能在没有底层 primitive 情况下凭空创造新 capability**�?

**结论已固化为 spec**：`supporting/SELF_GROW_CAPABILITY_LIMITS_SPEC.md`

7 层定义：

- L0 参数微调 / L1 结构生长 / L2 策略组合：默认可达，不算"自长"
- L3 策略涌现（hide-and-seek 6 阶段那种）：可达，最低门�?真自�?
- L4 技能分化（DIAYN / Option-Critic）：需 brain 架构�?modular 空间
- L5 通信协议涌现（emergent communication）：需通信任务立起�?
- L6 组合性能力（Voyager / Sims）：需 capability 改原子化
- L7 凭空�?capability / 新物理：**物理不可�?*，必须人�?

R-008 期待逐项落层�?

- "自己长出新技�? �?L3 + L4，可�?
- "行为分化 / 局部模块化" �?L4，可�?
- "通信 / 协作模式" �?L5，可�?
- 隐含期待"自己长出�?capability" �?L7�?*不可�?*

硬规则：

- 任何"自长 X"表述必须指明 Layer 编号
- L7 必须人写，不在任何文档承�?
- 任何机制证据链必须包�?`target_layer: L0~L7` 字段

三条路线�?

- A · capability 通用化（�?1-2 个改原子，L6 实验候选）�?推荐
- B · LLM-as-author（Voyager 风格）❌ 违背神经进化主线
- C · 修正 R-008 期待写进 spec �?必须�?

---

## 进行�?

### Phase 2 · Memory V2（基础接线已恢复，先冻结）

- 已实现：`SpatialMemoryBank` / `EpisodicMemoryBank` / `SocialMemoryBank` / `GoalMemory`
- 已接入：`MemoryCognitionPerception`
- 已完成当前编译错误修复，`neural-eco-protagonist` 重新可编�?
- 已重新跑�?protagonist smoke
- 当前决策已更新：先不直接跳到完整 `ProtagonistBrainV3`，而是�?`supporting/MEMORY_PHASE_A_EXECUTION_PLAN.md` 先把现有 Memory V2 接活，再向更大脑结构推进

### Phase CT · 连续时间世界底座（当前项目重点）

当前准备�?`supporting/CONTINUOUS_TIME_WORLD_DESIGN.md` 推进�?

- CT-0：时间类型与兼容层（已完成）
- CT-1：runtime �?`episode_ticks` 迁到 `episode_duration_seconds`（已完成�?
- CT-2：首�?capability 连续持续时间化（进行中）
  - 已完成：`move / drink / signal` �?`dt` 乘法语义接�?
  - 已完成：`EatCapability + ResourceLayer` 改为�?`eat_rate_per_second * dt` 的部分消�?持续摄食
  - 已完成：`SurvivalStatusLayer / InterventionDirectorLayer` 残留 `per_tick` 口径切到 `per_second`，并保留 legacy 配置兼容读取
  - 待继续：`chop / build / worksite credit / event queue` 的更强连续化表达

### Phase B · DNC-forcing micro-task（已完成最小验证，2026-05-21�?

- 已新�?route-local `MemoryTrialLayer`：episode 开头短暂给�?one-hot cue，随后把 protagonist 送到 choice hub，必须记�?cue 才能�?left/right 两个对称 target 中选对并拿�?fitness bonus
- 已新�?`MemoryTrialPerception`：只�?cue phase 暴露 2 维提示，不复�?`SignalLayer`，避�?agent 自己发信号把任务污染
- 已新增训练配置：`configs/protagonist_ecology_train_dnc_cue_choice.toml`
- 已新增最小单测：`tests/route_protagonist/test_memory_trial_layer.cpp`
- 已补 route config 回归测试：`tests/route_protagonist/test_protagonist_ecology_config.cpp`
- 当前刻意保持：`memory_cognition_enabled = false`、background = 0、其它生态层基本关闭，先把任务收敛成“DNC 记住瞬时 cue �?延迟选边”的纯记忆基�?
- 已完成最小验证：Release �?`test_core` / `neural-eco-protagonist` 重编通过；`MemoryTrialLayer.*` 3 个单测通过；短 smoke�? �?× 1 episode）可运行并产�?`fitness.csv / evolution.csv`
- 验证中顺手修复一处真实接�?bug：`ProtagonistEcologyConfig::fromConfig()` 之前漏读 `world.signal.*`，导�?`world.signal.enabled = false` 不生效、micro-task 配置会在 runtime 里触�?`action_dim` mismatch；现已补 parser 并加回归测试
- 当前结果：短 smoke �?`best_fitness = 24`、`min_fitness = -6`，与 `MemoryTrialLayer` �?success/failure reward 对上，说明任�?reward 路径已真正接通；但这还不是“DNC 已学会任务”的结论
- 尚未做：trace 指标专项解读，以及更长训�?ablation 去确�?DNC 是否真的在被�?

### Phase B+ · `ProtagonistBrainV3` 最小壳层（进行中，2026-05-21�?

当前约束已经明确：`total_input_dim_`、现�?6 组参�?buffer、`ProtagonistDenseGpu` 的矩阵布局、`ProtagonistEvolution` 的显�?crossover/mutate 接线，暂时都属于 **load-bearing compatibility surface**，所以这一步先�?*结构拆壳**，不�?`GenomeV2/V3` 升级�?

**已落�?*�?

- 新增 `brain/ProtagonistBrainV3Components.h` / `brain/ProtagonistBrainV3Components.cpp`
- `ProtagonistBrain` 现在开始持有并经由这些组件组织前向�?
  - `ObservationEncoder`
  - `MemoryQueryEncoder`
  - `BodyStateMemory`
  - `WorkingMemoryBank`
  - `BehaviorFusionTrunk`
  - `MotorCore`
  - `ActionHeads`
  - `InterfaceHead`
- `forward()` / `reset()` 已切到新壳层组件，但仍复用：
  - �?`total_input_dim_ = external_input + dnc_read_vectors`
  - �?6 组权�?/ bias buffer
  - �?DNC `decodeInterface()` 路径
  - �?optional dense GPU fallback 路径
- 已开始把壳层记忆折叠回现�?learnable path�?
  - `BodyStateMemory.summary()` 现在会加�?observation slice
  - `WorkingMemoryBank.summary()` 现在会加�?memory-query slice
  - 因此两者已经能通过�?`in_hidden_weights_` 影响 controller 前向，而不需要先升级 genome buffer
- 已继续把 working memory 往下游 action path 再接一层：
  - `WorkingMemoryBank.summary()` 现在还会调制 `motor_target`
  - 因此 working memory 不只影响 trunk input，也会继续影�?`MotorCore -> ActionHeads / InterfaceHead`
- `routes/protagonist_ecology/CMakeLists.txt` 已把 `ProtagonistBrainV3Components.cpp` 接入 route �?
- 已补最小壳层测试：`tests/route_protagonist/test_protagonist_brain_v3_shell.cpp`
  - 只验�?`forward()` 能产出动作，并且 `WorkingMemoryBank / BodyStateMemory` 壳层对象存在
  - 不对学习效果或行为结论做任何判断
- 已补 learnable path 护栏测试�?
  - `ProtagonistBrain.FeedsBodyAndWorkingMemoryIntoLearnablePath`
  - 用同 seed 的两�?brain 对照，只预热 body/working memory，要求动作输出发生变�?
  - `ProtagonistBrain.FeedsWorkingMemoryIntoLearnablePath`
  - 只预�?working memory，要求动作输出也发生变化

**当前状�?*�?

- 这批 BrainV3 改动已完�?*最�?build/test 验证**�?
  - Release �?`test_core` / `neural-eco-protagonist` 重编通过
  - `ProtagonistBrain.ExposesMinimalBrainV3ShellState` 通过
  - `ProtagonistBrain.FeedsBodyAndWorkingMemoryIntoLearnablePath` 通过
  - `ProtagonistBrain.FeedsWorkingMemoryIntoLearnablePath` 通过
  - `MemoryTrialLayer.*` �?`ProtagonistEcologyConfig.ParsesSignalSection` 关键回归保持通过
- 此外已补一�?**DNC cue-choice �?smoke**�?
  - 使用裁短后的 `protagonist_ecology_train_dnc_cue_choice_brainv3_smoke.toml`
  - �?repo 根启动，产物落在 `data/runs/protagonist_dnc_cue_choice_brainv3_smoke/20260521-110925`
  - `fitness.csv` 继续得到 `best_fitness = 24`、`min_fitness = -6`
  - `protagonist_behaviors.csv` �?DNC 指标继续非零
- 在接�?learnable path 后又补跑一轮同配置�?smoke�?
  - 产物：`data/runs/protagonist_dnc_cue_choice_brainv3_smoke/20260521-111856`
  - `Gen 1` 再次出现 `best_fitness = 24`
  - `fitness.csv` 这次�?`gen0=-6, gen1 best=24`，说明短 smoke 仍通，�?route 当前并非严格�?run 确定�?
- 在继续把 working memory 接到 `motor_target` 之后，又重跑一轮同配置�?smoke�?
  - 产物：`data/runs/protagonist_dnc_cue_choice_brainv3_smoke/20260521-113010`
  - 结果仍是 `gen0=-6, gen1 best=24`
  - 说明更深一层的兼容接线没有把任务链跑崩
- 现阶�?`BodyStateMemory / WorkingMemoryBank` 虽已进入现有 learnable path，但仍不是独立参数化的新主路�?
- 当前可以认为“结构拆壳成立”，但还**不能**认为 `BrainV3` 已完成更深层行为升级

---

## 待解决问题清�?

### P1 · Trace / Replay / Resume

- [x] episode trace 输出
- [x] protagonist checkpoint resume
- [x] checkpoint manifest 完整记录
- [ ] replay manifest �?3D 面板数据格式稳定�?
- [x] background species NEAT genome 序列化与 resume
- [x] RNG state resume
- [x] innovation tracker resume
- [x] generation index resume
- [x] generation+1 resume semantics
- [x] background speciation state resume

### P2 · Memory V2

- [x] 记忆系统总设计文档：`supporting/MEMORY_SYSTEM_MASTERPLAN.md`
- [x] Memory Phase A 接线计划：`supporting/MEMORY_PHASE_A_EXECUTION_PLAN.md`
- [~] WorkingMemoryBank（BrainV3 壳层与最小验证已完成，且已通过输入折叠方式进入现有 learnable path；仍未独立参数化�?
- [x] SpatialMemoryBank
- [x] EpisodicMemoryBank
- [x] SocialMemoryBank
- [x] GoalMemory
- [x] MemoryCognitionPerception
- [ ] ProtagonistGenomeV2
- [x] Spatial 语义拆槽与写入补�?
- [x] MemoryCognitionPerception 扩维�?Phase A 版本
- [x] GoalMemory 最小闭环（runtime rule-based goal refresh�?
- [~] episodic/social query 真接�?brain（已通过 `MemoryCognitionPerception` 做基础 readback，但 query 深度和专项任务仍弱）
- [~] memory ablation eval（smoke harness、fixed-task C++ runtime�?-episode traceprobe �?4-episode full run 都已完成，并确认 recent `worksite_return` fix 生效；当前剩余的是更强解释、trace 证据与任务分离收口）

### P3 · 因果奖励与合作信�?

- [ ] EventLineageTracker
- [~] ContributionLedger（worksite/hunt 两条已落地，signal credit �?lineage 仍缺�?
- [x] worksite credit 重构
- [x] hunt credit 重构
- [ ] signal credit 重构
- [x] 路过者不吃分验证

### P4 · 通信语义

- [ ] scout-worker partial observability task
- [ ] danger warning task
- [ ] build request task
- [ ] signal mutual information metrics
- [ ] signal ablation eval

### P5 · 开放式训练

- [~] BehaviorDescriptor（现�?`BehaviorCharacterization` 形式存在�?
- [x] NoveltyArchive
- [~] MAP-Elites archive（离�?archive / report 已有，尚未接�?reproduction�?
- [ ] EnvironmentGenome
- [ ] POETCoordinator

### P6 · World Model

- [~] trace dataset builder
- [ ] observation-action predictor
- [ ] prediction-error curiosity
- [ ] world model latent input

### P7 · 层级目标

- [ ] Manager head
- [ ] Worker head
- [ ] goal persistence
- [ ] long-chain task eval

### P8 · 连续时间世界框架

用户�?2026-05-20 提出：当前世界是 tick-by-tick 调度，agent 每固�?tick 做一次响应，看起来不像现实世界的连续时间过程�?

后续需要专门研究并设计�?

- [x] 连续时间世界设计文档：`supporting/CONTINUOUS_TIME_WORLD_DESIGN.md`
- [ ] `Tick currentTick()` �?`double currentTimeSeconds()` 的关�?
- [ ] `world.tick(dt)` �?`world.advance(dt_seconds)` 的迁移策�?
- [x] CT-0 时间类型与兼容层
- [x] CT-1 runtime seconds �?
- [ ] CTRNN protagonist brain：连续神经状�?`dx/dt`
- [~] 连续动作强度：move/drink/eat/chop/build/signal 不再只是离散按钮
- [ ] 事件异步触发：状态积分达到阈值触发，而不是只�?tick 边界触发
- [ ] reward 时间积分：按真实持续时间/强度结算
- [ ] �?tick runtime �?continuous runtime 并存对比

当前决策：连续时间框架已前置为当前项目重点；优先�?CT-0 / CT-1，再继续深化 Memory V2 / credit / CTRNN�?

### P9 · Compute Scaling

- [x] `supporting/COMPUTE_SCALING_GPU_CPU_PLAN.md`
- [x] `Agent` decision / apply 两相拆分
- [x] `AgentLayer` per-world agent decision 并行
- [x] protagonist route outer episode pool �?inner decision threads 联动调度
- [x] `ProtagonistBrain` 可�?dense GPU backend + route �?CUDA CMake 接线
- [x] 本机完整 CUDA toolkit / `nvcc` 可用性接通（portable 路径�?`D:\NVIDIA\CUDA\v12.8\`�?
- [~] protagonist dense GPU 路径真正编入并完�?smoke profiling（CUDA build 可运行，�?camp-building benchmark 未显示训练侧收益�?
- [~] CPU/GPU dense forward 对照 profile 与日志（已有 1-generation wall-clock 基准；仍缺更细粒�?dense-forward profiling�?

---

## 下一�?

1. `signal_response` 收口下一轮（如重启）：按 audit § 7.5 列出的方向之一推进——大幅放�?listener reward 量级（`signal_response_dense_reward 1�?` + `signal_response_reward 1.5�?`）、或�?SignalResponse ablation task 内把 `food_weight 4�?.5` �?food 不再霸占 fitness、或�?food spawn near emitter 让进 halo 与活下来不冲突、或软化 halo 12�?6m + window 15�?5s。当�?5 �?v3→v7 数据强烈指向：不�?perception/reward channel 缺口，是 task 全局比例失衡 + NEAT 30/60 代探索效率不足�?
2. �?`Build / Hunt / Signal` 三条 credit 统一成一�?route-local `EventLineage / Contribution` 口径，避�?communication 继续缺因果归因�?
3. 基于新的 communication 指标�?ablation，优先完成一条真的成立的通信任务（`scout-worker` / `danger warning` / `build request` 三选一先收口）�?
4. �?`supporting/METRICS_FIELD_GUIDE.html` 继续作为解释层基座维护，后续 communication / lineage / lifecycle 字段都要同步进去�?
5. �?cognition-social 主线没有收口前，不默认继续扩 `BrainV3`、GPU profiling、正式长训�?D replay �?live world�?


## 2026-05-30/31 拟真验证：env-perc 因果（OFF 对照 + 确定性多 seed 矩阵）

任务（拟真主线，非提速）：验证"环境感知 protagonist_environment_perception 在生存压力下是否被进化因果选择"。北极星=拟真度；A/B 必须零科学改动。inbox/outbox 协同：结论写 inbox，决策者回 outbox。

**Round 2（单 seed OFF 对照）**：与已完成 ON（seed50001 / dmg0.25 / cold30 / epg2 / 80代）逐行同配、仅关 env-perc 跑 OFF（data/runs/realism_pressure_long_off/20260531-015823）。结果：ON/OFF 双双 harmony 7/7；ON avg_fitness 末值 +74%、best +25%，但趋火(active_fires)/工地/造具无差异 → 诚实判"弱/弥散支持"。

**线程裁决（实测，回应"数学等价提速"建议）**：episode pool(num_threads) 数学等价（同 seed gen0 位元一致）但被 epg 封顶（epg=2→pool=2）；in-world agent_decision_threads>1 **非**数学等价（同 seed gen0 偏离 58%）→ 科学混淆量，禁入 A/B。源码 src/core/world/layers/AgentLayer.cpp:167-208 写回串行且按 agent index 有序，分歧来自并行 decide() 阶段的共享 scratch / beginTick 竞态（潜在正确性 bug）。

**Round 3（确定性多 seed 矩阵，推翻 round_2 假阳性）**：用确定性配置（agent_decision_threads=1 去竞态 + num_threads=2 pool 数学等价）跑 seed 50001/50002/50003 × ON/OFF 共 6 run（data/runs/realism_det_*）。结果：全 6 harmony 7/7；但 ON>OFF 跨 seed **不一致**——avg_fitness AUC 仅 1/3 seed ON 胜、me_coverage gen79 仅 1/3、存活 OFF 全 3 seed ≥ ON、趋火/工地/造具无 ON 优势。→ **判决：当前压力(dmg0.25/cold30)下 env-perc 未被因果选择**；round_2 的 +74% 是单 seed 噪声 + auto 非确定配置的假阳性，确定性多 seed 一跑即蒸发（方法论胜利，诚实>达标）。唯一弱信号：ON best_fitness 峰值在 50002/50003 显著更高(~13-14k vs ~7.6k)，但未扩散到种群级 avg/存活。

**CPU 吃满（用户第一优先级）= 实验级并发解决**：单 run 受 epg 封顶 + in-world 非等价双限只能 ~2 核；6 个独立进程并发 = ~10-11 核（单 run 5.5×）、VRAM 峰值 2.5/8.1GB 无 OOM、整矩阵 wall≈单 run。这是"32 核吃满"的正解（进程隔离、数学零风险），非 run 内线程。

**Round 4（进行中）**：决策门"ON≈OFF→加压"分支。单变量 thermal_damage_per_second 0.25→0.5、其余全冻结，同确定性多 seed 矩阵（data/runs/realism_detP2_*），保 harmony 7/7（C7 不灭绝为硬天花板）。用于判别根因"压力不足(b)"vs"编码太粗(a)"，二者都通向 Phase C。

**红线遵守**：未动 reward/HRL 权重、未动 epg、未碰 deep_protagonist；所有科学 run 一律 adt=1。配置：tmp/layer1/det_{seed}_{on,off}.toml、tmp/layer1/detP2_{seed}_{on,off}.toml。decide() 竞态修复列为 Phase C 工程项（非阻塞、低优先）。

---

## 2026-05-31 �����������ߣ�Round 4-final / 5 / 6 / 7���о���У��Ϊ"����"���� fitness��

**�о�У�����û�ָ�**��������֪������ı��������Զ�������ղ���"ON ��� OFF �� fitness"�������������ҹ/����/���ڵĸ�֪�Ƿ�ת��Ϊ"��ʵ����"����λ��������Ӧ��Ԥ��ʽ���Ǵ���е�ͺ�Ӧ����OFF �������Զ��ա�fitness ��Ϊ��¼�����ɱ�׼�� = г���ع���λ��ǰ��phase-lead��lead ���� +6s ��ΪԤ�У���

**Round 4-final��dmg0.5 ��ѹ = ��ѹ����֤α��**�������� thermal_damage 0.25 �� 0.5��6-run ȷ���Ծ���realism_detP2_*���������C1-C6 ������ C7���������6 ��ȫ FAIL ���� ������ dmg0.5 �¼���ÿ���������� 0-2����ON/OFF һ���������ɣ�ON �� corr(��,�ٶ�) Լ +0.3~0.4���� OFF һ���У��� gen0 ���� corr Լ 0.67~0.73 = ����е��ϣ�����ֱ��Ӱ���ƶ����������Ǹ�֪�����ۣ���������ѹֻɱ���ǡ���ɸ��֪���ų�"ѹ������(b)"��ƿ��ָ��"�ɴ���ѡ���ݶ�ȱʧ" + ���롣

**Round 5��A ��� + Step1��**���� trace ����"��"�� cold_temp_threshold=30 ���Ǻ㶨ȫ�ֺ�Ѫ��core_temp �˳�ʱ�������ֵ���޻�ů��Ϣ�������ʽ� dmg ���á�����Ӳ��������Step1 = �� cold_temp_threshold 30 ���� 22��ʹ��ֻ������ʱ�δ��� = ��ҹ/������λ�������ռ�ɹ�ܣ�Ӫ�� warmth_radius=14 �� core_temp ���� ambient+20����"�ɹ�ܵ͹�"��12 �� smoke��C7 ���ÿ����� 24-46/48����ÿ������ 5-24 ֻ = ��ʵѡ��ѹ���� trivially ȫ���

**Round 6��th22/dmg0.3 ȷ���Ծ��� + �׸���ʵ���ԣ�**��6-run��seed 50001/2/3 �� ON/OFF��80 ����adt=1+pool �ɸ��֣�realism_detP2b_th22_*����harmony ȫ 6 �� 7/7 PASS��C7 ��� 24-43/48��C6 �ص� 0.40-0.52�������ɣ�phase-lead ��׼�ߣ������ǣ�gen79 �� gen0��������ͨ�� ON �� core_temp ��͵�֮ǰԼ 11-12s ������������ֵ��3 seed һ�£�fire_phi Լ 0.95��3����OFF �ǵ����������𣨷�Ӧʽ/ͬ�ࣩ��gen0 �޽��ɡ�= �׸� ON ר���������ӿ�֡��� seed �ɸ��ֵ�"��֪�� -> ��ǰ����"Ԥ�н��ɡ���������������ֻ����������� R2 С�ڵ��� 0.08�����ͨ���Դ���е(ON=OFF)��

**Round 7���б���ϣ�rise vs plateau�������У�**������ round_6 ѡ����ˡ���ṹ���յĺ�ѡ 3 ���� ��ѡ���ӳ��� 160 ����������Ԥ�� R2/����Ƿ�������������������=���빻�á�����ѡ�񣩻��� plateau��ƽ=�������컨�塢�ſ���Ѫ���񻯣������̵㣺ԭ��� gen79 checkpoint ���ܣ������� full-resume У��Ҫ�󱳾����������������ó�ʼֵ����������Ⱥ�� borderland �����������ѱ䣨pack_hunter �� 450 �ȣ�������ֱ�ӱ����������������+�ر��루����ȫ��Χ�������߱��������ȳ����ܶ��ף��� adt=1 ȷ���� = ��ͷ�ܵ� 160 ����ȷ���� gen0-79 �����쵽 159����ѧ�ȼۡ����ã�tmp/layer1/detP2c_th22_{seed}_{on,off}.toml��generations=160��tail_generations=20 ץ gen140-159��max_episodes_per_generation=1 ʡ�̣������ realism_detP2c_th22_*�����ߵ㣺gen0/gen79��detP2b ���У�+ gen145/gen155������β�Σ�����Ѫ������ spec �Ѳ���Ԥ��ơ�������"�Ȳ��ύ"�ݴ������

**�������أ�������**��δ�� reward/HRL Ȩ�ء�δ�� epg��δ�� deep_protagonist�����п�ѧ run һ�� adt=1���������Σ�cold ��ֵ/dmg�����Ϸ���reward �ؼ�Ȩ��ֹ��������ά=��Ѫ��������ʽ PR + smoke����


---

**Round 7 EOD ��β��2026-05-31 UTC 07:35��**���������� outbox д EOD handoff��������β�����ٿ��³��ܡ����½����ĵ�����candidate 3��gen��160 rise/plateau ��ϣ���������С�δ���꣺6 run��3 seed �� ON/OFF��th22/dmg0.3��adt=1 ȷ���ԣ�realism_detP2c_th22_*��ȫ� gen 24-25/160������ = Ĭ�Ϻ�̨�����ܣ�WMI �ѹ������� SSH �Ự������������ִ����ֱ���������ȣ����ػ�/ͣ�磬adt=1 ȷ���Ա�֤�� gen0 �ȳ�������λ���֡���������ʧ��checkpoint �򱳾���������ƥ�䲻�ɱ��������ܣ���R2/phase-lead ���ߴ� gen140-159 trace ��ɺ��� build_aggs_p2c.ps1 + phase_harmonic.py ȡ��round_7 ������д inbox���Թ鵵��δ�� decision_round_7�������� note �Ѹ��µ� v6���´ε�һ�� = gen159 ȡ���� R2 ������ rise�����빻�á�����ѡ��/ plateau�������컨�塢����Ѫ�� cand1+2�������߲��䣨���� reward �ƽ��ɡ�������ά=��Ѫ����C7 �ذ塢harmony 7/7��adt=1 ȷ���ԡ�fitness ����¼����
