# STATUS.md · Neural Eco 全仓交接手册

> 最后巡检�?026-05-29 下午晚间 (UTC)�?*Trajectory-analysis pivot�?1/21 streak 后用户叫停跨 seed 验证，转�?agent 脑子里到底在干啥"的诚实评�?*）：用户判定继续累积同质 stage 4b/4c run 边际收益已小，且预言"agent 轨迹画出来肯定是无头苍蝇"。本会话围绕这条做了三块�?1) **TraceRecorder.tail_generations** (`d081fd0`) �?加配置字�?`trace.tail_generations`，让 TraceRecorder 可专门捕获最�?N 代（替代�?N 代），配合磁盘纪律只留训练完成的 gen 79 真实脑况；新�?`shouldTraceEpisode` overload 接收 total_generations，旧 call site 行为不变�?2) **Python 轨迹分析工具�?*：`scripts/protagonist_trajectory_analyze.py`（~660 行）�?agent_trace.csv，对每个 agent �?velocity_autocorr / direction_persistence / dominant_goal_frac / goal_switches / visited_cells / path_length / radius_of_gyration / food_pull_fraction / predator_push_fraction �?20+ 维度，聚合后输出 headless_score �?[0,1] + 三档 verdict (PURPOSEFUL / MIXED / HEADLESS_FLIES)，`--brained-only` 过滤�?183 个非 NEAT 背景物种只看 24 个主角脑；`scripts/protagonist_trajectory_compare.py` 做两�?JSON 间的 diff（aggregate / goal distribution shift / 5%/25%/50%/75%/95% 百分位分布）；`scripts/inspect_trace.py` 快速探查原�?CSV 结构�?3) **gen 0 vs gen 3 Run 10 (s58193) 数据初步对照**：gen 0 mean velocity_autocorr=0.943 / direction_persistence=0.947 / dominant_goal_frac=0.581 / goal_switches=5.5 / headless_score=0.164；gen 3 同维 0.824 / 0.816 / 0.631 / 5.9 / headless_score=0.236�?*两代 verdict 都是 PURPOSEFUL**——用户预测的"无头苍蝇"�?gen 3 阶段已初步证伪：autocorr 主动�?0.94 降到 0.82 = 脑子开始响应环境而非走死板直线，dominant_goal_frac 涨说明对单一目标承诺度增加，path_length 633�?78 说明实际行动量增�?*gen 79 数据缺失**：s71359 tail-mode run (�?seed, max_generations=0 + tail_generations=1) 跑到 gen 38 / ~50% 时被用户叫停（要玩游戏占 GPU），s71359 partial 数据已删�?*下一会话首要任务**：补一�?80-gen tail-mode run �?gen 79 trace �?trajectory_analyze �?gen 0/3/79 三点曲线，下�?verdict�?*磁盘纪律**：删�?s58193 1.5 GB trace + 60gen_2d_curriculum 149 MB + s71359 partial，D �?free ~250 GB；`data/runs/` 只剩 6 �?stage4b run（每�?~40 MB CSV+snapshot）�?*Run 10 (s58193) 7/7 PASS �?commit** `1f4ce28` �?21/21 streak（live-agent 时代第十次独�?MWH end-of-run 验证）。本会话 commit 链：`1f4ce28` (Run 10) + `d081fd0` (trajectory pivot) + �?commit (docs 同步)�?*未达�?*�?2/22 streak、gen 79 trajectory verdict、Layer 7 viewer（用户明�?先不要可视化"）�?
> 巡检范围：`d:/claude-code/c++` 整个工作�?+ `D:/NVIDIA/` + `D:/claude-code/downloads/`
> 当前分支：`master`
> 当前 HEAD（protagonist_ecology 视角）：�?commit 待写 (gen 79 trajectory verdict + docs/note sync)；当�?HEAD `c1f6e16` trajectory wrap-up；前 commit `d081fd0` trajectory-analysis pivot (TraceRecorder.tail_generations + Python analyzer 三件�? + `1f4ce28` Run 10 (s58193) 7/7 PASS + 21/21 streak。再�?`35e8535` stage 4c MVP / `c76a506` Run 5 stage 4c first validation / `ffd4187` Run 9 / `4c75aa3` Run 8�?

> **2026-05-30 上午晚些 (UTC) 第四�?run s63810 �?gen 79 PURPOSEFUL 第三次同 seed 复现�?5/25 streak**�?
> - **背景**：�? #1 结论已闭环（�?seed 稳健 PURPOSEFUL）。按 §0 铁律 6"一直跑"接的�?4 跑（seed 63810，gen79-only tail-mode 省盘），用来再加一个独立数据点验证不退步。本会话期间 Devin VM 被系统重启，�?run �?WMI 脱离 SSH 后台跑，照常跑完�?6:30�?8:13 UTC，~103 min）�?
> - **执行**：`tmp/layer1/long_run_80gen_stage4b_s63810_tail.toml`。`MainWorldHost end-of-run cumulative sim_time=800.000s tick_calls=800`（invariant #1 �?N 次验证）；`TripleWorldRuntime end-of-run borderland=288 main_world=338`；stage 4c snapshot 219 alive agents（cumulative_seed_count=320）�?
> - **harmony 7/7 ALL PASS** �?**25/25 streak**。C5 worksites peak=2/gen total=139，C6 me_cov peak=0.440 qd=50994，C3 coop peak=1954.5s，C7 无灭绝�?
> - **gen 79 (24 brained, ep0)**：headless_score **0.244 �?PURPOSEFUL**，和 s81527(0.245)/s92641(0.285) 完全同型。dominant_goal_frac **0.729**（四轮最高，越练越专注主目标）、predator_push **0.701**（四轮最强，天敌规避）、food_pull 0.482、velocity_autocorr 0.770（反应式）、goal_switches 7.58（低，和 s81527 �?8.1 同档，远低于 s71359 �?seed �?16.3）�?
> - **�?seed gen 79 汇�?*：s81527=0.245 / s92641=0.285 / s63810=0.244�?*三个干净�?seed run �?PURPOSEFUL**�? s71359=0.354（跨 seed，唯一 MIXED 且刚过线）�?*3/4 干净 run PURPOSEFUL，结论进一步加固：PURPOSEFUL，确定不是无头苍�?*�?
> - **磁盘**：trace 已删（agent_trace 33 MB + event_trace 200 MB cap 命中），产物 tmp/traj_s63810_gen79.json。commit 待写�?
>
> **2026-05-30 早上 (UTC) 第二 seed 交叉验证 �?gen 79 PURPOSEFUL 复现，结论稳�?*�?
> - **动机**：s81527 �?seed �?gen 79 判成 PURPOSEFUL，但要排�?这只�?81527 一�?seed 的运�?，再用全�?seed 92641 同样�?gen 0/3/79�?
> - **执行**：`tmp/layer1/long_run_80gen_stage4b_s92641_traj3pt.toml` WMI 后台跑完�?4:40�?6:16 UTC，~96 min）。`MainWorldHost end-of-run sim_time=800.000s tick_calls=800`；`TripleWorldRuntime end-of-run borderland=288 main_world=338`�?*harmony 7/7 �?24/24 streak**（C6 qd=67154、C5 total=148、C3 peak=2528.3s、C7 无灭绝）�?
> - **s92641 gen 0/3/79**：headless 0.149 �?0.232 �?**0.285，三代均 PURPOSEFUL**（都 < 0.35）。dominant_goal_frac 0.64�?.69、predator_push 0.39�?.65、velocity_autocorr 0.94�?.73、goal dist explore 53%�?5% / emit_signal 18%�?9% —�?�?s81527 完全同型�?
> - **�?seed gen 79 汇�?*：s81527=0.245 / s92641=0.285 / s71359=0.354（跨 seed，仅这条 MIXED 且刚过线 +0.004）�?*2/3 干净�?seed run �?PURPOSEFUL，结论稳健：PURPOSEFUL，确定不是无头苍�?*。稳定信号（dom_goal_frac↑、predator_push↑、food_pull 稳、autocorr�?反应式）�?seed 一致；唯一 seed 敏感项是 goal_switches�?.1/13.4/16.3），正是它把 s71359 顶过 MIXED 线�?
> - **food_eaten**：s92641 gen79 2/24 吃到（sum 1.2），同样非恒零，确认上次�?seed=0 是采�?artifact �?bug。未�?reward�?
> - **磁盘**：trace 已删，D free ~231 GB。产�?tmp/traj_s92641_gen0/3/79.json + docx/.../traj_3seed_verdict.md。commit 待写�?*§3 #1 trajectory honesty 评估彻底闭环**�?
>
> **2026-05-30 凌晨晚些 (UTC) �?seed gen 0/3/79 复核 �?gen 79 改判 PURPOSEFUL，food_eaten 疑点决起**�?
> - **动机**：上一�?gen 79 = MIXED 是跨 seed 对比（gen0/3 �?s58193，gen79 �?s71359），只能作趋势参考。本 run 用同一 seed (81527) 一次抓 gen 0/3/79 三点（`trace.max_generations=4` + `tail_generations=1`）做干净�?run 对比，并直接�?food_eaten�?
> - **执行**：`tmp/layer1/long_run_80gen_stage4b_s81527_traj3pt.toml` WMI 后台跑完�?2:44�?4:27 UTC，~103 min，~80s/gen）。`MainWorldHost end-of-run cumulative sim_time=800.000s tick_calls=800`；`TripleWorldRuntime end-of-run: borderland=288 main_world=338`�?
> - **harmony 7/7 ALL PASS** �?**23/23 streak**。C5 worksites total=152，C6 me_cov peak=0.440 qd=54101，C3 coop peak=3131.5s，C7 无灭绝�?
> - **�?seed gen 0/3/79 (24 brained, ep0)**：headless_score 0.181 �?0.195 �?**0.245**�?*三代�?PURPOSEFUL**（都 < 0.35）。上条的 MIXED (0.354) 是跨 seed artifact，不是真退化。关键：dominant_goal_frac 0.62�?.65�?.69�?*越来越专注于主目�?*）；predator_push 0.42�?.41�?.66（天敌规避变强）；velocity_autocorr 0.90�?.79（从死直线变反应式，是好事）；goal_switches 只升�?8.1（跨 seed 那次�?16.3 = seed 特性）�?
> - **food_eaten 疑点决起**：同 seed �?food_eaten sum gen0=1.8 (3/24) / gen3=10.2 (9/24) / gen79=3.6 (6/24) —�?**不是恒零**，逐代有波动。上次跨 seed gen79=0 是单 episode/seed 采样 artifact，非感知→进�?bug。protagonist 是协调专业型（emit_signal 主导 68%）非主觅采食者，感知到食物会走过�?(food_pull 0.45)�?*未改 reward**（�? 铁律 2：查 bug 不调奖励，且这里�?bug）�?
> - **goal 分布 (�?seed gen0→gen79)**：explore 41%�?9%，emit_signal 33%�?8%，respond_signal 23%�?%——同一 seed 内确认了从乱逛转向信号协调�?
> - **磁盘纪律**：trace 已删，D �?free 232 GB。产�?tmp/traj_s81527_gen0/3/79.json + traj_s81527_compare_0_79.md + compare_0_3.md + traj_s81527_verdict.md�?
> - **结论**：诚实判�?**PURPOSEFUL（不是无头苍蝇）**。跨整个 80 �?MWH run，brained protagonist 保持高目标承诺、响应食�?天敌、转向协调信号�?

> **2026-05-30 凌晨 (UTC) Layer 6 R3 trajectory honesty eval �?gen 79 verdict 拿到�?无头苍蝇"被证�?*�?
> - **背景**：上一会话留的最高优先级�?= gen 79 trajectory 没拿到（s71359 tail run 上次跑到 gen 38 被用户叫停占 GPU 玩游戏）。本会话接班把它补上�?
> - **执行**：`tmp/layer1/long_run_80gen_stage4b_s71359_tail.toml`（evolution.generations=80 + trace.max_generations=0 + trace.tail_generations=1，只�?gen 79）WMI 后台跑完�?0:52�?2:22 UTC，~90 min，~75s/gen）。`MainWorldHost end-of-run cumulative sim_time=800.000s tick_calls=800` �?N 次实证主世界永不 reset (invariant #1)；stage 4c snapshot �?219 �?agent (cumulative_seed_count=320)�?
> - **harmony_check 7/7 ALL PASS** �?22/22 streak。C5 worksites total=151 peak=2/gen，C6 me_coverage peak=0.440 qd=83887，C3 coop peak=3337.0s，C7 无灭绝�?
> - **trajectory_analyze gen 79 ep0 (24 brained agents, --brained-only)**：headless_score=**0.354** �?verdict **MIXED**（阈�?PURPOSEFUL<0.35 / MIXED 0.35-0.55 / HEADLESS>0.55，刚过线）。聚合：mean_velocity_autocorr=0.660 / direction_persistence=0.665 / dominant_goal_frac=0.608 / goal_switches=16.3 / food_pull_fraction=**0.540** / predator_push_fraction=**0.688**�?
> - **诚实结论**�?*不是无头苍蝇**。反应性指标强�?4% 感知到食物朝它走�?9% 见捕食者逃�?1% 单目标承诺）。headless_score 升到 MIXED 几乎全因 goal_switches 5�?6，但这是�?explore 大降、信号协调主导背景下，更像快速切换有目的子目标而非混乱�?
> - **�?seed 趋势对照**（gen0/gen3 = s58193 seed58210，gen79 = s71359�?*�?seed 仅作趋势参考、非�?run**）：velocity_autocorr 0.943�?.660（不再走死直线，开始响应环境）；goal 分布 explore 43%�?2%、emit_signal 29%�?0%、respond_signal 24%�?0%（从乱逛转向信号通信协调）；bbox_area 80582�?9916（活动收�?4x，围�?工地聚集非散开乱飞）�?
> - **诚实保留**：gen79 ep0 brained agent food_eaten_final mean=0.000（会朝食物走但这一局没真吃到）—�?标注存疑，后续跑盯�?
> - **磁盘纪律**：分析完立即�?trace dir�?56 MB），D �?free 232 GB。分析产物留 tmp/traj_gen79_ep0.json + tmp/traj_compare_0_79.md + tmp/traj_compare_3_79.md�?
> - **commit**：`protagonist_ecology: gen 79 trajectory verdict (MIXED, reactive-purposeful) + harmony 22/22 + docs/note sync`�?

> **2026-05-28 下午 Layer 6 R3 stage 4 MainWorldHost continuous tick �?FINAL_BLUEPRINT #3 落地 + 七连 7/7 PASS �?80-gen**�?
> - **背景**：用户路线图 "1�?�?�?�?"�?1 (MigrationScheduler sandbox→borderland) + #2 (NEAT graduate AB) 已完整闭环并经六�?7/7 PASS 验证�?3 main world continuous tick �?FINAL_BLUEPRINT 最大遗留模块，核心�?主世界永�?reset (PRODUCTION_WORLD_LIFECYCLE_SPEC invariant #1)"。spec § 6 划分了三层优先级：priority 1 = sandbox→borderland migration (done)，priority 2 = main world 内存中不 reset (本次)，priority 3 = disk 序列�?(deferred)�?
> - **MainWorldHost class (`67e83a4`, 230 行新代码)**�?
>   - `routes/protagonist_ecology/runtime/MainWorldHost.{h,cpp}` 新建：constructor �?`ProtagonistEcologyConfig const&`，内部用 `createBootstrapWorld(route)` 构造一�?`std::unique_ptr<World2D>`�?
>   - 提供 `tick(SimTimeSeconds dt)`：dt > 0 时调 `world_->tick(float(dt))` + 自增 tick_call_count_，dt �?0 noop �?false�?
>   - 提供 `simTimeSeconds()` 委托给底�?World2D 的单调时钟，`tickCallCount()` 自己计数，`world() const/non-const` 暴露 World2D 指针（被测试 / 未来 migration hook 用）�?
>   - 不可拷贝 / 不可移动（host �?unique owner）�?
> - **5 个新 gtest (`tests/route_protagonist/test_main_world_host.cpp`)**�?
>   - `ConstructionYieldsWorld`：构造后 `world() != nullptr`, `simTimeSeconds()==0`, `tickCallCount()==0`
>   - `TickAdvancesSimTime`：tick(0.1) + tick(0.25) �?sim_time 严格单调上升
>   - `NonPositiveDtIsNoop`：tick(0) / tick(-1) 不改 sim_time 也不增计�?
>   - `NeverRecreatesWorldAcrossManyTicks`�?0 �?tick(0.05) �?`world()` 指针完全等于初始指针�?*这是 § 1.3 "永不 reset" invariant 的结构化测试**�?
>   - `SimTimeMatchesWorldClock`：host.simTimeSeconds() == world()->currentTimeSeconds()，确认无双重簿记
> - **训练循环接入 (`e5e67dd`, 5 文件 / 125 �?/ 1 �?**�?
>   - `ProtagonistEcologyConfig.h::TripleWorldConfig` �?3 字段：`main_world_continuous_tick_enabled{false}` / `main_world_tick_dt_seconds{1.0}` / `main_world_ticks_per_generation{10}`
>   - `ProtagonistEcologyConfig.cpp` �?`config.getOr<>()` 解析三�?
>   - `ProtagonistEcologyRuntime.cpp::runTraining()` �?`triple_world` 构造后�?`std::unique_ptr<MainWorldHost> main_world_host` 持有（gate off 时不构造，零成本）。每�?generation 末尾（在 checkpoint save 之后、`continue_training` 检查之前）�?`main_world_host->tick(dt)` × `ticks_per_generation`，LOG_INFO 输出 `MainWorldHost: gen X advanced N ticks (dt=Ys, cumulative sim_time=Zs tick_calls=K)`。运行结束输�?`MainWorldHost end-of-run: ...`�?
>   - 2 个新 config 测试覆盖 defaults-off + toml 解析两条路径
>   - 修复了一�?forward-decl `class ProtagonistEcologyConfig` �?`struct ProtagonistEcologyConfig` �?MSVC LNK2019 mangling mismatch
> - **rebuild + 全部测试 PASS**：`vcvars64.bat` (`C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat`) + `D:\claude-code\downloads\ninja\ninja.exe -C build_ninja_cuda -j 8 test_core neural-eco-protagonist` �?5+7 unit + 1 link = OK，`bin\test_core.exe` 跑出 `[==========] 225 tests from 57 test suites ran` + `[  PASSED  ] 225 tests`�?
> - **首次 80-gen curriculum-v2 跑完�?(PID 52972, 12:02:19�?3:53:25 UTC, ~111 min)**：harmony_check 输出 ALL PASS�?
>   - C1 hunters: ambush_predator>=3, pack_hunter>=11, apex_predator>=2 PASS
>   - C2 herbivores: rabbit 22-26 / large_herbivore 20-79 / small_forager 13-14 PASS
>   - C3 coop peak=2059.0s PASS
>   - C4 scavenger pop 6-7 fit 1413 / omnivore pop 16-38 fit 5088 PASS
>   - **C5 worksites total=137 peak=2/gen** �?七轮最�?(curriculum-v1=88, structural-v1=110, structural-v2=113, baseline-v2=40)
>   - **C6 me_coverage peak=0.480 qd=55305** �?历史最高并�?curriculum-v1
>   - C7 all archetypes survived PASS
> - **80-gen 释义**：六�?60-gen 7/7 PASS 已经横向验证 fix �?5 config 通用；本�?80-gen 纵向验证 fix + cap 在更长训练下持续生效，且 C5 / C6 还在继续涨而不是塌缩（说明 protagonist �?60�?0 代之间仍在学新行为，graduate pipeline 还有上行空间）�?
> - **下一�?*�?a) Layer 6 R3 stage 4 后续 commit：把 borderland→main_world genome promotion 链路扩展�?�?MainWorldHost �?World2D 里也 spawn 一�?live agent"，目前只�?genome 不搬 agent�?b) Layer 7 R3 stage 2 Godot viewer�?c) 启动 main_world_continuous_tick_enabled=true 的长 run �?host log 行真实输出�?

> **2026-05-28 早晨 disk-budget 二次硬化 + structural-mutation AB v2 (7/7 PASS) + �?run 编译管线恢复**�?
> - **背景**：凌晨连�?graduate-AB-v2 (curriculum) + structural-mutation AB v2 各一轮，每轮 trace 输出 ~76 GB�?0 gen × 2 ep × ~750 MB `event_trace.jsonl`），D: 盘从 244 GB 跌到 92 GB free�?*消�?152 GB，已逼近爆盘**）。前�?D-044 �?CSV（CreditLedger / CommunicationMetrics / `_signal_debug.csv`）加�?200 MB cap，但 trace 子系统漏配——`shouldTraceEpisode()` 只按 `max_generations / max_episodes_per_generation` gate，没�?byte-cap，配�?`max_generations = 60` 就爆�?
> - **代码修复**（pending commit，已编译验证通过）：
>   - `routes/protagonist_ecology/runtime/TraceRecorder.{h,cpp}`：新�?`TraceRecorderConfig::max_bytes_per_file{200 MB}` 字段 + `TraceRecorder::{event,agent,world,viewer}_cap_warned_` 四个 once-flag�?
>   - 新增 `checkAndCapStream(stream, warned, path, cap)` 工具方法：`stream.is_open() && tellp() >= cap` 时立�?`stream.close()` + 单次 `LOG_WARN` 一行（D-044 同款 once-warn）�?
>   - 4 �?write 点全部前置守门：`writeEvent` (event_trace.jsonl) / `writeAgentSnapshot` (agent_trace.csv) / `writeWorldSnapshot` (world_trace.csv) / `writeViewerFrame` (viewer_frames.jsonl)�?
>   - `loadTraceRecorderConfig()` �?`trace.max_bytes_per_file = N`（默�?200 MB，写 0 即关 cap）�?
> - **配置回滚**（pending commit）：4 �?`tmp/layer1/long_run_60gen_2d{,_curriculum,_structural,_throw}.toml` �?`[trace].max_generations` �?60 �?4（已验证：harmony_check 只读 CSV 不读 event_trace�? �?trace 足够诊断回放），并叠 `max_bytes_per_file = 209715200` 兜底。其�?`_throw.toml` / 基础 `_2d.toml` 之前未入 git（`?? long_run_60gen_2d{,_throw}.toml`），改动留本地�?
> - **手工清理**：`Remove-Item D:\claude-code\c++\data\runs\long_run_60gen_2d_{curriculum,structural}\20260528-*\trace\generation_{2..58}` 删除中间代，仅保�?gen 0/1/59；释�?~144 GB，D: 重回 ~244 GB free�?
> - **structural-mutation AB v2 结果**（curriculum + structural-mutation �?60-gen 都收齐前）：harmony_check **7/7 PASS**，C5 worksites total **110** (curriculum 88、baseline 38)，C6 me_coverage peak **0.44** (curriculum 0.48、baseline 0.40)。结构�?NEAT mutation (`borderland_mutate_add_conn_rate=0.10 + add_node_rate=0.04`) �?fix 后的 graduate pipeline �?*继续不退步且 build 数还�?*�?
> - **编译管线恢复**：磁盘清理副作用导致 `build_ninja_cuda/` 几乎被清空（只剩 `lib/`+`routes/`，binary + build.ninja 都没了）。用 Clash proxy (`127.0.0.1:7897`) �?git 全局 http/https.proxy 重新�?_deps，cmake reconfigure 76s + ninja incremental rebuild 74s �?`test_core.exe` + `neural-eco-protagonist.exe` 新出炉（2026-05-28 07:44:28）。`test_core.exe` **218/218 PASS**（含我新加的 TraceRecorder cap 代码，无回归）�?
> - **新长跑就�?+ 验完�?(PID 14576, 07:45:13�?9:00:09 UTC, 75 min)**：trace 实时�?cap 关——log 里出�?2 �?`[warning] TraceRecorder: closed ...event_trace.jsonl after reaching disk-budget cap 200 MB`，cap 实战生效。结束后 run_dir �?**1.73 GB**（vs 旧版 ~90 GB �?**52 倍降�?*），D: free 244�?43 GB 没动�?
> - **第三�?60-gen harmony_check 7/7 PASS** (structural-mutation v2 with disk-cap)：C1 hunters PASS / C2 herbivores PASS / C3 coop peak=2766s / C4 scavengers PASS / **C5 worksites total=113** (最高，curriculum-v2=88、structural-v1=110、baseline=38) / **C6 me_coverage peak=0.400 qd=40882** / C7 no extinction�?*disk-cap 不影响训练结果，�?v1 完全可比**�?
> - **第四�?60-gen 完成 (PID 46200, 09:04�?0:27 UTC, 84 min)**：harmony_check **7/7 PASS** (curriculum-v2 with disk-cap)。C5 worksites total=109、C6 me_coverage peak=0.400 qd=52804、C3 coop 2165s、C4 omni fit 5072�? �?cap warnings 触发，run_dir 1.74 GB�?
> - **第五�?60-gen 完成 (PID 52936, 10:30�?1:12 UTC, 42 min)**：harmony_check **7/7 PASS** (throw_penalty v2 with fix + cap)。但**指标显著降级**：C5 worksites total=**34** (vs curriculum/structural-v2 109-113)、C6 me_coverage peak=**0.320** (其他 4 �?0.40-0.48)、C3 coop peak=1680s (其他 2165-2766s)、C4 omni fit=3443 (其他 3443-5072)�?*关键发现**：throw_penalty `-0.05` 仍然抑制 build/coop 行为，但 fix 把它�?5/7 负优�?救到"7/7 PASS 但低�?——证�?**Stone Bootstrap commit 4 fix 是通用根因修复，不�?graduate 特例**。throw_penalty �?reward shaping 副作用而非 NurseryProgression bug�?
> - **第六�?60-gen 完成 (PID 43996, 11:15�?1:59 UTC, 44 min)**：harmony_check **7/7 PASS** (clean baseline `long_run_60gen_2d.toml` with cap，无 archetype_progression_persistence / curriculum / structural / throw 修饰)。C5 worksites total=**40**、C6 me_coverage peak=**0.440** qd=66394、C3 coop 1986s、C4 omni fit=3265、protagonist gen 59 avg=**3223** (六轮最�?�? �?cap warnings 触发�?*关键观察**：无 progression_persistence �?worksites 较低 (40 vs graduate 88-113) �?me_coverage 更高 (0.440 vs 0.400)，protagonist fitness 也更高——这�?graduate pipeline �?baseline pipeline 的天�?trade-off�?
> - **六轮 60-gen 全部 7/7 PASS** ✅。fix + cap �?baseline / curriculum / structural / structural+mut / throw_penalty 五种不同 config 下都稳定�?
> - **第七�?80-gen 已起�?*�?2:02:19 UTC �?PID 52972 跑新建的 `long_run_80gen_2d_curriculum.toml` (curriculum-v2 with cap，`generations=80`)。目标：(1) 验证 fix+cap �?60�?0 代延长下继续稳定�?2) 验证 ME-Elites coverage / worksite count 在更�?gen 下是否继续累积；(3) 满足用户"训练跑到 gen 70-80+"诉求。预�?~14:00 UTC 完成�?
> - 下一节点：等第七跑完�?�?�?**#3 main world continuous tick** (FINAL_BLUEPRINT 最大遗留模�? �?主世界永�?+ 自己 reproduce 的核心机制�?
> - **六轮 AB 对照表（截止 11:59 UTC�?*�?
>   | run | toml | 时长 | C5 worksites | C6 me_cov peak | C3 coop | C4 omni fit | gen59 fit | run_dir | cap fired |
>   |---|---|---|---|---|---|---|---|---|---|
>   | curriculum-v1 | post-fix | 190 min | 88 | 0.480 | 2376s | - | - | ~85 GB | �?|
>   | structural-v1 | post-fix+mut | 179 min | 110 | 0.440 | - | - | - | ~85 GB | �?|
>   | structural-v2 | +cap | 75 min | **113** | 0.400 | 2766s | 4453 | - | **1.73 GB** | 2 |
>   | curriculum-v2 | +cap | 84 min | 109 | 0.400 | 2165s | 5072 | 1786 | **1.74 GB** | 2 |
>   | throw-v2 | +cap+penalty | **42 min** | **34** �?| **0.320** �?| **1680s** �?| **3443** �?| 2352 | 1.73 GB | 2 |
>   | **baseline-v2** | clean+cap | **44 min** | 40 | **0.440** | 1986s | 3265 | **3223** | 1.80 GB | 3 |
>
>   **关键观察**�?1) cap-on �?cap-off 跑得�?2.4 倍；(2) 指标全程持平/略高（除 throw_penalty 实质降级）；(3) **6 �?runs �?7/7 PASS** �?5 �?config�?4) throw_penalty 降级模式�?fix 前完全不�?(worksites 减半 vs 全崩)�?5) **baseline (clean toml) gen 59 protagonist avg=3223，是六轮最�?* �?�?curriculum 压力�?protagonist 自由发展 fitness 反而更高；(6) graduate pipeline �?me_cov / fitness �?worksite 完成度（trade-off 健康）�?

> **2026-05-28 凌晨 Stone Bootstrap commit 4 fix 完整验证（graduate AB 60-gen 第二轮：harmony_check 5/7�?/7�?*�?
> - **commit `fb2c9c5`**（`master`）：`routes/protagonist_ecology/world/NurseryProgressionLayer.cpp` +54 行。`seedFromProgressionState` 检测每�?unlock flag �?false→true �?episode 转换，立即调对应 layer 重放物理世界 side-effect（stone �?10×ScatterStone via director；heavy �?heavy_unlock_count_×ScatterHeavyStone；first_fire/second_fire �?`campfire_->igniteNextDormant()`；worksite �?`worksite_->activateNextDormant()` + 1×ScatterStone support replay）。null-check �?collaborator 指针让原 unit test 继续过。`test_core.exe` **218/218 PASS**，`NurseryProgressionPersistence` 3/3 PASS�?
> - **根因**（不是文档说的「stone 解锁路径不是 6 sticks 累积」那条假设，是真正的代码 bug）：原版 `seedFromProgressionState` �?OR-merge 5 �?flag，但没重放物理世界的 side-effect。每个新 episode �?`createBootstrapWorld` 起全新世界（�?Stick、零 Stone、所�?campfire/worksite �?dormant）。seed �?`stone_unlocked_=true` 灌进来后，`dispatchRandomPendingEvent` �?`if (!stone_unlocked_ && deposits >= 6)` gate 短路，ScatterStone 永不再发——agent 继承 "stone_unlocked=true" 但活在零石头世界，整 60 �?deposits_stone=0、worksite=0、me_coverage workcraft 轴塌缩到 0.16�?*C5 + C6 是同一�?bug 的两个症�?*。文档第 23 行猜的「stone unlock 真实路径不是 6 sticks 累积」是错的——路径是对的，但 seed 阶段没复刻它对世界的副作用�?
> - **三段验证**�?
>   - **(1) 全测�?* `test_core.exe --gtest_filter='*' --gtest_output=junit:test_results.xml`�?*218/218 PASS**，无回归�?
>   - **(2) 4-gen smoke**（`tmp/layer1/smoke_4gen_curriculum_fix.toml` = curriculum.toml + generations=4，binary `build_ninja_cuda/bin/neural-eco-protagonist.exe` 2026-05-28 00:35 编入 fb2c9c5）：�?60-gen builds �?0；smoke 4-gen �?gen 2/3 各刷�?1 �?build；世�?stones>0 ticks 0�?85；deposits_stone>0 �?0�?99；me_coverage gen3 终�?5/25 (0.20) **已超�?60-gen 终�?* 4/25 (0.16)�?
>   - **(3) 60-gen 全跑** (`tmp/layer1/long_run_60gen_2d_curriculum.toml`，WMI `Win32_Process.Create` 启动避免 SSH 父进程拖死，PID 29416 01:04:16�?4:14:44 UTC�?90 min)�?
>     - `data/runs/long_run_60gen_2d_curriculum/20260528-010417/` checkpoint_gen{9,19,29,39,49,59} �?dump 完整；evolution.csv 60 �?fitness.csv 60 �?borderland_metrics.csv 完整�?
>     - **harmony_check �?7 �?PASS**（baseline �?7/7，原 5/7 修复达成）：
>       | # | 检�?| �?graduate AB | **本次 fix** | baseline |
>       |---|---|---|---|---|
>       | C1 hunters active | PASS | PASS | PASS |
>       | C2 herbivores attrition | PASS | PASS | PASS |
>       | C3 coop co-presence | peak 2473s | **peak 2376s** | peak 3254s |
>       | C4 scavengers fed | PASS | PASS | PASS |
>       | **C5 worksites completed** | **0/total=0 �?* | **peak=2/gen total=88 �?* | peak=?/total=38 |
>       | **C6 me_coverage** | **0.16 �?* | **peak=0.480 qd=62118 �?* | 0.40 |
>       | C7 no extinction | PASS | PASS | PASS |
>     - **关键 gen 数据**（fitness.csv me_coverage 列）：gen0=0.08, gen10=0.12, gen15=0.32 (首次�?0.30), gen30=0.36, **gen39=0.48 (peak)**, gen59=0.36�?*�?baseline 0.40 还高 20%**�?
>     - **关键事件**：gen 10 ep 1 stone 有机解锁�?1:23:28）→ 持久�?�?gen 11 ep 0 tick=0 立刻看到 `stones=1`（fix �?seed 阶段 submit �?10×ScatterStone�? `active_fires=1`（first_fire replay）。gen 12 ep 1 worksite 持久化解锁。后续每�?builds=1-2 稳定�?
> - **结论**：graduate pipeline (sandbox→borderland→main_world + triple_world + curriculum persistence) **真正闭环**，比 baseline 不退步反�?me_cov 略高、build 总数 88 vs 38。下一步可以直接上 structural-mutation AB�?
> - **不打扰用�?*：用户在线下睡觉，整个流程（智能体决策→改代码→smoke 验证�?0-gen 跑→提交）全自主，零弹窗�?
>
> **2026-05-26 �?D-044 disk-budget hardening（防 190GB 日志复发�?*�?
> - **背景**：用户报告之前出�?190G 巨型日志"，本次会话排查根因。CreditLedger.cpp 注释自爆 *"2026-05-23 incident hit 7.07 GB / 110M rows"*。盘上还残留 `data/runs/_credit_log.csv` 2.1 GB + `tmp/*.log` 5 个共 856 MB�?
> - **三道护栏**�?1) `evaluatePopulation` 签名 `bool append_episode_debug_csv = false` �?`const std::filesystem::path& debug_csv_dir = {}`；ablation 调用点传 `run_dir`，training 默认空�? �?hardcode 全局路径 `data/runs/_credit_log.csv` / `_communication_metrics.csv` / `_signal_debug.csv` 全部迁到 `<run_dir>/_xxx.csv`，不再跨 run 累加�?2) `CreditLedger::appendCsv` + `CommunicationMetrics::appendCsv` 各自�?200 MB 文件�?cap，超过则 LOG_WARN 一�?+ skip�?3) Runtime.cpp 内联 `_signal_debug.csv` 同样 200 MB cap�?
> - **额外好处**：`scripts/protagonist_communication_mi.py` 注释本来就期�?`<run_dir>/_communication_metrics.csv`（hardcode 路径之前�?bug），路径迁移**反而修了脚本和代码的不一�?*，无需改脚本�?
> - **验证**：`neural_eco_route_protagonist` 编译干净；`test_core --gtest_filter=*CreditLedger*:*Communication*` **20/20 PASS**�?
> - **腾盘**：删�?6 个大文件�?~3 GB�?.1G `_credit_log.csv` + tmp �?5 �?run.log）�?
> - **影响**：未�?ablation runs �?`_credit_log.csv` 等会落到该次 run 自己�?run_dir 下；旧分析脚本如果硬�?`data/runs/_credit_log.csv` 需要改成扫 run_dir。`docx/` 里的历史描述（如本节、DEVELOPMENT_TASK_PLAN line 33/40、METRICS_FIELD_GUIDE line 206/218、MEMORY_EVIDENCE_AUDIT line 324）保留原文以反映写入时刻状态，不是 bug 而是历史档案�?

> **2026-05-25 �?60-gen graduate AB 完整结果（关键发现）**�?
> - **AB 启动 21:05 �?完成 23:47 (2h42min, 60 gen × 2 ep)**。binary �?commit K (20:42) 编的，commit L/M/N/O 没进 binary�?
> - **harmony_check 5/7 PASS** vs baseline `long_run_60gen_2d.toml` 7/7�?
>   - �?C1 hunters / C2 herbivores / C3 coop (peak 2473s, baseline 3254s -24%) / C4 scavengers fed / C7 no extinction
>   - �?**C5 worksites=0/total** (baseline 38) �?completed_worksites �?0
>   - �?**C6 me_coverage 0.16** (baseline 0.40, 阈�?0.30) �?protagonist diversity 大幅下降
> - **borderland_metrics_summary verdict: HEALTHY (partial)** �?9 archetype eval 全工�?(large_herbivore mean fit 549�?693 +755%, ambush 100�?470 +2370%, protagonist 4415�?548 +116%)�? cycle b2m promote 全跑 (border=288/main=258), �?extinction。但 csv 限制：背景物�?promoted_to_main 列全�?0 �?commit H 只记 protagonist 那列（这�?csv 限制不是真实 0）�?
> - **关键失败原因（待下个 session 调）**：deposits_stick gen 0=5 / gen 29=1 / gen 58=46，但 deposits_stone **整个 60 代全 0**。Stone bootstrap commit 1/2/3 �?episode/process 持久化机制工作正常，�?**stone 的实际解锁路径不�?"6 sticks 累积" 这条**（gen 58 已经 46 stick deposit），是别的（first_fire? worksite preactivate? 主世�?task 设计？）。需要查 NurseryProgressionLayer 的真�?stone unlock 条件�?*worksite=0 �?C5 �?FAIL**�?
> - **二次失败原因**：triple_world 每代 sandbox→border 抽走 4 �?protag 精英 + 16 bg 精英，sandbox NEAT crossover 留下来的基因质量降低 �?me_coverage 0.16（baseline 0.40 �?60%）。这�?triple_world 设计的代价，可能�?promote 频率减半（period_gens 10�?0）或 top_k_protag 4�?�?
> - **结论**：graduate AB 暂时 **�?baseline 退�?*�?/7 �?5/7）。triple_world + curriculum 全开会拖�?protagonist 主任务。下�?session 需要：(a) �?stone unlock 真实路径并修�?b) �?triple_world period/top_k 减少 sandbox 精英抽走�?c) 之后再启�?commit L 结构�?mutation �?structural-mutation AB�?
>
> **2026-05-25 下午 L6 R3 commit L + M + N + O（结构�?NEAT mutation + metrics + per-archetype eval flavor�? 60-gen graduate AB 启动**�?
> - **Commit L** (`138eacb`)：`BorderlandEvaluator::mutateStructuralPool(archetype, add_conn_rate, add_node_rate, seed)`。Per-genome Bernoulli 决定是否�?add-connection / add-node mutation，mirror sandbox `NeatEvolution::addConnectionMutation` + `addNodeMutation` (24-attempt retry on add-conn, edge-split on add-node)。Pool-local `InnovationTracker` 先扫一遍池子，�?next_node_id 推过 max NodeId 防冲突。ProtagonistGenome (dense) dynamic_cast skip。Runtime hook 紧跟 mutatePool 之后，seed offset +13 �?weight perturb rng 独立。Toml 字段 `borderland_mutate_add_conn_rate / add_node_rate` 默认 0=off�? 个新 gtest (AddsConnections / AddsNodes / ZeroRatesNoop) �?PASS�?
> - **Commit M** (`0659c8e`)：`borderland_metrics.csv` header �?`node_count_avg, connection_count_avg` 两列。Per-row 对池�?NeatGenome 累加 nodes/connections，protagonist 池写 0 (dense 没结构概�?。`borderland_metrics_summary.py` 表加两列 + 新增 **Structural mutation: ACTIVE/OFF** 诊断�?(阈�?> 0.5 avg new nodes �?> 1.0 avg new edges)。配�?2 �?toml-parse gtest (defaults / parses)。Total tests �?**218/218 PASS** (commit L �?3 + commit M �?2 vs 上次 213)�?
> - **Commit N** (`ed640a0`)：第一�?per-archetype eval flavor。`miniTickBackground` �?`prey_pool` 参数；当 archetype �?ApexPredator/PackHunter/AmbushPredator 时，�?LargeHerbivore borderland pool 拉一�?top-fitness �?herbivore genome 注入 mini-world �?hunting target。这�?predator 选择真信�?(commit F �?1-agent isolated mini-world predator 全打 0)。LargeHerbivore 不在 background_archetypes �?prey_pool=null，silently fall back 到原行为。Scavenger / Omnivore / Rabbit 留下�?commit�?/5 borderland gtest 仍过 (placeholder path 不变)�?
> - **60-gen graduate AB 后台跑中** (`tmp/layer1/run_curriculum_60gen.bat` started 21:05)：全 pipeline 启用 �?curriculum.archetype_progression_persistence + triple_world (period=10, max_pool=32, eval_period=1, min_continuity=5, sigma=0.01, perturb=0.8, reset=0.05, extinction_rescue=2)�?*没启�?commit L/N** (binary �?commit K 时刻编译�?20:42，commit L/M/N 改的代码不进当前 binary)。下�?long-run 才会用到这些�?
> - **下一�?structural-mutation AB 配置已就�?* (`8c71f51`)：`tmp/layer1/long_run_60gen_2d_structural.toml + run_structural_60gen.bat`。在 graduate AB 配置基础上加 `borderland_mutate_add_conn_rate=0.10 + add_node_rate=0.04`。等 graduate AB 跑完 + binary rebuild 后即刻可启动�?
>
> **2026-05-25 中午 端到�?smoke 验证 + 60-gen throw_penalty AB 收口**�?
> - **Smoke `smoke_tw_eval.toml`�? gen, sigma=0.01, perturb=0.8, reset=0.05�?*：完整跑�?commit A-K wiring�? �?archetype 全部 mini-tick + mutate；protagonist pool 3�?2；large_herbivore mean fit **489�?178 (6.5×)**；omnivore **421�?159 (7.5×)**�?0 protagonist b2m promoted；borderland_metrics.csv 36 行�?*borderland_metrics_summary.py verdict: HEALTHY**。整�?graduation pipeline 端到端真闭环：sandbox→border→mutate→eval→cull→continuity-gate→main_world.
> - **60-gen throw_penalty AB 完整结果**：throw_attempt = -0.05 验证�?*负优�?*。harmony_check 对比：baseline 7/7 PASS vs throw_penalty **6/7 PASS** (C6 me_coverage �?0.40�?.28 退�?FAIL)；C3 coop -24% / C4 omnivore peak -26% / C5 worksites total 38�?8 (-26%)；所�?quantitative 指标全部退步。throw_attempt toml slot 默认 0=off 不需撤回；只�?`long_run_60gen_2d_throw.toml` 那个实验 config 别再用，改回 `long_run_60gen_2d.toml` baseline。throw spam 问题 (10000/ep × 14% 命中) 需要别的机制压�?(inventory 损失或碰�?penalty)，不是直接每 throw 扣分�?
>
> **2026-05-25 上午 L6 R3 commit J（真正的 borderland evolution�?*（commit `6b1e24a`）：
> - 之前 commit E/F/G 只能�?选择"，同一 genome 永远评出同样 fitness。Commit J 加上"探索"�?
> - `TripleWorldRuntime::mutateBorderlandPool(archetype, mutator)` �?pool 内每�?genome �?caller-supplied mutator。stashed_fitness 保持，下�?evaluateBorderland �?max(aged, fresh)�?
> - `BorderlandEvaluator::mutatePool(triple_world, archetype, sigma, seed)` �?ProtagonistGenome �?dense 权重 (input/hidden/action/interface) + NeatGenome �?ConnectionGene::weight �?Gaussian perturb。dynamic_cast 分派 (IGenome 没有 uniform mutate API)�?
> - toml gate `[runtime.triple_world] borderland_mutate_sigma`（默�?0.0 = off）�?
> - Runtime hook 顺序：promote �?**mutate (commit J)** �?evaluate �?cull �?continuity-gate �?b2m promote �?metrics csv (commit H)�?
> - Smoke config `smoke_tw_eval.toml` �?`borderland_mutate_sigma = 0.01` 让端到端 wiring 验证就位�?
> - 4 个新 gtest (Mutate pool applies/no-op + 2 config parse)�?*213/213 total**�?
> - 整套 commit A-J 现在是真正的 "borderland 内部 evolution arena"：sandbox 提供 founder �?borderland mutate + evaluate + cull 反复迭代 �?50-代物种延�?gate 决定毕业 �?main world�?
>
> **2026-05-25 上午 L6 R3 commit H/I（数据可视化 hook�?*�?
> - **Commit H** (`67f2154`)：`borderland_metrics.csv` per-(generation, archetype) 行写入。Columns: generation / archetype / pool_size / mean_fitness / max_fitness / borderland_eval_count / promoted_to_main / culled。empty pool 跳过。File created unconditionally�?
> - **Commit I** (`14ba6e3`)：`scripts/borderland_metrics_summary.py` 配套审计脚本。读�?CSV，per-archetype markdown 表（pool first→last / mean fit drift / eval_count / promoted total / culled total�? 全局 verdict (HEALTHY / GATED / STAGNANT / INACTIVE)。一行命令分�?borderland 是否真在 evolve"�?
> - **Commit H/I 是分�?infrastructure**：让�?run AB 完成后能立刻判断 borderland evolution 健康度，不需要手�?60-gen log�?mirrors `harmony_check.py` 模式�?
>
> **2026-05-25 上午 L6 R3 stage 3 commit D/E/F/G 四连**（commits `8aa4b5f` + `2883226` + `5ca63ae` + `d5f8a5f`）：
> - **Commit D** (`8aa4b5f`)：BorderlandEvaluator 骨架 + placeholder 占位 (fitness=complexity\*100)。把 commit B �?loop inline lambda 抽到独立类�?
> - **Commit E** (`2883226`)：protagonist (archetype 0) 走真 mini World2D tick。每 genome �?`createBootstrapWorld(route)` �?`makeProtagonistAgent(id=1, g, ...)` �?tick 100 substep × 0.1 sim_sec = 10 sim_sec �?fitness = alive_substeps + agent.fitness()。Deterministic per-genome RNG.
> - **Commit F** (`5ca63ae`)：background archetype (1..8) �?same mini-tick。API �?optional `background_archetypes` vector 映射 archetype index �?SpeciesArchetype。`miniTickBackground` �?makeBackgroundAgent，try/catch wrap �?genome 类型错配 crash。`tickAndScore` 共享 helper�?
> - **Commit G** (`d5f8a5f`)：protagonist mini-world 注入 �?3 �?peer（同 pool 的其�?protagonist genome），�?cooperation / signal / co-presence reward 路径真触发。focal agent fitness 才被读，peers 只是生态压力。Background �?1-agent (cross-archetype mix �?commit H+)�?
> - **smoke configs 已就�?*：`tmp/layer1/smoke_tw_eval.toml` + `run_smoke_tw_eval.bat`�? gen smoke, period=1 + aging=1.0�? `tmp/layer1/long_run_60gen_2d_curriculum.toml` + `run_curriculum_60gen.bat`�?0 gen AB 启用 curriculum gate, 验证 stone bootstrap 端到端）。Binary 空闲后立刻能跑�?
> - **整套 L6 R3 stage 3 7 commits 总成**：toml gate `borderland_eval_period_gens > 0` 启用后，**�?archetype 都跑�?mini world** + protagonist mini-world 还带 3 �?peer。borderland fitness 真反�?borderland 选择压力�?
> - **测试总数**�?09/209（commit E/F/G 没加�?test，unit test 构�?ProtagonistGenome + full route 复杂度高于实施本身，�?smoke 验证）�?
> - **未做**�?a) smoke 验证（binary lock 中）�?b) per-archetype eval flavors (e.g. predator �?prey, builder �?worksite candidate)�?c) 完整 50-代物种延续闭环（gate 已在 commit C 落，但还没在�?run 中验�?promote→demote 节奏）。这些是后续 commit H+ 工作�?
>
> **2026-05-25 上午 Stone bootstrap commit 1/2/3 三件�?done**（commits `199b510` + `7cf32da` + `c087a41`，每件都�?unit test，smoke �?binary 空闲）：
> - **Commit 1** (`199b510`)：NurseryProgressionLayer �?episode API。新 `ArchetypeProgressionState` struct�? monotonic unlock bool�? `seedFromProgressionState(state)` setter + `progressionState()` getter。setter �?OR-merge 语义，stale-false 永不清掉�?true �?flag�? �?gtest（DefaultStateIsAllFalse / SeedRoundtrips / SeedIsMonotonicOrMerge）�?
> - **Commit 2** (`7cf32da`)：主 loop hook + toml gate。`ProtagonistEcologyConfig::CurriculumConfig::archetype_progression_persistence_enabled`（默�?false�? runTraining �?loop 持有 `std::array<ArchetypeProgressionState, 9>` slot 0=protagonist；createBootstrapWorld 后调 seedFromProgressionState，evaluatePopulation �?OR-merge �?array；spdlog::info 日志状态变化�? �?config gtest（CurriculumArchetypeProgressionPersistenceDefaultsOff / ParsesTrue）�?
> - **Commit 3** (`c087a41`)：跨进程持久化。`MainWorldSnapshotMetadata` �?`std::vector<ArchetypeProgressionState> archetype_progression`；save() �?`archetype_progression` JSON 数组（empty 时跳�?�?�?snapshot consumer 看不�?schema 变化）；load() parse 数组；JsonScanner �?readBool helper；ProtagonistEcologyRuntime �?save metadata �?dump 当前 array、load metadata �?OR-merge�? �?gtest（ArchetypeProgressionRoundtrips + EmptyProgressionLoadsEmpty）�?
> - **整套总成**：API �?runtime hook �?JSON schema �?跨进程持久化�?closed-loop。toml gate 默认 off �?现有 train 配置完全不受影响；启用后 protagonist "stone unlock" �?episode 跨进�?sticky�?*未做**：smoke test（需�?neural-eco-protagonist binary 空闲�? AB 长训证明 stone deposit/gen 数字真升上去�?
> - **测试总数**�?00 �?**207**�?7 = 3 + 2 + 2）�?
>
> **2026-05-25 上午 Layer 6 R3 stage 3 三件落地 + harmony 工具 + stone bootstrap 提案 + 60-gen throw AB 后台�?*（commits `c14598b` + `be9dd43` + `8a49dbb`）：
> - **背景**：用户路线图选择"1�?�?�?�? 顺序�?，且要求"训练时手头还有可以并行跑的任务就并行�?。本会话后台启动 throw_penalty 60-gen AB 长跑（`tmp/layer1/long_run_60gen_2d_throw.toml`�?D BD + `throw_attempt = -0.05`，run_dir `data/runs/long_run_60gen_2d_throw/20260524-185129`），同时前台推进 graduation pipeline 收尾 (#2)�?
> - **Trace 配置改动**：`tmp/layer1/long_run_60gen_2d_throw.toml` �?`[trace].max_generations` �?4 改成 60、`agent_interval`/`world_interval` �?10 改成 20、`viewer_frame_interval` �?40 改成 80（控�?disk overhead）�?0-gen full trace 启用第一次落地，�?gen ~50MB（event_trace 主头），60-gen �?17GB �?D �?free space�?
> - **Stone bootstrap 提案**（`docx/supporting/STONE_BOOTSTRAP_PERSISTENCE_PROPOSAL.md`，~200 行）：read-only 调研报告，给�?4 个方案对照。推荐方�?D archetype-level unlock state 持久化（�?MainWorldPersistence 哲学一致），分 3 �?commit 落地，~250 行总，4 test�?
> - **Harmony check 脚本**（`scripts/protagonist_harmony_check.py`，~200 行）：内部审计工具，�?`protagonist_behaviors.csv` + `species_metrics.csv` + `fitness.csv`，给�?7 �?harmony checklist �?PASS/FAIL 报告（hunters active / herbivores attrition / coop co-presence / scavengers fed / worksites completed / me_coverage �?0.30 / no extinction）。验证上次会�?baseline `long_run_60gen_2d/20260524-171849`�?*7/7 全过**（me_cov 0.40 / coop 3254s / 38 worksites total / �?archetype 存活），说明训练已经接近"和谐"标准线�?
> - **L6 R3 stage 3 commit A** (`c14598b`)：`TripleWorldRuntime::evaluateBorderland(archetype, eval_fn, aging_factor)` API。Spec § 1.2 "50 代考验 + 不灭绝才进入主世�?。stage 2 + R2-stage-3 留下 borderland �?fitness pool accumulator，每�?genome �?stashed fitness �?sandbox→borderland promote 时的快照，从未重新评估。新 API �?borderland pool，调 caller 提供�?eval_fn（让 TripleWorldRuntime 保持 World2D/brain 无关），�?`max(aged_old, fresh)` 更新 stashed fitness。aging_factor=1.0 默认�?aging�? 1.0 让停止表现的 genome 渐渐丢掉领先优势。max 语义防止一次差评把已证明的 survivor 立刻 demote�? �?unit test（identity eval + aging factor decay）�?
> - **L6 R3 stage 3 commit B** (`be9dd43`)：主 loop hook。toml gate `[runtime.triple_world] borderland_eval_period_gens`（默�?0=off�? `borderland_eval_aging_factor`（默�?1.0）。Runtime �?N 代调一�?evaluateBorderland，eval_fn 当前�?no-op identity（按 id 配对�?stashed fitness 返回）�?commit B 只负责把 plumbing 接通，commit C 才会替换成真正的 borderland tick。INFO 日志：`TripleWorld gen N: borderland eval (no-op identity) updated K genomes (aging=X.XXX)`�? �?config parse test（默�?disabled + toml roundtrip）�?
> - **L6 R3 stage 3 commit C** (`8a49dbb`)�?0-�?物种延续 gate。`WorldPopulationPool` �?`borderland_eval_count`（每�?evaluateBorderland 自增）。toml gate `borderland_min_continuity_gens`（默�?0=off）。`promoteBorderlandToMain` 之前�?archetype 检�?eval_count �?min_continuity；未达标 archetype 跳过（其�?archetype 仍正�?promote）。INFO 日志：`TripleWorld gen N: borderland continuity gate blocked K archetype(s) from main promote`�? �?gtest（eval_count tracking + early-return �?bump counter）�?
> - **L6 R3 stage 3 真正没做�?(deferred)**：commit B �?eval_fn 仍是 no-op identity，没�?真正�?borderland �?World2D + tick N ticks + �?fitness" �?那需�?reuse `createBootstrapWorld` 或新建一�?minimal borderland world factory，工程量 ~1000+ 行新代码 + brain wiring�?*这是下一�?session 的最高优先级**。当�?plumbing 已就位，只等真正�?evaluator 函数实现。Smoke (`smoke_tw_eval.toml`) �?deferred �?60-gen AB 训练在跑没法�?neural-eco-protagonist binary�?
> - **测试总数**�?94 �?**200**�?
>

> **2026-05-25 凌晨 训练收口审计�?D BD 验证为负优化 + throw_penalty 机制就位**（commits 用户 push �?+ `da3fc4f`）：
> - **背景**：用户质�?是不是过早进可视化阶段，训练真的闭环了吗"。本会话先把 long run #10 60-gen 跑完�?D BD `tmp/layer1/long_run_60gen.toml` seed=7777, 70 min），再跑 2D BD AB（`tmp/layer1/long_run_60gen_2d.toml` �?hunt 维度撤掉退�?5×5=25 cells）做对比�?
> - **3D BD 60-gen 结果（baseline�?*：avg fitness 1185�?774�?.3×），best peak 13236（gen 1 spike）后回落�?10684（gen 58），me_coverage 全程 1.5-4%�? cells / 192）。Build chain gen 35-40 才觉醒，crafts 0�?0-16，但 worksite 完成数仍 0-1/gen，stone deposit **0-2/gen**（核心瓶颈）�?
> - **2D BD 60-gen 结果（AB�?*：avg fitness 1185�?*5917**（gen 58 峰，5×），best peak **15248**（gen 58, 1.43× 3D），me_coverage 早期�?0.08-0.16，gen 14 跳到 0.32，gen 28 顶到 **0.40 (10 cells / 25)**，qd_score 峰�?65183。Build chain gen 8-10 就有 worksite 完成事件，gen 28-32 出现 142-169 deposits + **6-13 stone deposits/gen**（stone 真正打通！�? 18 crafts�?
> - **结论**：v17 hunt-axis 3D BD 192 cells 验证�?*负优�?* �?Vassiliades 2017 spec 早预测的 curse of dimensionality 真发生：archive 大半时间空着，selection 退化成 "�?random NEAT"，进化没�?diversity 信号。下次训练默认应�?2D BD（`long_run_60gen_2d.toml` 已就位），或者切 CVT 模式（早已实现，从未真正测过）�?
> - **Throw_penalty 机制**（commit `da3fc4f`, +73/-3 lines, 194/194 tests）：3D BD 评估发现 throws 8-22k/gen × 14% 命中 = network spam throwing 浪费 inventory（thrown stone 永远到不�?worksite）。新 toml gate `reward.goal_completion.throw_attempt`（默�?0=off）走现有 HRL 子奖励数�?slot 13；ThrowCarriedObjectCapability �?`noteGoalCompletion(13)`。负权重生效�?N × |weight| �?fitness 扣。`long_run_60gen_2d_throw.toml` �?`-0.05` �?10000 throws/ep × 0.05 = -500/ep �?1 worksite_completion_reward 等量�?足够压但不毁�?。AB 长跑下次会话做�?
> - **Stone deposit 调查（read-only�?*：`ProtagonistWorldFactory.cpp` line 121 �?**training role �?bootstrap zero stones**，全�?sticks。`NurseryProgressionLayer` 进度 gate�? deposits unlock stone�?0 �?ScatterStone），但每 episode 重置，protagonist 必须�?episode 重学整条链。是为什�?stones �?3D BD 下永远不到位的根因之一�?
> - **测试总数**�?92 �?194�?
>
> **2026-05-25 凌晨 Layer 6 R2 stage 3 三件全做�?*（commits `45f56a6` + `2e24abf` + `39cd7da`）：
> - **Commit 1 borderland cull cap** (`45f56a6`)：stage 2 留下 borderland 永远只增不减�?0 代后能涨�?240+ genome/archetype。新 `TripleWorldRuntime::cullBorderland(archetype, max_pool_size)` �?stashed fitness �?top-N 砍其余，每次 sandbox→borderland promote 之后调一次。toml gate `[runtime.triple_world] borderland_max_pool_per_archetype` 默认 0=关。Smoke (`tmp/layer1/smoke_tw_cull.toml`, cap=4) 验证：每 archetype 稳定 4，border �?36 = 9 archetype × 4，culled 数学完全对应�?*未做 (R3 deferred)**：真正的 "borderland 进化" 即独�?World2D + 50 代物种延�?test。这次只�?pruning�?
> - **Commit 2 MainWorldPersistence pool-only snapshot** (`2e24abf`)：spec invariant #1 "主世界永�?reset" 之前只是进程内保证。新 `MainWorldPersistence::save/load` �?borderland + main_world pool 写到 JSON manifest + per-genome 二进�?blob (复用现有 `NeatGenome::saveToFile` / `ProtagonistGenome::saveToFile`)。toml gate `persist_dir` 默认�?关；启用时启�?load + 结束 save 自动。Smoke (`tmp/layer1/smoke_tw_persist.toml`, 2 次顺序运�?：run1 end border=36 main=56，run2 start "loaded snapshot border=36 main=56"，run2 end border=36 main=112 �?**第一次用具体数字证明 invariant #1 跨进�?*�?*未做 (R3 deferred)**：完�?World2D snapshot �?agent/worksite/tree/water/weather/season 字段；当�?borderland/main_world 还是 pool accumulator，只能诚实持久化 pool�?
> - **Commit 3 extinction rescue** (`39cd7da`)：spec invariant #4 "灭绝 �?fallback drift"。新 `rescueExtinctSpecies(archetype, k)`：main_world[archetype] 空时�?borderland �?archetype �?top-K 克隆�?main_world (borderland 不消�?；非空时 no-op。主循环每代 promote 之前 sweep 所�?archetype。toml gate `extinction_rescue_top_k` 默认 0=关。Smoke (`tmp/layer1/smoke_tw_rescue.toml`, period=999 �?border->main 不触�? rescue=2)：gen 0 rescue 18 = 9 archetype × 2，gen 1-3 main 已非�?rescue �?no-op，main 稳定 18�?
> - **Layer 6 R2 完整收口**：borderland 有界 + main_world 跨进�?+ 灭绝自愈三件齐全。R3 �?deferred 的：(a) borderland 独立 World2D + 50 代评估循环；(b) full World2D snapshot (agent/worksite/tree/water �?�?c) 完整 incident �?challenge �?训练 �?边境 �?迁徙闭环�?
> - **测试总数**�?92/192�?85 + 7 = stage 3 三个 commit 各加 2/3/2 �?7）�?
>
> **2026-05-24 深夜 Layer 6 R2 stage 2: 主循环接�?*（commit `3254e9e`）：
> - 不再只是 isolated skeleton，TripleWorldRuntime 现在**在真实训练循环里**每代触发 promote�?
> - `WorldPopulationPool` �?`fitnesses` 字段；borderland 现在记得每个 promoted genome 当时�?fitness，所�?borderland→main 可以无需重新评估直接 re-rank�?
> - �?`TripleWorldRuntime::promoteExternalToBorderland(archetype, source, fits, lineages, k)` —�?ProtagonistEcologyRuntime 保持拥有 sandbox pool (`population` + `species_contexts[i].population`)，三�?runtime �?mirror，只�?top-K 克隆�?borderland slot�?
> - �?stored-fitness 重载 `promoteBorderlandToMain(archetype, k)` �?slot �?stashed fitness�?
> - `[runtime.triple_world]` toml gate 默认 off：`enabled`/`top_k_protagonist_per_gen=4`/`top_k_background_per_gen=2`/`borderland_to_main_period_gens=50`/`borderland_to_main_top_k_protagonist=8`/`borderland_to_main_top_k_background=4`�?
> - 主循�?hook：每�?species_evaluated 收完后，build `{genome_id �?fitness}` map per archetype，把 top-K 克隆�?borderland；每 `period` 代再 promote borderland→main。INFO 日志：`TripleWorld gen N: sandbox->border protag=P bg=B | border->main protag=p bg=b | border=BT main=MT`�?
> - 运行结束�?`triple_world_summary.json` �?run_dir：每 archetype 槽位的最�?genome 数量�?
> - Smoke (`tmp/layer1/smoke_tw.toml`, 4 gen, period=2, seed=9999) 完整跑通：gen 0/1/2/3 全部 promote 触发；end-of-run summary borderland=76 main_world=58�? �?archetype 槽位 `[12,8,8,8,8,8,8,8,8]` / `[10,6,6,6,6,6,6,6,6]`，与 `top_K × gen / period` 数学完全对应�?
> - **未做（stage 3 deferred�?*�?a) borderland / main_world 独立 tick + 评估循环（现在只�?accumulator，没�?NEAT crossover �?borderland 内发生）�?b) MainWorldSnapshot 磁盘序列化；(c) extinction rescue（rescueExtinctSpecies）�?
> - **测试总数**�?85/185（之�?184 + 新增 1 = 185）�?
>
> **�?evening �?*�?
>
> **2026-05-24 �?Layer 6 R2 + Layer 7 R3**�?
> - **Layer 7 R3 step 1 完整**（commit `<L7R3-trace>`）：viewer_frames.jsonl �?LAYER7_GODOT_VIEWER_SPEC § 4.1 补齐 `trees[]`(id/x/y/iv/av) + 天气进度 `wp` + 季节进度 `sp`。view_frames.py 现在�?trees（绿色圆，半径随 integrity 缩放，砍倒后变灰木桩�? HUD 显示 `weather=N(WP%) season=N(SP%)`。Smoke 验证：第一�?80 棵树 + wp/sp 数据正常�?
> - **Layer 6 R2 skeleton**（commit `2570aa9`）：�?LAYER6_THREE_WORLDS_SPEC § 6 优先�?1 �?in-memory 多世界迁徙骨架�?
>   - `WorldRole` enum + `worldRoleName()` + `WorldPopulationPool` + `WorldSlot` 值类型（之前�?R1 只有 toml 字符�?accessor）�?
>   - `MigrationScheduler`（无状态，参数化）：`extractTopK` �?fitness 稳定排序克隆 top-K 保留 lineage_id；`seedFromCandidates` 追加不清空；`promoteTopK` 链式封装�?
>   - `TripleWorldRuntime` 持有三个 `WorldSlot` + `promoteSandboxToBorderland` / `promoteBorderlandToMain`；提�?`seedSandboxPool` 测试辅助�?
>   - **真实集成�?ProtagonistEcologyRuntime �?tick 循环 deferred**（spec 明确�?分阶段做"），以及主世�?disk 序列�?+ extinction rescue �?deferred �?这次 commit 只锁迁徙算术不破坏现有流程�?
>   - 7 个新 gtest 覆盖：enum 命名 / topK 排序 / k>N 截断 / size mismatch throw / append 语义 / sandbox→borderland 往�?/ borderland→main 往返�?
> - **测试总数**�?84/184 pass�?77 + 7 = 184），无任何回归�?
>
> **2026-05-24 傍晚 训练就绪审计剩余 3 项收口（#6 / #9 / #10�?*�?
> - **#6 [MEDIUM] 主角自�?BuildingType**（待提交）：
>   - �?`Agent::buildingTypeChoice()` uint8_t 字段（core/route 解耦），新 `routes/protagonist_ecology/capability/ChooseBuildingTypeCapability` 4-output argmax�?
>   - `WorksiteLayer::activateNextDormant()` �?vote 覆盖：在 `interaction_radius * 3` 内的 protagonist（archetype_tag==0）多数票决定�?site �?building_type；无 protagonist 时退�?seed �?weight 分布（向后兼容）�?
>   - toml gate `population.protagonist.building_type_choice_enabled = false` 默认 off，启用时 `action_dim` 必须 +4�?*所有现�?toml/checkpoint 不需改动**�?
>   - 验证：`Layer5ChooseBuildingType.*` / `Layer5WorksiteVote.*` 3 个新 gtest 全过；联�?smoke `tmp/layer1/smoke_69.toml`（action_dim=17 + DNC memory 同时启用�? gen × 1 ep 完整跑通无 crash�?
> - **#9 [MINOR] 背景�?DNC memory option**（待提交，方案改进）�?
>   - �?audit 建议 "�?NeatBrain 改成可插�?IBrain" �?实际**用项目已有的 perception/capability 扩展点更�?*：新 `BackgroundMemoryLayer`（IWorldLayer，按 AgentId 持有 DNCMemory�? `BackgroundMemoryReadPerception`（read_vectors 作为输入�? `BackgroundMemoryWriteCapability`（decode interface vec �?DNC step）�?
>   - **NeatBrain 完全不动**，所�?~7 �?`dynamic_cast<NeatBrain*>` 调用点（Hebbian / writeWeightsBackTo / ComprehensiveMetrics / HeadlessRuntime monitoring）全部保持工作�?
>   - toml gate `[population.background.dnc_memory] enabled / slots / word_size / read_heads`，默�?off + 默认 tiny 配置（slots=4 word=2 heads=1 �?read_vec_dim=2 / interface_dim=16，约 ~32 floats / agent�?5 个背�?agent �?8KB 总内存，远低�?audit 估计�?3MB）�?
>   - 验证：`Layer5BackgroundMemory.*` 3 个新 gtest 全过（getOrCreate 单例语义、read/write 往返、cull dead agents）；联合 smoke 同上�?
> - **#10 [MINOR] 50-100 gen 长训**�?*性能证据已收**，binary 不归档）�?
>   - �?reproduction + storage_deposit_reward + must_build_to_survive + Lamarckian Hebbian writeback **全开**�?60-gen 配置（`tmp/layer1/long_run_60gen.toml`，seed=7777）跑�?**13 �?/ 60 �?* 后被 link 步骤打断（kill 进程腾出 exe）�?
>   - 13 代结果：13 �?`PopulationLayer→NEAT bridge: injected N in-world born children into next-gen pools` 全部触发�?*累计 352 �?in-world 出生注入了进化池**�?6 + 28 + 38 + 85 + 62 + 44 + 29 + 16 + 17 + 2 + 2 + 4 + 9）。Gen 0 高峰�?founder Adult 集中产，后期回落到稳�?2-9/代，符合 reproduction_cooldown_sec=90 的预期节奏�?
>   - **意义**：第一次实�?Layer 4 reproduction + #1 NEAT bridge + #2 Lamarckian writeback 在长跑中能持续工作不崩溃（之前所�?reproduction 报告都仅 single-episode smoke 验证）。完�?60-gen 由用户后续按需启动�?
>
> **新增**：`tests/route_protagonist/test_layer5_choice_and_bg_memory.cpp` 6 gtest 全过；`test_neat_brain.cpp::InputMismatchSilentDegrade` 替换�?`InputMismatchThrows`（适配 bd07564 silent degrade 行为）�?*全测 177/177 pass**�?
>
> **2026-05-24 下午 训练就绪审计 9/12 项收口（�?5ccac49 Session summary 后）**�?
> - **#1 [SEVERE] in-world reproduction �?�?episode NEAT pool**（commit bd07564, b0cbfe4）：
>   `PopulationLayer::drainBornChildren()` + `runtime` bridge �?in-world 出生孩子注入 `species_contexts[i].population`，所以孩子不再只�?画面上看�?，而是真的进化池一员。Smoke 验证：单�?8-10 个孩子注入�?
> - **#2 [SEVERE] Lamarckian 真正继承**（bd07564）：�?`NeatBrain::writeWeightsBackTo(NeatGenome&)` �?Hebbian 在线学习的权重写�?child genome（toml gate `evolution.lamarckian.enabled`）�?
> - **#3 [SEVERE] kill() �?killWithCause() 全量替换**（ebf4299）：8 �?`agent->kill("...")` 全部改成正确�?`DeathCause::{Starvation/Predation/Combat/Disease/Thermal/Dehydration}`。`Agent::death_counted_` one-shot flag 防止 PopulationLayer �?tick 重复计数�?
> - **#4 [SEVERE] PopulationLayer O(N²) �?O(N)**（ebf4299）：spatial bucket on (archetype_tag, cell_x, cell_y), cell_size = reproduction_radius�?×3 邻域扫描。Smoke 验证 same-seed 输出与原 O(N²) 完全一致�?
> - **BUG ROOT CAUSE [pre-existing, fixed bd07564]** �?上一次会话报�?4 in-world births verified"实际�?mid-episode 后立�?STATUS_STACK_BUFFER_OVERRUN 崩溃（每�?reproduction 启用就会复现）。Root cause：`PopulationLayer` �?`Agent::speciesId()` 配对，但本路径下背景 agent 全是 `kInvalidSpeciesId`，所�?Scavenger�? outputs）会�?Omnivore�? outputs）配对，child brain �?parent_a �?outputDim �?capability �?parent_a �?archetype，结�?`Agent::applyCapabilities` �?"output slice overflow"，从 parallelFor worker 抛出 �?std::terminate �?/GS canary。Fix: 改用 `archetypeTag()` 作为同种判定 key。同时把 `Agent::decide` / `decideUntilDense` / `applyCapabilities` �?`NeatBrain::forward` 的所�?throw 改成 spdlog::error + 静默降级，并�?`AgentLayer::parallelFor` worker �?try/catch �?�?诊断比崩溃更有用"�?
> - **#5 [MEDIUM] Layer 4 vs Layer 5 A/B 验证**（b0cbfe4）：4-gen × 2-ep × seed=4242 同种子对比。Layer 5 reward shift Gen 0/2/3 都比 Layer 4 �?+44/+60/+51 % avg fitness，最高个�?+194%，QD score 提升 +257% �?Gen 3。Layer 5 不是噪声。详�?`docx/supporting/LAYER5_AB_RESULT.md`�?
> - **#7 [MEDIUM] Lookout sense_radius_scale 接入 4 �?perception**（a8dc76e）：`TreePerception` / `WaterPerception` / `DangerPerception` / `ObjectLogisticsPerception` 全部�?`Agent::senseRadiusScale()`。之前只�?`AgentSocialPerception` 用，所�?Lookout 建筑只能"看更远的 agent"，看不到�?�?�?物资 �?现在全部联动�?
> - **#8 [MEDIUM] Storage deposit reward + building_type toml shaping**（a8dc76e）：�?`reward.storage_deposit_reward` toml binding，每�?deposit 落到完工 Storage 时按 `cooperation_weight × storage_deposit_reward` 加分。Smoke 启用后从 8 �?children 升到 10 个，me_cov �?4/192 升到 5/192�?
> - **#11 [MINOR] task_template must_build_to_survive penalty**（待提交）：�?toml 字段 `[[evolution.task_templates.must_build_to_survive]]` + `must_build_penalty`，episode 结束时给�?episode 完全没造过 worksite �?protagonist �?200 fitness。把"没造就活不下去"硬编码到训练信号�?
> - **#12 [MINOR] Reproduction unit tests**（b0cbfe4）：`tests/route_protagonist/test_layer4_reproduction.cpp`�? �?gtest 覆盖 NeatBrain::writeWeightsBackTo、PopulationLayer �?archetype_tag 配对、BornChild 队列、Agent::deathCounted、senseRadiusScale clamp�?
> - **2026-05-24 下午被暂缓的 3 项已在傍晚收�?*（详见上�?"傍晚 3 项收�? 段）�?
>   #6 / #9 / #10 全部完成，audit 12/12 项关闭�?
>
> **早一些的工作�?ccac49 之前�?*�?
> - Layer 1 (a0669f6, 7a098a6): 体温 + 600×600，alive 0.45�?.79
> - Layer 2 (202186b): WeatherLayer 6 type，avg_fit +167%
> - Layer 3 (e3d2b8b): Rabbit + 9 物种，avg_fit 6685 best 12390
> - Layer 4 (da86050, 1377352): LifecycleStage/DeathCause/lineage_id/PopulationLayer�? in-world births 验证 + perf throttle（注：reproduction crash 这次会话才修�?
> - Layer 5 (3501070, da86050, e598450, 5824eb7, b8326ee): BuildingType 4 type + Shelter 雨天免疫 + reward shift + building_type distribution + Storage overflow + Lookout perception bonus
> - **Layer 5 30-gen 长跑结果**: Gen 27 peak protagonist avg=4543 best=18451 (vs v17 baseline 737 = **6.2x improvement**)
> - Layer 6 R1 (b53a4e8): WorldRole borderland/main_world accessors (TripleWorldRuntime 实现 deferred)
> - Layer 7 R1 (456e09b): viewer_frames.jsonl frame dumper (agents/worksites/weather/season JSONL)
> - Layer 7 R2 (617d5df): minimal pygame Python viewer (160 LoC，PEP 723 self-contained)
> - Layer 8 R1 (4d57714): Lamarckian Hebbian online weight adaptation (toml gate)
> - 4 �?docx/supporting/LAYER{5,6,7,8}_*.md 详细设计文档
>
> **2026-05-24 蓝图驱动 Layer 1-5 完整 + 6/7/8 R1 baseline + 设计 spec**�?
> - Layer 1 (a0669f6, 7a098a6): 体温 + 600×600，alive 0.45�?.79
> - Layer 2 (202186b): WeatherLayer 6 type，avg_fit +167%
> - Layer 3 (e3d2b8b): Rabbit + 9 物种，avg_fit 6685 best 12390
> - Layer 4 (da86050, 1377352): LifecycleStage/DeathCause/lineage_id/PopulationLayer�? in-world births 验证 + perf throttle
> - Layer 5 (3501070, da86050, e598450, 5824eb7, b8326ee): BuildingType 4 type + Shelter 雨天免疫 + reward shift + building_type distribution + Storage overflow + Lookout perception bonus
> - **Layer 5 30-gen 长跑结果**: Gen 27 peak protagonist avg=4543 best=18451 (vs v17 baseline 737 = **6.2x improvement**)
> - Layer 6 R1 (b53a4e8): WorldRole borderland/main_world accessors (TripleWorldRuntime 实现 deferred)
> - Layer 7 R1 (456e09b): viewer_frames.jsonl frame dumper (agents/worksites/weather/season JSONL)
> - Layer 7 R2 (617d5df): minimal pygame Python viewer (160 LoC，PEP 723 self-contained)
> - Layer 8 R1 (4d57714): Lamarckian Hebbian online weight adaptation (toml gate)
> - 4 �?docx/supporting/LAYER{5,6,7,8}_*.md 详细设计文档
> - viewer pipeline 端到端可用：C++ runtime �?viewer_frames.jsonl �?Python viewer 渲染。Godot 4 + Kenney 完整 vision deferred 到下次会话�?
>
> **2026-05-23 下午 A2.4 fairness 暴露�?memory_ablation_results.csv 收口**（Stage A 100% 完成，pending commit）：
> - **背景**：A2 Credit/EventLineage 主线唯一剩的 [~] �?fairness 指标暴露�?CSV。`CreditLedger::computeFairness` 已实现并 4 gtest 覆盖（freeloader_ratio / share_gini / top_share_mean / total_reward），但只写到 `data/runs/_credit_log.csv` 的�?payout 行，没有 per-(task, variant) 聚合�?`memory_ablation_results.csv`�?
> - **改�?*�? �?surgical 编辑——`evaluatePopulation` 加可�?`CreditFairnessMetrics* out_fairness = nullptr` 参数（默�?nullptr，training call site 4009 零影响）；函数末尾在 `appendCsv` �?`if (out_fairness) *out = computeFairness(0xFFu)`；`MemoryAblationAggregate` �?4 sums + 3 计数；`MemoryAblationResultRow` �?4 fairness + 2 count avg；memory ablation call site �?`&episode_fairness`；episode loop �?只在 payout_count>0 时累�?fairness 三指�?（避免空 episode 0s 注入�?完美平均"）；CSV header + write �?6 列�?
> - **验证**�?66/166 tests 全过；用 v16 best genome resume eval `tmp/a24_fairness_test/a24_test.toml`�? ablation variants × 7 tasks × 2 episodes = 70 episodes）；35 �?csv �?6 列全部正常生成；26 �?non-zero fairness 数据真实分化：`comm_build_request/baseline` gini=0.267 top_share=0.804 显示少数人扛 80% 工作；`signal_response/baseline` gini=0.475 freeloader=0.024 显示 2.4% free rider；`comm_danger_warning/baseline` 最活跃 payouts=37 sources=13；`water_return` total_reward=0 合理（该任务�?build/hunt/signal reward 输出）�?
> - **意义**：Stage A 主线 100% 完成（A1 �?[x]、A2 �?[x]、A3 �?[x]）。下一步默认进 Stage B（物种角�?R-005），�?v16 留的 hunting 退化（archive �?build 挤掉 hunt genes）会污染"群猎�?角色验证，建议先修后再进 B 阶段�?
>
> **2026-05-23 下午 L2.9 v16 公平 scale 验证**（验证数�?+ toml + py �?commit 98f508f）：
> - **背景**：v15 报告"24× v8 alive / 97× v8 crafts"，但 v15 �?protag=8 / ep=6（每�?48 evals），v8 �?protag=24 / ep=3（每�?72 evals）；per-(protag×ep) 归一化只能消除统�?bias，不能消除生态动力学差异（资源人均充足度）。用户�?#3 公平 scale 路线验证 v15 是否被分母扭曲�?
> - **改�?*：复�?`tmp/v15_qd_real/v15_qd_real.toml` �?`tmp/v16_fair/v16_fair.toml`，仅�?3 处：`scenario_name`、`runs_dir`、`population.protagonist.count: 8 �?24`、`episodes_per_generation: 6 �?3`。其余配置（archive、me_neat 4 字段、reward、task_template、background）完全不动。无代码改动，复�?v15 同一 binary�?
> - **v16 60-gen 验证 (seed=91, total_prot=720 �?v8 完全匹配)**�?
>   * **真实进步**：avg_fitness 452�?052 (**4.54×** v15)、qd_score 5922�?9743 (**6.71×**)、me_coverage 10%�?0% (**1.88×**)、coop_per_prot 24�?00 (**4.23×**) �?这些�?24 protag 池子带来的真实改善（更稳定的 fitness 估计、更易触发合作、archive 填充更快）�?
>   * **v15 的虚高被揭示**：alive 50.2%�?9.2% (×0.78)、works 0.05�?.032 (×0.64)、crafts 0.41�?.32 (×0.78)、chops 2.34�?.22 (×0.95)、hunts 0.36�?.24 (×0.66) �?这些数字 v15 �?8 protag 时被资源人均充足放大�?20-35%，公�?scale 下退化�?
>   * **真实 v16 vs v8**：alive **18.7×** v8、chops **24×**、works **4.6×**、crafts **75×** �?仍然是巨大跃进，�?v15 报告�?24×/25×/7×/97× 略小但仍�?huge�?
>   * **未解决问�?*：throws 894�?082 (×1.21) 仍在恶化（BD 不含 throw 维度）；hunts 0.36�?.24 退化更严重（archive �?build/craft 挤掉 hunting genes）�?
> - **诚实结论**：v15 真正�?涌现"突破是真的，�?8-protag 配置让数字看起来比公平对比下乐观 20-35%。v16 之后所�?ME-NEAT 实验默认应使�?protag=24 配置避免分母 confound。BD 不含 throw/hunt 是已知架构限制，不是这次实验的目标�?
>
> **2026-05-23 中午 L2.9 v15 real ME-NEAT 突破**�? commit + 验证数据�?push）：
> - `<v15-code-commit> L2.9 v15: real ME-NEAT - archive-driven breeding + iso+line + init phase`
> - **背景**：v14 implicit fitness shaping + v14b 细化 bin 都没用，me_coverage 死在 4%。第二轮 Tavily 文献定位真问题：MAP-Elites 必须�?init phase (Nilsson 2021) + 必须 archive-uniform sampling (Mouret 2015) + Vassiliades 2018 iso+line operator �?uniform crossover �?3-4x coverage�?
> - **改�?*：`ProtagonistEvolution` �?public `reproduceFromArchive` + `crossoverLine` + `mutateBoosted` ~140 行；Config �?4 个新字段 (`me_neat_archive_breeding`/`init_generations`/`line_sigma`/`mutation_boost`)；Runtime �?reproduce 调用�?dynamic_cast 分流 archive_breeding=true 路径。`v15_qd_real.toml`: chops_bin=5, workcraft_bin=1, max_bins=8, archive_breeding=true, init_generations=10, line_sigma=0.15, mutation_boost=2.0�?
> - **v15 60-gen 验证 (seed=91)**�?*worksites 0.1�?.4 (24x v13, 4.8x v8 baseline)**�?*crafts 0.3�?9.5 (65x)**�?*survived 7.6�?4.1 (3.2x v13, 16x v8)**、chops 68�?12 (+65%)、me_cov 4%�?0%。代价：avg_fit 3191�?52, coop_s 4261�?131 - fitness 公式不奖�?worksite/craft 中段步骤导致数字看不出来，但 build chain 第一次真正涌现�?
> - **地基重构方向被验�?*：v8-v13 single-fitness NEAT 死局�?v15 真正�?MAP-Elites 流程突破，文献预测的 QD 优势�?60 代内可见。下次需要：�?fitness 公式�?worksite/craft 中段也奖�?+ BD 扩到 3D + 120-gen 长训�?
>
> **2026-05-23 凌晨 L2.8 v14 ME-NEAT 地基重构第一�?*�? commits �?push, +766 �?+ 验证数据）：
> - `<v14-code-commit> L2.8 v14: ME-NEAT (MAP-Elites + NEAT) architectural pivot`
> - `<v14-data-commit> L2.8 v14: 60-gen ME-NEAT validation - min_fit +1478, me_cov=4%调参不到位`
> - 用户怀�?地基从一开始就出错�?，让 AI 查文献。Tavily MCP 文献调研直接打脸 v9-v13�?*Craftax ICML 2024** "UED fail to make material progress" 直接打死 v12 ACCEL�?*Mouret 2019 GECCO** "NEAT 'success stories' need a lot of fitness shaping (not highlighted in videos/papers)" 完美描述 v9-v13 反复调参�?*InstaDeep ICLR 2023 spotlight** 证明 QD-NeuroEvolution �?RL+intrinsic �?skill discovery。诊�?3 个地基错�?1) single fitness vs Quality Diversity�?2) task-curriculum vs skill-discovery�?3) �?NEAT vs evolution+gradient hybrid。用户拒�?PPO/Dreamer (4-6 周工程量)、选择 ME-NEAT 演化路线�?
> - **改�?*：新�?`routes/protagonist_ecology/runtime/QualityDiversityArchive.{h,cpp}` (5x5 cell grid, BD = chops × build+craft, ~120 �?；ProtagonistEcologyRuntime �?`BehaviorDescriptor` per-genome 累积 + `accumulateGenomeBehavior` + 评估完投�?archive + `evaluated_for_breeding` 把非 cell-elite fitness × 0.05 (implicit MAP-Elites)；fitness.csv �?3 �?`me_coverage,me_qd_score,me_max_fitness`；`tmp/v14_qd/v14_qd.toml` 关掉 ACCEL/SMT 保留 v13 HRL 子奖励，me_neat_enabled=true�?*166/166 tests 全过**。me_neat_enabled 默认 false 所�?v8-v13 配置零影响�?
> - **v14 60-gen 验证 (seed=91, �?v10/v11/v12/v13)**�?*min_fitness -385 �?+1093 (+1478, CV 大幅下降)**、crafts 0.3�?.0 (+233%)、coop_s 4261�?2203 (+186%)、hunts 36.6�?2.9 (+17%)。avg_fitness 3191�?227 (+1% 持平)�?*�?me_coverage 始终 1-2/25 (4%)** �?bin_size=20 对早期数据太粗，archive 几乎没填，机制退化成"�?1 elite + 普�?NEAT"。chops 反而从 68�?8 (-73%)、throws 4647�?0962 (+136%)。地基方向被验证可行（min_fit 突破 + crafts 翻�?+ coop 翻倍），但首次 bin 调参不到位�?
> - 下次必须细化 bin_size (chops_bin=5, workcraft_bin=1) �?BD �?hunt 维度�?

> **2026-05-23 凌晨 L2.7 v13 HRL sub-rewards + 细节修复**�? commits, +322/-9 代码 + 验证数据）：
> - `084a2a8 L2.7 v13: HRL sub-rewards + weapon bonus + carry slowdown + stick throw`
> - `1b0a985 L2.7 v13: 60-gen HRL+details validation results`
> - 用户洞察�?投出去的效果都是一样的，那做出毛的意义又是什么呢" 触发 8 个细节调�?+ 同时建议 HRL。文献定位：Sutton & Barto Options framework + Vezhnevets FuN，配合现�?13-type GoalManager。修复：(1) 9 �?capability 在成功路径调 `agent.noteGoalCompletion()` �?reward 公式�?sum(weight × this_tick_count) �?outcome_w 解耦；(2) `GoalContextPerception` (13-onehot) �?worker 看到 manager�?3) 武器加成 (spear×1.6/stone×1.3/stick×1.1) �?melee + carry slowdown + stick throw�?
> - **v13 60-gen 验证 (seed=91)**：fitness avg avg **-924 �?+3066** (从负转正)、CV **88% �?30%** (v9 水平)、chops **18�?7** (+264%)、throws **0.4�?825** (×4500 emergent 投掷)、alive 3.8�?.1。peak_best 889�?941 (×10)。仍未追 v9: worksite 0.3 / craft 0.2 / drink 0 �?需 build chain phase-by-phase shaping (下次)�?

> **2026-05-23 凌晨 L2.6 v12 4-机制 literature-backed stack**�? commits, +314/-10 代码 + 验证数据）：
> - `422847f L2.6 v12: 4-mechanism literature-backed protagonist autocurriculum stack`
> - `d41a0ff L2.6 v12: 60-gen 4-mechanism stack validation results`
> - 用户反馈�?不能为了某个指标好就舍弃信号 / 让我去查文献"。文献定�?4 个真名症�?(Lazy Agent / Negative Transfer / Multi-task Averaging / Sparse Long-horizon Reward) + 4 个对应正解：(1) Context-conditioned policy (PEARL/RL²) �?8-onehot template idx 加到 brain input�?2) Decoupled intrinsic motivation (Cuccu/Schmidhuber 2011) �?�?`intrinsic × novelty` �?`outcome_w` 缩放里独立出来；(3) ACCEL regret-based weighting (Parker-Holder 2022) �?每代 EMA 更新 `(best - avg) / (\|avg\|+1)` 自动 upweight �?regret template�?4) SMT Hard-Tasks-First (Cho 2024) �?早期 N 代用 difficulty 加权（线�?taper）�?66/166 tests 全过�?*v12 60-gen 验证 (seed=91, �?v10/v11)**：alive 1.9�?.8 (+100%)、worksite 0.1�?.5 (+400%)、chops 7�?8.6 (+166%)、craft 0.1�?.7 (+600%)、hunts 19�?1 (+61%)；CV v10 327% �?v11 103% �?v12 88%；起�?fitness v11 -107 �?**v12 +216**。仅 co_presence -49% (early-gen mixed_ecology 比例过高，参数调而非机制问题)�?
> - 没追�?v9 长链�?build/craft 数字 �?60 代不�?+ 需�?hierarchical RL extension（不在本 commit 范围）�?*4 机制是后续涌现任务的稳定底座**�?

> **2026-05-22 �?v2 物种差异化全部落�?*�? commits, +2740 行）�?
> - `200ea1a L2.1: species differentiation v2` (damage threshold + stealth + scavenge eff + 8 species capability differentiation)
> - `3896e90 L2.2: homeostatic drive state v2` (5 drives + drive-delta reward formula + per-species setpoints + 3 toml gates)
> - `06daaf0 L2.3: resource niche partitioning v2` (4-kind plant + corpse + per-species eat efficiency)
> - `7d978b6 L2.4: per-episode task template autocurriculum` (POET-lite 6 templates from spec § 4.2)
> - **30 �?long retrain 验证通过**（`tmp/l2_5_long/v10_long.toml`�? templates × 6 episodes/gen × 30 gen = 180 episodes，~85s）：所�?8 物种 fitness 健康收敛�?*none collapsed**；scavenger peak avg=1068 (scavenge_eff working)、ambush_predator avg 300-660 (stealth + niche)、protagonist drive penalty �?-1551 �?-127 (best @ gen 22)�?66/166 unit tests PASS�?
> - 所�?v2 改动**全部默认 off**：`reward.drive_weight=0` / `world.resources.plant_weights.grass=1.0` / `[[evolution.task_templates]]` 空数�?= �?v9 / a3 / memory ablation 训练分布完全不变�?
> - 用户原话核心理由�?**奖励太直�?�?反涌�?*" �?v1 30 reward labels 路径完全废弃，v2 �?机制压出�?路径，符�?spec § 0.1 "v1 v2 根本差异" 表�?
>
> **历史 commit �?*（保留参考）�?
> 关键近期 commit 链：
> - `81f763f feat(protagonist): listener-side signal response reward + metric decoupling`
> - `2c81ca4 feat(protagonist): controlled signal emitter for camp+signal co-training`
> - `89b56ab docs(protagonist): condense experience_log + sync PROGRESS / STATUS for ablation tightening`
> - `02d5a08 fix(protagonist): tighten memory ablation tasks + peer_signal_nearby fix`
> - `71ddc9b wip(protagonist): prior session work - memory ablation infra + brainv3 shell + goal manager`
> 交接目标：让新对话能�?*不依赖旧上下�?*的情况下，快速接手仓库真实状态�?
>
> **2026-05-22 最新审�?*：动 memory / perception / reward 前请先看 `supporting/MEMORY_EVIDENCE_AUDIT.md`（三任务对照�?+ 44 �?perception 真用状�?+ 四类 memory bank 最小有�?query + DEAD/WEAK 标记 + spatial→goal cascade 排雷）�?
>
> **2026-05-22 audit-driven fix 落地**：基�?audit § 4.1 + § 4.2 + § 4.4 + A1.4 的修复已 commit（peer_signal_heard episodic 写入、perception dim 18-21 specialise、dim 34-35 cascade 解耦、listener-side dense reward `signal_response_dense_reward`、TraceRecorder 4 �?signal 归因 + 4 perception unit tests）。共 5 轮重�?v3→v7 验证�?*cascade 解耦四轮独立证据稳定（water/worksite spatial/goal �?-55%~-100% 分离�?*；signal_response_events �?0，根因是 reward 全局比例失衡（v6 5 variants fitness bit-exact 相同 / v7 反向），非单点缺口。见 `supporting/MEMORY_EVIDENCE_AUDIT.md` § 7�?
>
> **2026-05-22 下午 ROOT CAUSE 修正 + tick=0 sentinel fix**：systematic-debugging 重新�?Phase 1 取证（instrument 9 �?debug counter + `data/runs/_signal_debug.csv`），发现 v3~v7 全部 signal_events=0 �?*两层 bug 叠加**，不�?reward economy 失衡�?1) ablation toml `resume_checkpoint_path=""` �?跑随机网络，events 必然 0�?2) `runtime/ProtagonistEcologyRuntime.cpp` 3 �?`emit.tick == 0` 哨兵�?controlled emitter 第一�?emit 永久跳过。修后用同一 v7 genome 跑：events_avg 0.25~1.75（≠ 0），**avg_fitness 11~21 �?72~79（�?�?*，`min_dist_to_emitter` 10m �?0.12~3.8m。详�?`supporting/MEMORY_EVIDENCE_AUDIT.md` § 8。Audit § 7 �?需要换 PPO"假设被推翻——NEAT 已经能学�?protagonist 紧贴 emitter，events 数字偏低�?dedup 副作用�?
>
> **2026-05-22 �?A3 通信语义代码就位**：S1 `CommunicationMetrics` 基础设施（`runtime/CommunicationMetrics.{h,cpp}` + 9 �?gtest + `data/runs/_communication_metrics.csv`�? 4 个信�?ablation 开关（`world.signal.ablation = disabled|listener_blind|one_channel_only|noise`，runtime �?emit / subscriber / listener / episodic 四处拦截�? `scripts/protagonist_communication_mi.py` 离线 MI 计算。S2-S4 三个�?`MemoryAblationTaskKind`：`CommScoutWorker / CommDangerWarning / CommBuildRequest`，各自有差异�?reward profile（scout-worker �?food drop �?win condition；danger warning �?`signal_response_dense_reward = -0.6` 反向�?�?；build request 强制 worksite 不可见、靠信号定位）。S5 结果文档骨架 `supporting/COMMUNICATION_SEMANTICS_EVIDENCE.md` 已建�?*128/128 tests PASS**�?
>
> **2026-05-22 �?A3 跑分完成 + v8 retrain**：发�?`CommScoutWorker / CommBuildRequest` �?`background.forager_count / predator_count` �?0，触�?`protagonistSensoryDim` 里的 `hasBackgroundAgents=false` 分支，sensory dim 缩水拒绝加载已训 genome�?*修：`MemoryAblationEval.cpp` 给两�?task �?`forager_count=1`**（与其它 4 task 一致），rebuild `build_vs3131/bin/Release/neural-eco-protagonist.exe`�?*v8 retrain** (`configs/protagonist_ecology_train_a3.toml`)：seed 91�?37、generations 60�?20、controlled emitter 周期 30s�?5s、food count 2�? × amount 12�?8、jitter 10�?6m、`signal_response_dense_reward` 1.0�?.5、`signal_co_attendance_reward` 1.5�?.0、`signal_emit_companion_radius` 30�?6m�?20 代约 12 min（CPU），peak best_fitness=229k（v7 同期 ~140k）�?*Phase 1 5×7 ablation sweep** (commit 0119e78)�? �?signal ablation (`none/disabled/listener_blind/one_channel_only/noise`) × 7 task × 5 memory variant = 175 �?results�?
>
> **2026-05-22 �?A3 Phase 2 task 重设�?+ 第二�?sweep**（本 commit）：Phase 1 暴露两个 task 设计缺陷——`comm_danger_warning` �?200s episode �?0 死亡（randomPosition �?3 predator 散得太开 + dense_reward=-0.6 反向 reward 难训�? `comm_build_request` worksite_completion=0（v8 不会 chop→carry→deposit）。Phase 2 修复�?1) `comm_danger_warning` 改成 safe-rallying 语义：world 120�?0、predators 3�?2、emitter 永远�?nest 边、`signal_response_dense_reward` -0.6�?0.6（朝信号�?= �?nest �?= 安全），baseline 现在产出 4.5 死亡/episode（survived 3.5/8�? fitness 41�?.87 �?8%�?2) `comm_build_request` 第一�?try (tree.count=0 + nursery_supply auto-stick) 失败：v8 不会 pickup loose stick；第二次 try (tree.count=2 + required_sticks 2�?, stones 1�?) 也失败：visibility=false �?v8 perception 看不�?worksite 走不�?deposit；最�?worksite_completion �?0，fitness �?7% 是当前能给的最强证据�?*最终判�?*：A3 主线"关闭信号 �?行为明显下降"判定**多重成立**——scout-worker primary 4.25�? �?00% + fitness �?3%、danger_warning fitness �?8%、build_request fitness �?7%、signal_response fitness �?8%；survived / worksite_completion 这两类需要物理实体的 primary metric �?v8 能力下不能用，要让它们起来需要在 protagonist 训练阶段就接触对应任务（B 阶段范围）。完整对�?+ 9 �?Phase 2 报告�?`supporting/COMMUNICATION_SEMANTICS_EVIDENCE.md`�?

> **本版本相对旧�?handoff 的关键修正（精简版）**�?
> - 继续保留已落地代码：CT 结构化事件、Memory Phase A、worksite/hunt credit、rule-based goal loop、world-model dataset builder、offline MAP-Elites、DNC micro-task、BrainV3 最小壳�?
> - **重新校准完成度口�?*：按 `WORLD_UNDERSTANDING...` �?8 �?Phase 算，目前只有 **Phase 1 Trace/Resume** 真正收口；Phase 2/3/5/6/7 都还�?partial；Phase 4/8 还没真正开�?
> - **重新校准 GPU 口径**：CUDA build 路径已可编译运行，但 camp-building 训练基准下暂未观察到优于 CPU 的总耗时收益
> - 不再把固定�?9/79”写成当前源码树的绝对测试总数；当前工作树已新�?route tests，历史绿测数字只代表旧快�?

---

## 2026-05-31 ���ٵ��飨�����ɣ�δ�Ĵ��룩

- **Ŀ��**������/������/������䣬ֻ���١�Ԥ����� smoke��
- **��ȫ�ܸ�ȫ��Ч**��batched_dense �� 40% ����ƫ���������dense_gpu on/off �ٶ���ͬ���ر� AVX2/FMA Ҳ��Ч���Ķ��ѻ��ˣ���
- **����ʵ��**��ѵ����������ǰ��׶�ֻ�� 1.09/32 �� ~ ���̣߳�24 ���� per-agent ǰ���У�`agent_decision_threads=15` δ�������С�ƿ����ȱ���в����������ʶ� GPU/CPU/AVX2 ȫ�����С�
- **�����ܸˣ���ʶ����Ԥ��δʵʩ��**�����л������� per-agent ǰ�򣨶�ȫ��->������->ͳһд�أ����������䡢��ѧ�ȼۣ����ڹ������� AgentLayer ���Ȳ㡢�� deep_protagonist ���ã���Ĵ���+�ر�+���� harmony�������¸���/��Ԥ��ʱ����

---



## 0. 一页结�?

### 0.1 5 个事实新对话必须先记�?

1. **不要把这份文档旧版里的“Stage 0~5 全做完”理解成长蓝图完成�?* 当前真实口径是：`Trace/Resume` 完整；`Memory V2 / credit / QD / world-model dataset / goal rule loop` 都只�?partial�?
2. **CUDA build 路径可用，但“GPU 已加速训练”这个结论目前不成立�?* toolkit �?`D:\NVIDIA\CUDA\v12.8\`，`build_cuda\bin\neural-eco-protagonist.exe` 也能运行；但最�?camp-building 基准显示当前总训练耗时未优�?CPU�?
3. **没有跑过任何正式长训�?* 当前所有验证仍�?smoke、短 benchmark、短配置为主；开正式训练依然是用户决策权�?
4. **一个核心机制环确实已经端到端可观测**�?
   - chop �?ChopCompleted �?EpisodicMemoryBank.chopped_tree �?MemoryCognitionPerception �?brain
   - build �?per_agent_contribution_seconds �?BuildCompleted �?ledger �?share �?reward �?CreditAssigned 事件 �?built_worksite episodic
   - kill �?cumulative damage ledger �?joint_hunt_reward �?damage share �?CreditAssigned �?killed_prey/attacked_enemy episodic
5. **接手时一定要先做**：`git status --short` �?读这文件 �?�?`build/` 路径确认 baseline；CUDA 路径只在需要时再验证�?*不要悄悄开正式训练**�?

### 0.2 哪些没做（按设计明确推迟�?

| �?| 状�?| 备注 |
|---|---|---|
| 正式长训 | �?用户保留决策�?| 5 ablation × �?00 �?|
| Manager/Worker 双脑�?| �?Phase B | 脑结构升�?|
| DNC-forcing micro-task 长训 / ablation 解释 | �?Phase B | 最�?build/test/smoke 已完成，但还没证�?DNC 真学�?|
| WorkingMemoryBank 多时间尺�?| �?Phase B | |
| ProtagonistGenomeV2 | �?Phase B | |
| Real world model predictor / 预训�?/ 加载�?brain | �?Phase B | dataset 已可生成 |
| MapElitesArchive �?NeatEvolution reproduction | �?Phase B | 当前只有离线 archive / report |
| POET / EnvironmentGenome | �?Phase B+ | |
| 可视化层 (M2) | �?ADR-0005 延后 | `src/viz/` 仍空骨架 |

---

## 1. 仓库当前结构（高层）

```
D:\claude-code\c++\
├── src/                     # 主仓核心代码（共享底座）
�?  └── core/evolution/novelty/
�?      ├── NoveltyArchive.{h,cpp}      # �?
�?      └── MapElitesArchive.{h,cpp}    # Stage 5 新增
├── routes/
�?  └── protagonist_ecology/  # 当前主力工作�?
�?      ├── brain/            # ProtagonistBrain + DNCMemory + 显式记忆 + DenseGpu
�?      ├── perception/       # MemoryCognitionPerception 44 维等
�?      ├── capability/       # ChopTreeCapability(per-agent session) �?
�?      ├── runtime/          # ProtagonistEcologyRuntime + Trace
�?      ├── world/            # 路线专有世界层（base/tree/worksite/...�?
�?      └── docx/             # 路线规划与本 STATUS
├── tests/
�?  ├── core/                 # 历史绿测�?+ novelty/map-elites �?
�?  └── route_protagonist/    # contribution ledger / memory trial / BrainV3 �?route 测试
├── scripts/                  # Python 离线分析工具
├── configs/                  # TOML 配置�?
├── build/                    # �?MSBuild VS 构建目录
└── build_cuda/               # �?Ninja + portable CUDA 构建目录（gitignore�?

D:\NVIDIA\CUDA\v12.8\         # 便携 CUDA toolkit（无 admin、无 registry�?
├── bin/                      # nvcc.exe, cudart64_12.dll, cublas dlls
├── include/                  # cuda_runtime.h, cuda.h, thrust, cub
├── lib/x64/                  # cuda.lib, cudart.lib
└── nvvm/                     # nvvm64_40_0.dll, libdevice.10.bc

D:\claude-code\downloads\
├── cuda_12.8.0_571.96_windows.exe    # 3.15 GB local installer
├── 7zr.exe                           # 600 KB 单文件解包工�?
├── ninja\ninja.exe                   # 570 KB 单文件构建工�?
├── extract_cuda_portable.ps1         # �?CUDA �?D:\NVIDIA\
├── activate_cuda_portable.bat        # 一�?reconfigure + build + test
├── install_cuda_12.8.bat             # （留作备用，需�?admin 路径�?
├── activate_protagonist_gpu.bat      # （留作备用，需�?admin 路径�?
└── README_CUDA_SETUP.md              # 早版用户手册
```

---

## 2. 推荐阅读顺序

### 2.1 必读

1. `AGENTS.md`
2. `PROGRESS.md`（顶层）
3. `routes/protagonist_ecology/docx/PROGRESS.md`
4. `routes/protagonist_ecology/docx/STATUS.md`（本文件�?
5. `docs/ROADMAP.md`
6. `docs/ARCHITECTURE.md`

### 2.2 �?protagonist 路线动手�?

1. `DEVELOPMENT_EXECUTION_GUIDE.md` + `DEVELOPMENT_TASK_PLAN.md`
2. `supporting/MEMORY_SYSTEM_MASTERPLAN.md` + `supporting/MEMORY_PHASE_A_EXECUTION_PLAN.md`
3. `supporting/MEMORY_ABLATION_SMOKE_RESULT.md`（诚实结论：smoke 反向 != 记忆有害�?
4. `supporting/CONTINUOUS_TIME_WORLD_DESIGN.md`
5. `supporting/COMPUTE_SCALING_GPU_CPU_PLAN.md`
6. `supporting/PRODUCTION_WORLD_LIFECYCLE_SPEC.md` �?**�?R-002 / R-006 / R-007 / 3D viewer 任何代码之前必读**；固定了主世界永�?reset、训练永远在别处做、新血脉只能通过出生或迁徙进入、灭绝场景的合法处理路径�?17 条不变量
7. `supporting/SELF_GROW_CAPABILITY_LIMITS_SPEC.md` �?**�?R-008 / Stage E / 任何"自长 / 涌现"机制之前必读**�? �?L0-L7 精确化定义，L7 凭空�?capability 物理不可达必须人写，任何"自长 X"表述必须指明 Layer 编号

补充说明�?

- `archive/HANDOFF_NEXT_AI_2026-05-21.md` 只保留为单次会话历史参考，不再作为默认主入�?
- 当前 route 默认导航已切�?`DEVELOPMENT_EXECUTION_GUIDE.md` / `DEVELOPMENT_TASK_PLAN.md` / `STATUS.md`

### 2.3 接手第一件事不是写代�?

```bash
# 1. 看工作树脏不�?
cd /d D:\claude-code\c++
git status --short

# 2. 看最�?17 个新 commit（自 c7f1665 之后都是本会话产物）
git log --oneline -20

# 3. 验证 CPU build 还能�?
build\bin\Release\test_core.exe --gtest_brief=1
build\bin\Release\neural-eco-protagonist.exe --config configs\protagonist_ecology_train_smoke.toml

# 4. 验证 GPU build（如�?D:\NVIDIA\CUDA\v12.8\bin\nvcc.exe 在）
D:\claude-code\downloads\activate_cuda_portable.bat
```

---

## 3. 本次会话所有产出（按时间线�?

> 这一节是"如果新对话只看一节就知道这一夜做了什�?�?

### 3.1 Stage 0 · CT-2B/CT-3 结构化动作事�?�?

**commit**: `8ea81b2`

之前 chop/build 只有"attempts/successes �?1"的离散计数。改�?started/completed 配对结构化事件：

- `core/events/EventType.h` 新增 6 �?protagonist route 事件：`ChopStarted` / `ChopCompleted` / `ChopAbandoned` / `BuildStarted` / `BuildCompleted` / `WorksiteMaterialDeposited`
- `core/events/EventPayloads.h` 含完�?payload 结构（chopper、tree_id、started_at_seconds、duration_seconds、wood_yield、contributors[]、contribution_seconds[]…）
- `TreeLayer::chopNearest` 改返�?`ChopHitResult{tree_id, tree_pos, remaining_integrity, felled, wood_yield}`
- `ChopTreeCapability` 改成有状�?per-agent session 状态机：start / continue-same-tree / target-switch (abandon-old + start-new) / abandon (output drop / out-of-range / now-carrying) / complete
- `WorksiteLayer` 三个新字段：`build_started_at_seconds` / `per_agent_contribution_seconds` (map) / `active_builders` (set)
- `progressConstruction` 在首次进入发 BuildStarted、每 dt 累积 per-agent 贡献时间、完成时按贡献时长降序发 BuildCompleted
- `absorbCarriedMaterials` 实际入库时发 WorksiteMaterialDeposited �?depositor + material_type
- `TraceRecorder` 接好全部 6 个事件的 payload 序列�?

**验证**：smoke trace 显示 ChopStarted=8 / ChopCompleted=6 / ChopAbandoned=2 完整闭合，payload 字段齐全�?

### 3.2 Stage 1 · Memory Phase A 收口 �?

**commits**: `d6bc642` + `72e2199` + `01eec4d`

- �?runtime 评估循环新增 3 �?tick 收集器（`tick_chop_completed` / `tick_build_completed` / `tick_deposits`），订阅 EventBus 在每�?substep 收集
- �?per-agent memory 写入循环里转�?5 �?episodic：`chopped_tree`（按 duration salience）、`built_worksite`（按 contributor rank salience）、`deposited_stick` / `deposited_stone`�?.30）、`signal_emitted`�?.20�?
- `scripts/protagonist_memory_ablation.py`：一次跑 baseline + 4 �?ablation（spatial/episodic/social/goal），自动生成对照�?
- �?`supporting/MEMORY_ABLATION_SMOKE_RESULT.md` 诚实记录�? 开关都�?fitness 显著移动（基础设施 PASS），�?ablation 反�?fitness 更高的反直觉现象需要长训才能反转，**不能当成"记忆有害"的科学结�?*

**Phase A 7 项验收全�?PASS**（详�?PROGRESS.md �?8 节表）�?

### 3.3 Stage 2 · P3 ContributionLedger �?

**commits**: `77dc5dc`（worksite�? `b9bae01`（hunt�?

替换 free-rider-prone �?在场就分"模式�?

- **Worksite credit**: �?BuildCompleted.contributors[] �?contribution_seconds[]（已�?Stage 0 落地）。Runtime 现在�?
  - 找自己在 contributors 中的 index
  - 若贡�?< 0.5s（free-rider threshold）跳�?
  - 否则�?`my_seconds / total_legitimate_seconds` 分得 worksite_completion_reward
  - 社交合作分也�?share 分配给同 worksite 其他贡献�?
- **Hunt credit**: 新增 `hunt_damage_ledger[victim_id][attacker_id]` 累计 episode 内伤害。新�?`tick_deaths` 收集器订�?AgentDied�?
  - 每次死亡�?damage share 分配 joint_hunt_reward
  - 阈�?1% 过滤偶然擦伤
  - 社交合作分也�?share 分配给同杀其他攻击�?
- `tests/route_protagonist/test_contribution_ledger.cpp`�? �?builder（长 / �?/ 路过者）的单测，验证排名正确 + 路过者过滤。`tests/CMakeLists.txt` �?`neural_eco::route_protagonist` 链接

**验证**�?3�?4�?5 单测全绿；smoke 全程跑通�?

### 3.4 Stage 3 · 富化 GoalManager（P7 partial）✅

**commit**: `983f391`

之前 GoalManager 只用 13 �?GoalType 中的 6 个。本 stage �?`GatherStick / GatherStone / RespondSignal / RestNearFire` 等分支接进了 runtime 规则环；代码里也加入�?`EmitSignal` 分支，但按当前条件顺序它会被更高优先级的 `Flee` 条件盖住，所以还不能算稳定可达的现行 goal�?

- `GatherStick`: 活跃 worksite �?sticks AND 不在搬运
- `GatherStone`: 活跃 worksite �?stones AND 不在搬运
- `RespondSignal`: companion radius 内监听到 peer 信号，且满足刷新条件
- `RestNearFire`: 火点附近且健康较低时休整
- `EmitSignal`: 代码分支已存在，但当�?ordering 下仍未真正打�?

当前更准确的优先级链应写成：Flee > FindWater > BuildWorksite > GatherStick > GatherStone > RespondSignal > RestNearFire > FindFood > Explore�?

**未做**：真正的 Manager/Worker 双脑头（�?Phase B 脑结构升级范畴）�?

### 3.5 Stage 4 · World Model trace dataset builder（P6 partial）✅

**commit**: `983f391`

新增 `scripts/trace_to_world_model_dataset.py`：遍历指�?run 下所�?`trace/generation_*/episode_*/agent_trace.csv`，按 agent_id 分组拼成 `(obs_t, action_t, obs_{t+1}, reward_delta, dt)`。obs_dim=31，action_dim=4，输出单一 CSV，无 torch/parquet 依赖�?

**验证**：smoke run 生成 8864 �?transition�?2 �?header 一致�?

**未做**：实际的 world model 预训�?/ 模型加载�?brain（属 Phase B）�?

### 3.6 Stage 5 · MapElitesArchive（P5 partial）✅

**commits**: `146c37f` + `9d77e14`

- 新增 `src/core/evolution/novelty/MapElitesArchive.{h,cpp}`：与 NoveltyArchive 共用 `BehaviorCharacterization`，网格化离散行为空间，`considerCandidate` 返回是否�?cell �?fitness 严格提升。`bestFitness() / avgFitness() / coverage()` 诊断接口
- 单测 `tests/core/test_map_elites_archive.cpp` 验证：新行为开�?cell / �?fitness 不替�?/ �?fitness 替换 / 聚合 fitness 计算正确
- `scripts/trace_to_map_elites.py`：从 trace 离线�?BehaviorCharacterization 喂给网格�?archive，输�?`<run_dir>/map_elites_report.csv` + `map_elites_summary.txt`

**验证**：smoke run 生成 33 �?occupied cells�?^8 = 65536 可能）�?

**未做**：把 MapElitesArchive 真正接到 NeatEvolution �?reproduction loop（属 Phase B 进化策略升级）�?

### 3.7 Bonus 1 · STATUS.md 重写 �?

**commit**: `8bfd3a2`

�?2026-05-20 版的 1191 行交接手册重写成 5-stage 完成版�?

### 3.8 Bonus 2 · CUDA setup readiness（早版） �?

**commit**: `acd405b`

下载 14MB network installer + 写好 install_cuda_12.8.bat（需 admin�? activate_protagonist_gpu.bat（需 admin VS 集成路径�? README_CUDA_SETUP.md�?

**这一版是 admin 路径**，后来被 portable 路径取代。这些文件保留作 reference�?

### 3.9 Bonus 3 · 背景物种听信�?�?

**commit**: `b9f4ab9`

- `computeBackgroundSensoryDim` �?`SignalPerception::featureDim(channels)`
- `attachBackgroundCommonSensors` 给所�?background species �?SignalPerception(radius=16)
- 现在 prey �?predator 都能"听见" protagonist 与同类的信号
- �?signal emit reward / social communication wiring 形成基础通信回路，但不应再把它写成“EmitSignal goal 已稳定闭环�?

**注意**：这�?fresh-population breaking change（旧 background checkpoint 输入维度对不上）。新训不影响�?

### 3.10 Bonus 4 · CreditAssigned 事件 �?

**commit**: `703558b`

- �?EventType `CreditAssigned` + payload `{recipient, source_kind, source_id, share, reward}`
- source_kind = 0 �?worksite_build, 1 �?hunt_kill
- runtime 在两�?ledger 支付点都�?CreditAssigned
- TraceRecorder 序列化让 trace 可审�?

**验证**：smoke 没真触发（没�?protagonist kill 命中 + �?worksite 完成），但代码路�?OK。长训自然会出现�?

### 3.11 Bonus 5 · Ablation harness 大扩�?�?

**commit**: `277bf5c`

`scripts/protagonist_memory_ablation.py` 现在每个 variant 还会�?
- `parse_event_trace_counts()` 统计 chop/build/deposit/CreditAssigned 计数 + total reward
- `parse_map_elites_coverage()` 自动调用 trace_to_map_elites.py 生成 coverage �?
- markdown 表加列：chop_compl / build_compl / deposits / credit_build / credit_hunt / credit_total_reward / map_cells

一次运行解�?机制有没有真发火 + 反生命财产分�?+ 行为空间覆盖"三问�?

### 3.12 Bonus 6 · NoveltyArchive 单测 �?

**commit**: `5d7fddf`

�?F-NOVELTY 时代以来 NoveltyArchive 一直没单测。Stage 5 旁加 4 �?case：空 archive 返回 max novelty / 已知点自距离 0 / 远点比近点更 novel / clear 重置�?

**75 �?79 测试**�?

### 3.13 Bonus 7 · drank_water episodic �?

**commit**: `7748a55`

- �?`previous_hydration` map 跟踪 tick-over-tick hydration ratio
- hydration 跳升 > 0.02 �?�?`drank_water` 事件，salience 0.55
- smoke 验证 1233 行成�?top-salience 事件类型

### 3.14 Bonus 8 · saw_predator episodic �?

**commit**: `90d5ced`

- �?`had_predator_in_view_last_tick` set 跟踪状�?
- predator_nearby 上升沿（不是�?tick）→ �?`saw_predator` 事件，salience 0.85
- smoke 验证 1508 行成�?top-salience 事件类型

### 3.15 Bonus 9 · Portable CUDA + Ninja 接�?✅✅�?

**commits**: `b30771c` (CMake fix) + `4dd1dbc` (gitignore)

**这是本会话最大的工程突破**。详见第 4 节�?

---

## 4. CUDA / GPU 路径详细记录

### 4.1 起点

- 本机：RTX 5060 Laptop（Blackwell sm_120�? NVIDIA driver 596.49（CUDA 13.2 capable�?
- `C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v12.4` �?`v11.8` 是空壳（只有 `extras/`，无 `bin/include/lib`�?
- `where.exe nvcc` 找不�?
- protagonist GPU 代码（`ProtagonistDenseGpu.cpp/.cu/.h`）和 CMake `NEURAL_ECO_ENABLE_PROTAGONIST_CUDA` 开关早就在仓库里，只是没编�?build
- 用户硬性要求：不装 C 盘、不�?admin

### 4.2 解决思路

NVIDIA 的标�?CUDA 安装强制要求 admin（registry + 系统 DLL 注册）。绕过：

1. **下载 local installer**�?.15 GB，比 network installer 完整，包含所�?.nvi 子包�?
2. **�?7zr.exe �?installer �?7z 解包**（不需�?admin，因�?7z 只是读字节流�?
3. **挑出 nvcc + cudart + nvrtc + cublas + cccl(thrust+cub) + nvtx + cuxxfilt + nvprof + profiler_api + cuda_cccl 这些组件**合并�?`D:\NVIDIA\CUDA\v12.8\`
4. **�?Ninja 生成�?+ `-DCMAKE_CUDA_COMPILER=...nvcc.exe`** �?CMake 直接驱动 nvcc，不�?Visual Studio �?MSBuild CUDA 集成（那�?.props 文件需要写�?`Program Files (x86)\Microsoft Visual Studio\…\BuildCustomizations\` 才能装，需�?admin�?
5. 跳过的组件：display_driver / Nsight VS plugin / GeForce Experience / CUDA Samples �?这些才需�?admin

### 4.3 文件清单

```
D:\NVIDIA\CUDA\v12.8\
├── bin\
�?  ├── nvcc.exe              17 MB
�?  ├── cudart64_12.dll       574 KB
�?  ├── cublas64_12.dll
�?  ├── cublasLt64_12.dll
�?  ├── fatbinary.exe
�?  ├── cudafe++.exe
�?  ├── cuinj64_128.dll
�?  └── ...
├── include\
�?  ├── cuda_runtime.h
�?  ├── cuda.h
�?  ├── cooperative_groups\
�?  ├── cub\
�?  └── ...
├── lib\
�?  ├── x64\cuda.lib
�?  ├── x64\cudart.lib
�?  ├── x64\cudart_static.lib
�?  └── cmake\                # find_package(CUDAToolkit) �?
├── nvvm\
�?  ├── bin\nvvm64_40_0.dll
�?  ├── bin\cicc.exe          # device code compiler
�?  └── libdevice\libdevice.10.bc
└── LICENSE

D:\claude-code\downloads\
├── 7zr.exe                                       600 KB（standalone�?
├── ninja\ninja.exe                               570 KB（standalone�?
├── cuda_12.8.0_571.96_windows.exe                3.15 GB（local installer�?
├── cuda_12.8.0_windows_network.exe               14 MB（早�?network installer 备用�?
├── extract_cuda_portable.ps1                     �?CUDA �?D:\NVIDIA\
├── activate_cuda_portable.bat                    一�?reconfigure + build + test（最终方案）
├── install_cuda_12.8.bat                         （早�?admin 方案，留�?reference�?
├── activate_protagonist_gpu.bat                  （早�?admin 方案，留�?reference�?
├── README_CUDA_SETUP.md                          早版 user 文档
├── test_vcvars.bat                               诊断脚本
├── check_url.ps1                                 URL 验证
└── cuda_build.log                                build 日志
```

### 4.4 CMake 改动（必须，别回滚）

`CMakeLists.txt` �?15-32 行：�?`add_compile_options(/W4 /utf-8 /Zc:__cplusplus)` 改成生成器表达式 `$<$<COMPILE_LANGUAGE:CXX>:...>`�?

**为什么必�?*：nvcc �?`/W4` �?Windows 路径解析，导�?`nvcc fatal: A single input file is required for a non-link phase`。这�?fix �?.cu 编译干净通过�?

### 4.5 当前更可信的 GPU 口径

| �?| 数�?|
|---|---|
| nvcc 版本 | NVIDIA 12.8.61 |
| build_cuda/ | 可生�?`build_cuda\bin\neural-eco-protagonist.exe` |
| 运行时依�?| 手动运行时需�?`D:\NVIDIA\CUDA\v12.8\bin` 放进 PATH，否则会�?`cudart64_12.dll` |
| camp-building benchmark（CPU build, 1 gen = 3 episodes�?| ~`5.6s/gen` |
| camp-building benchmark（CUDA build, 同配置） | ~`8.5 ~ 9.2s/gen` |
| protagonist_dense_gpu config 默认 | true |
| 当前结论 | **CUDA build 可运行，但当前总训练耗时未优�?CPU** |

这意味着现在最多只能说“GPU 路径接通了”，�?*不能**说“训练过程已经因�?GPU 明显受益”。根因很可能是：当前�?offload �?protagonist dense forward 的一小段，而且每步仍有 host↔device 拷贝，世界推�?/ 事件 / memory / DNC 主体仍在 CPU�?

### 4.6 重新�?GPU build 步骤

```cmd
:: 一次性（已经跑过，不需要重复）
powershell -ExecutionPolicy Bypass -File D:\claude-code\downloads\extract_cuda_portable.ps1

:: 每次想重 build�?
D:\claude-code\downloads\activate_cuda_portable.bat
:: 这个 .bat 会：
:: 1. call vcvars64（VS2022 BuildTools�?
:: 2. set CUDA_PATH = D:\NVIDIA\CUDA\v12.8
:: 3. PATH 头部�?CUDA_PATH\bin
:: 4. 删旧 build_cuda/，新建空目录
:: 5. cmake -G "Ninja" -DCMAKE_CUDA_COMPILER=...nvcc.exe -DCUDAToolkit_ROOT=...
:: 6. cmake --build build_cuda
:: 7. test_core --gtest_brief=1

:: �?GPU smoke�?
set CUDA_PATH=D:\NVIDIA\CUDA\v12.8
set PATH=%CUDA_PATH%\bin;%PATH%
cd /d D:\claude-code\c++
build_cuda\bin\neural-eco-protagonist.exe --config configs\protagonist_ecology_train_smoke.toml
```

### 4.7 如果碰到问题

- **`nvcc` 不在 PATH**：检�?`D:\NVIDIA\CUDA\v12.8\bin\nvcc.exe` 是否在；不在就重�?extract_cuda_portable.ps1
- **CMake 找不�?CUDA**：删 `build_cuda/CMakeCache.txt` 后重�?activate_cuda_portable.bat
- **启动直接报缺 `cudart64_12.dll`**：说明运行时 PATH 没带 `D:\NVIDIA\CUDA\v12.8\bin`
- **`nvcc fatal: A single input file...`**：检�?`CMakeLists.txt` �?22-28 行的生成器表达式没被回滚
- **跑出�?GPU util 一�?0**：检�?config �?`[compute] protagonist_dense_gpu_enabled = true` 没被关；ProtagonistBrain �?cudaGetDeviceCount 失败时会静默�?CPU，那�?nvidia-smi 没流�?
- **TLS 错误重试 git clone**：本机网络偶发，CMake �?FetchContent 自带 retry，不用管

---

## 5. 当前 commit 链（master 顶端�?

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

`c7f1665` 之前是上一会话 STATUS.md �?HEAD。从 `8ea81b2` �?17 �?commit 都是本会话的工作�?

---

## 6. 关键 Episodic 事件类型清单（脑能记什么）

protagonist EpisodicMemoryBank 现在记录这些事件，每条带 `(tick, time_seconds, position, type, salience)`�?

| 类型 | salience | 触发条件 |
|---|---|---|
| `ate_food` | 0.50 | foodEatenThisTick > 0 |
| `drank_water` | 0.55 | hydration_ratio 跳升 > 0.02（Bonus 7�?|
| `chopped_tree` | 0.40 + 0.1×duration | ChopCompleted 收到 |
| `built_worksite` | 1.0 - 0.10×rank | BuildCompleted 收到，自己在 contributors |
| `deposited_stick/stone` | 0.30 | WorksiteMaterialDeposited 收到 |
| `signal_emitted` | 0.20 | tick_emits 含自�?|
| `saw_predator` | 0.85 | predator_nearby 上升沿（Bonus 8�?|
| `was_attacked` | 0.20 - 1.0 by damage | tick_attacks 含自己作�?victim |
| `attacked_enemy` | 0.20 - 1.0 by damage | tick_attacks 含自己作�?attacker |
| `killed_prey` | 0.90 | kill_delta > 0 |
| `completed_worksite` | 1.0 | completed_this_tick > 0 AND �?worksite radius |

`MemoryCognitionPerception` �?`topSalient(1)` �?`recentOfType("was_attacked", 1)` 等查询喂给脑作为 44 �?perception 的一部分�?

---

## 7. 测试体系（历史绿�?+ 当前源码树新�?route tests�?

需要先把口径说清楚�?

- 旧版 `79/79` 指的是此前某个已构建快照，不再代表当前源码树的固定总数
- 当前工作树已经额外包含：
  - `tests/route_protagonist/test_memory_trial_layer.cpp`
  - `tests/route_protagonist/test_protagonist_ecology_config.cpp`
  - `tests/route_protagonist/test_protagonist_brain_v3_shell.cpp`
- `tests/CMakeLists.txt` �?glob 收集测试，所以下次重�?discover 时总数会变

历史上已新增并跑过的�?
- `tests/core/test_map_elites_archive.cpp`（Stage 5�? cases�?
- `tests/core/test_novelty_archive.cpp`（Bonus�? cases�?
- `tests/route_protagonist/test_contribution_ledger.cpp`（Stage 2�? case�?
- `tests/route_protagonist/test_memory_trial_layer.cpp`
- `tests/route_protagonist/test_protagonist_ecology_config.cpp`
- `tests/route_protagonist/test_protagonist_brain_v3_shell.cpp`

`tests/CMakeLists.txt` 通过 glob 收集测试，并统一链接 `neural_eco::route_protagonist`�?

---

## 8. 当前主要硬底线（接手人不能违反）

1. **不能擅自开正式训练**：所�?5 ablation × �?00 �?或类�?1+ 小时训练任务必须先问用户�?
2. **不要悄悄�?CMakeLists.txt �?22-28 行的生成器表达式**：nvcc 编不过会回到 Stage 6 之前�?
3. **不要�?smoke ablation 结果当成"记忆有害"的科学结�?*�? 代不够，详见 `supporting/MEMORY_ABLATION_SMOKE_RESULT.md`�?
4. **不要回滚 / 重命名已经落地的 Stage / bonus 代码**。虽然文档口径已从“全部完成”修正为“多�?partial”，但这些代码路径现在仍�?load-bearing�?
5. **不要�?GPU build (`build_cuda/`) 提交�?git**。已 .gitignore，但万一新人手动 git add -f 会出事�?
6. **不要�?D:\NVIDIA\CUDA\v12.8 误装�?C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v12.8**。我们故意走 D �?portable，C 盘那俩空壳是历史残留�?
7. **不要 admin �?install_cuda_12.8.bat**：会装出双份 CUDA，CMake 选谁会随机（可能选到 v12.4 空壳）�?

---

## 9. 接手时建议默认行动顺�?

### 9.1 用户�?继续 / 接着�?�?

```
1. git status --short  �?看是�?dirty
2. 先看 root/route PROGRESS 顶部，确认当前不是“整 phase 已完成”的错觉
3. 如需验证 baseline，优先看 `build/` 路径；CUDA 只在你确实要�?GPU 线时再碰
4. 默认推进方向�?
   A. 先把一�?partial phase 真收口（优先 `Memory V2 / ContributionLedger / communication semantics`�?
   B. �?`DNC-forcing micro-task` �?trace / ablation 解读
   C. 如果�?GPU 线，直接做更细粒�?dense-forward profiling，而不是再重复“CUDA build 成功�?
   D. 如果�?world-model 线，再开始训练一个真正的 predictor
```

### 9.2 用户�?GPU 还在工作�?

```
nvidia-smi  # �?driver 还在
D:\NVIDIA\CUDA\v12.8\bin\nvcc.exe --version  # �?toolkit 还在
:: 手动运行 build_cuda\bin\neural-eco-protagonist.exe 前，先确�?PATH �?CUDA bin
set CUDA_PATH=D:\NVIDIA\CUDA\v12.8
set PATH=%CUDA_PATH%\bin;%PATH%
:: 再决定是否需�?activate_cuda_portable.bat 重新 build
```

如果 D:\NVIDIA\ 整个被清掉了，重�?extract_cuda_portable.ps1（installer 还在 downloads/）�?
另外�?*“GPU 还在工作吗”要和“GPU 有没有让训练更快”分开回答**：前者看 build/path/runtime，后者要看具�?benchmark�?

---

## 10. 最终交接结�?

如果新对话只记住一句话�?

**当前 protagonist 路线最真实的状态是：Trace/Resume 已完整收口，Memory V2 / credit / QD / world-model dataset / goal rule loop 都已有代码落点但仍属 partial，通信语义任务与长训展示还未真正开始；CUDA build 路径可运行，但当�?camp-building 训练总耗时尚未显示出优�?CPU 的收益。下一步不该继续把 partial 写成完成，而应该先收口一个已经半落地�?phase�?*

## 2026-05-30 Phase B #2c ���գ�80 �� @ dmg=0.25/cold30, env-perc ON, seed 50001��
- harmony 7/7 ALL PASS��C6 ���� 0.440, C7 �������=> ��ѹ���� 80 ��δ�ƻ�Χ��/ʳ����/Э��/����/���ַֹ���
- ������ʵ���ۣ��ܶ� health < ������gen79 0.660 vs 0.862����
- �����Ա� gen0->gen79���ܶ� health 0.585->0.660����Դ�ӽ��� 0->1.576 => ������ + ���������𸽽���
- ��ʵ�������ܶ�ʱ��Դ�� +0.115��worksite ������Զ => �����ܺ��ź���ʵ��ƫ�������ֳ���������� docx/PHASE_B_2C_VERDICT.md��
- �û�Ҫ�������ԣ�����������һ�ּ�ͣ��δ�� ON/OFF �����ܣ����´Σ���


---

[Ѳ�� 2026-05-31 UTC 07:35 �� Round 7 EOD ��β]
- candidate 3��gen��160 rise/plateau ��ϣ������У�6 run��3 seed �� ON/OFF��th22/dmg0.3��adt=1��realism_detP2c_th22_*��ȫ� gen 24-25/160��harmony �� gen79/gen159 �������С�
- ������д outbox EOD handoff���������β�����ٿ��³��ܡ����½����ĵ���
- run ���� = Ĭ�Ϻ�̨�����ܣ�WMI �ѹ�������Ự�������������������ػ��� adt=1 �ȳ����ܸ��֡�����ʧ��
- �Ѹ������� note v6���������涨�� / �׸����� +11~12s ����Ԥ�� / ���ݶ��������� / gen��160 ��� / plateau �ſ���Ѫ�� / ȫ�׺��ߣ���round_7 д inbox �Թ鵵��
- δ�ջ����´ε�һ����= gen159 ȡ gen140-159 trace �� ���� R2 ���� gen79��~150 �� rise / plateau��
