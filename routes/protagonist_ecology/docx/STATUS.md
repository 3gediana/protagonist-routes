# STATUS.md · Neural Eco 全仓交接手册

> 最后巡检：2026-05-29 下午晚间 (UTC)（**Trajectory-analysis pivot：21/21 streak 后用户叫停跨 seed 验证，转向"agent 脑子里到底在干啥"的诚实评估**）：用户判定继续累积同质 stage 4b/4c run 边际收益已小，且预言"agent 轨迹画出来肯定是无头苍蝇"。本会话围绕这条做了三块：(1) **TraceRecorder.tail_generations** (`d081fd0`) — 加配置字段 `trace.tail_generations`，让 TraceRecorder 可专门捕获最后 N 代（替代头 N 代），配合磁盘纪律只留训练完成的 gen 79 真实脑况；新的 `shouldTraceEpisode` overload 接收 total_generations，旧 call site 行为不变。(2) **Python 轨迹分析工具链**：`scripts/protagonist_trajectory_analyze.py`（~660 行）读 agent_trace.csv，对每个 agent 算 velocity_autocorr / direction_persistence / dominant_goal_frac / goal_switches / visited_cells / path_length / radius_of_gyration / food_pull_fraction / predator_push_fraction 等 20+ 维度，聚合后输出 headless_score ∈ [0,1] + 三档 verdict (PURPOSEFUL / MIXED / HEADLESS_FLIES)，`--brained-only` 过滤掉 183 个非 NEAT 背景物种只看 24 个主角脑；`scripts/protagonist_trajectory_compare.py` 做两份 JSON 间的 diff（aggregate / goal distribution shift / 5%/25%/50%/75%/95% 百分位分布）；`scripts/inspect_trace.py` 快速探查原始 CSV 结构。(3) **gen 0 vs gen 3 Run 10 (s58193) 数据初步对照**：gen 0 mean velocity_autocorr=0.943 / direction_persistence=0.947 / dominant_goal_frac=0.581 / goal_switches=5.5 / headless_score=0.164；gen 3 同维 0.824 / 0.816 / 0.631 / 5.9 / headless_score=0.236。**两代 verdict 都是 PURPOSEFUL**——用户预测的"无头苍蝇"在 gen 3 阶段已初步证伪：autocorr 主动从 0.94 降到 0.82 = 脑子开始响应环境而非走死板直线，dominant_goal_frac 涨说明对单一目标承诺度增加，path_length 633→678 说明实际行动量增。**gen 79 数据缺失**：s71359 tail-mode run (新 seed, max_generations=0 + tail_generations=1) 跑到 gen 38 / ~50% 时被用户叫停（要玩游戏占 GPU），s71359 partial 数据已删。**下一会话首要任务**：补一个 80-gen tail-mode run 拿 gen 79 trace 跑 trajectory_analyze 看 gen 0/3/79 三点曲线，下个 verdict。**磁盘纪律**：删了 s58193 1.5 GB trace + 60gen_2d_curriculum 149 MB + s71359 partial，D 盘 free ~250 GB；`data/runs/` 只剩 6 个 stage4b run（每个 ~40 MB CSV+snapshot）。**Run 10 (s58193) 7/7 PASS 已 commit** `1f4ce28` — 21/21 streak（live-agent 时代第十次独立 MWH end-of-run 验证）。本会话 commit 链：`1f4ce28` (Run 10) + `d081fd0` (trajectory pivot) + 本 commit (docs 同步)。**未达成**：22/22 streak、gen 79 trajectory verdict、Layer 7 viewer（用户明确"先不要可视化"）。
> 巡检范围：`d:/claude-code/c++` 整个工作树 + `D:/NVIDIA/` + `D:/claude-code/downloads/`
> 当前分支：`master`
> 当前 HEAD（protagonist_ecology 视角）：本 commit 待写 (gen 79 trajectory verdict + docs/note sync)；当前 HEAD `c1f6e16` trajectory wrap-up；前 commit `d081fd0` trajectory-analysis pivot (TraceRecorder.tail_generations + Python analyzer 三件套) + `1f4ce28` Run 10 (s58193) 7/7 PASS + 21/21 streak。再前 `35e8535` stage 4c MVP / `c76a506` Run 5 stage 4c first validation / `ffd4187` Run 9 / `4c75aa3` Run 8。

> **2026-05-30 凌晨晚些 (UTC) 同 seed gen 0/3/79 复核 — gen 79 改判 PURPOSEFUL，food_eaten 疑点决起**：
> - **动机**：上一条 gen 79 = MIXED 是跨 seed 对比（gen0/3 用 s58193，gen79 用 s71359），只能作趋势参考。本 run 用同一 seed (81527) 一次抓 gen 0/3/79 三点（`trace.max_generations=4` + `tail_generations=1`）做干净同 run 对比，并直接查 food_eaten。
> - **执行**：`tmp/layer1/long_run_80gen_stage4b_s81527_traj3pt.toml` WMI 后台跑完（02:44→04:27 UTC，~103 min，~80s/gen）。`MainWorldHost end-of-run cumulative sim_time=800.000s tick_calls=800`；`TripleWorldRuntime end-of-run: borderland=288 main_world=338`。
> - **harmony 7/7 ALL PASS** → **23/23 streak**。C5 worksites total=152，C6 me_cov peak=0.440 qd=54101，C3 coop peak=3131.5s，C7 无灭绝。
> - **同 seed gen 0/3/79 (24 brained, ep0)**：headless_score 0.181 → 0.195 → **0.245**，**三代均 PURPOSEFUL**（都 < 0.35）。上条的 MIXED (0.354) 是跨 seed artifact，不是真退化。关键：dominant_goal_frac 0.62→0.65→0.69（**越来越专注于主目标**）；predator_push 0.42→0.41→0.66（天敌规避变强）；velocity_autocorr 0.90→0.79（从死直线变反应式，是好事）；goal_switches 只升到 8.1（跨 seed 那次是 16.3 = seed 特性）。
> - **food_eaten 疑点决起**：同 seed 下 food_eaten sum gen0=1.8 (3/24) / gen3=10.2 (9/24) / gen79=3.6 (6/24) —— **不是恒零**，逐代有波动。上次跨 seed gen79=0 是单 episode/seed 采样 artifact，非感知→进食 bug。protagonist 是协调专业型（emit_signal 主导 68%）非主觅采食者，感知到食物会走过去 (food_pull 0.45)。**未改 reward**（§0 铁律 2：查 bug 不调奖励，且这里没 bug）。
> - **goal 分布 (同 seed gen0→gen79)**：explore 41%→19%，emit_signal 33%→68%，respond_signal 23%→6%——同一 seed 内确认了从乱逛转向信号协调。
> - **磁盘纪律**：trace 已删，D 盘 free 232 GB。产物 tmp/traj_s81527_gen0/3/79.json + traj_s81527_compare_0_79.md + compare_0_3.md + traj_s81527_verdict.md。
> - **结论**：诚实判定 **PURPOSEFUL（不是无头苍蝇）**。跨整个 80 代 MWH run，brained protagonist 保持高目标承诺、响应食物/天敌、转向协调信号。

> **2026-05-30 凌晨 (UTC) Layer 6 R3 trajectory honesty eval — gen 79 verdict 拿到，"无头苍蝇"被证伪**：
> - **背景**：上一会话留的最高优先级洞 = gen 79 trajectory 没拿到（s71359 tail run 上次跑到 gen 38 被用户叫停占 GPU 玩游戏）。本会话接班把它补上。
> - **执行**：`tmp/layer1/long_run_80gen_stage4b_s71359_tail.toml`（evolution.generations=80 + trace.max_generations=0 + trace.tail_generations=1，只抓 gen 79）WMI 后台跑完（00:52→02:22 UTC，~90 min，~75s/gen）。`MainWorldHost end-of-run cumulative sim_time=800.000s tick_calls=800` 第 N 次实证主世界永不 reset (invariant #1)；stage 4c snapshot 写 219 活 agent (cumulative_seed_count=320)。
> - **harmony_check 7/7 ALL PASS** → 22/22 streak。C5 worksites total=151 peak=2/gen，C6 me_coverage peak=0.440 qd=83887，C3 coop peak=3337.0s，C7 无灭绝。
> - **trajectory_analyze gen 79 ep0 (24 brained agents, --brained-only)**：headless_score=**0.354** → verdict **MIXED**（阈值 PURPOSEFUL<0.35 / MIXED 0.35-0.55 / HEADLESS>0.55，刚过线）。聚合：mean_velocity_autocorr=0.660 / direction_persistence=0.665 / dominant_goal_frac=0.608 / goal_switches=16.3 / food_pull_fraction=**0.540** / predator_push_fraction=**0.688**。
> - **诚实结论**：**不是无头苍蝇**。反应性指标强（54% 感知到食物朝它走、69% 见捕食者逃、61% 单目标承诺）。headless_score 升到 MIXED 几乎全因 goal_switches 5→16，但这是在 explore 大降、信号协调主导背景下，更像快速切换有目的子目标而非混乱。
> - **跨 seed 趋势对照**（gen0/gen3 = s58193 seed58210，gen79 = s71359，**跨 seed 仅作趋势参考、非同 run**）：velocity_autocorr 0.943→0.660（不再走死直线，开始响应环境）；goal 分布 explore 43%→12%、emit_signal 29%→60%、respond_signal 24%→20%（从乱逛转向信号通信协调）；bbox_area 80582→19916（活动收缩 4x，围巢/工地聚集非散开乱飞）。
> - **诚实保留**：gen79 ep0 brained agent food_eaten_final mean=0.000（会朝食物走但这一局没真吃到）—— 标注存疑，后续跑盯。
> - **磁盘纪律**：分析完立即删 trace dir（456 MB），D 盘 free 232 GB。分析产物留 tmp/traj_gen79_ep0.json + tmp/traj_compare_0_79.md + tmp/traj_compare_3_79.md。
> - **commit**：`protagonist_ecology: gen 79 trajectory verdict (MIXED, reactive-purposeful) + harmony 22/22 + docs/note sync`。

> **2026-05-28 下午 Layer 6 R3 stage 4 MainWorldHost continuous tick — FINAL_BLUEPRINT #3 落地 + 七连 7/7 PASS 上 80-gen**：
> - **背景**：用户路线图 "1→2→3→4→5"，#1 (MigrationScheduler sandbox→borderland) + #2 (NEAT graduate AB) 已完整闭环并经六轮 7/7 PASS 验证。#3 main world continuous tick 是 FINAL_BLUEPRINT 最大遗留模块，核心是"主世界永不 reset (PRODUCTION_WORLD_LIFECYCLE_SPEC invariant #1)"。spec § 6 划分了三层优先级：priority 1 = sandbox→borderland migration (done)，priority 2 = main world 内存中不 reset (本次)，priority 3 = disk 序列化 (deferred)。
> - **MainWorldHost class (`67e83a4`, 230 行新代码)**：
>   - `routes/protagonist_ecology/runtime/MainWorldHost.{h,cpp}` 新建：constructor 取 `ProtagonistEcologyConfig const&`，内部用 `createBootstrapWorld(route)` 构造一个 `std::unique_ptr<World2D>`。
>   - 提供 `tick(SimTimeSeconds dt)`：dt > 0 时调 `world_->tick(float(dt))` + 自增 tick_call_count_，dt ≤ 0 noop 返 false。
>   - 提供 `simTimeSeconds()` 委托给底层 World2D 的单调时钟，`tickCallCount()` 自己计数，`world() const/non-const` 暴露 World2D 指针（被测试 / 未来 migration hook 用）。
>   - 不可拷贝 / 不可移动（host 是 unique owner）。
> - **5 个新 gtest (`tests/route_protagonist/test_main_world_host.cpp`)**：
>   - `ConstructionYieldsWorld`：构造后 `world() != nullptr`, `simTimeSeconds()==0`, `tickCallCount()==0`
>   - `TickAdvancesSimTime`：tick(0.1) + tick(0.25) 后 sim_time 严格单调上升
>   - `NonPositiveDtIsNoop`：tick(0) / tick(-1) 不改 sim_time 也不增计数
>   - `NeverRecreatesWorldAcrossManyTicks`：50 次 tick(0.05) 后 `world()` 指针完全等于初始指针（**这是 § 1.3 "永不 reset" invariant 的结构化测试**）
>   - `SimTimeMatchesWorldClock`：host.simTimeSeconds() == world()->currentTimeSeconds()，确认无双重簿记
> - **训练循环接入 (`e5e67dd`, 5 文件 / 125 行 / 1 减)**：
>   - `ProtagonistEcologyConfig.h::TripleWorldConfig` 加 3 字段：`main_world_continuous_tick_enabled{false}` / `main_world_tick_dt_seconds{1.0}` / `main_world_ticks_per_generation{10}`
>   - `ProtagonistEcologyConfig.cpp` 用 `config.getOr<>()` 解析三者
>   - `ProtagonistEcologyRuntime.cpp::runTraining()` 在 `triple_world` 构造后用 `std::unique_ptr<MainWorldHost> main_world_host` 持有（gate off 时不构造，零成本）。每个 generation 末尾（在 checkpoint save 之后、`continue_training` 检查之前）调 `main_world_host->tick(dt)` × `ticks_per_generation`，LOG_INFO 输出 `MainWorldHost: gen X advanced N ticks (dt=Ys, cumulative sim_time=Zs tick_calls=K)`。运行结束输出 `MainWorldHost end-of-run: ...`。
>   - 2 个新 config 测试覆盖 defaults-off + toml 解析两条路径
>   - 修复了一个 forward-decl `class ProtagonistEcologyConfig` → `struct ProtagonistEcologyConfig` 的 MSVC LNK2019 mangling mismatch
> - **rebuild + 全部测试 PASS**：`vcvars64.bat` (`C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat`) + `D:\claude-code\downloads\ninja\ninja.exe -C build_ninja_cuda -j 8 test_core neural-eco-protagonist` → 5+7 unit + 1 link = OK，`bin\test_core.exe` 跑出 `[==========] 225 tests from 57 test suites ran` + `[  PASSED  ] 225 tests`。
> - **首次 80-gen curriculum-v2 跑完整 (PID 52972, 12:02:19→13:53:25 UTC, ~111 min)**：harmony_check 输出 ALL PASS：
>   - C1 hunters: ambush_predator>=3, pack_hunter>=11, apex_predator>=2 PASS
>   - C2 herbivores: rabbit 22-26 / large_herbivore 20-79 / small_forager 13-14 PASS
>   - C3 coop peak=2059.0s PASS
>   - C4 scavenger pop 6-7 fit 1413 / omnivore pop 16-38 fit 5088 PASS
>   - **C5 worksites total=137 peak=2/gen** — 七轮最高 (curriculum-v1=88, structural-v1=110, structural-v2=113, baseline-v2=40)
>   - **C6 me_coverage peak=0.480 qd=55305** — 历史最高并列 curriculum-v1
>   - C7 all archetypes survived PASS
> - **80-gen 释义**：六轮 60-gen 7/7 PASS 已经横向验证 fix 跨 5 config 通用；本次 80-gen 纵向验证 fix + cap 在更长训练下持续生效，且 C5 / C6 还在继续涨而不是塌缩（说明 protagonist 在 60→80 代之间仍在学新行为，graduate pipeline 还有上行空间）。
> - **下一步**：(a) Layer 6 R3 stage 4 后续 commit：把 borderland→main_world genome promotion 链路扩展到"在 MainWorldHost 的 World2D 里也 spawn 一个 live agent"，目前只搬 genome 不搬 agent；(b) Layer 7 R3 stage 2 Godot viewer；(c) 启动 main_world_continuous_tick_enabled=true 的长 run 看 host log 行真实输出。

> **2026-05-28 早晨 disk-budget 二次硬化 + structural-mutation AB v2 (7/7 PASS) + 长 run 编译管线恢复**：
> - **背景**：凌晨连跑 graduate-AB-v2 (curriculum) + structural-mutation AB v2 各一轮，每轮 trace 输出 ~76 GB（60 gen × 2 ep × ~750 MB `event_trace.jsonl`），D: 盘从 244 GB 跌到 92 GB free（**消耗 152 GB，已逼近爆盘**）。前次 D-044 给 CSV（CreditLedger / CommunicationMetrics / `_signal_debug.csv`）加了 200 MB cap，但 trace 子系统漏配——`shouldTraceEpisode()` 只按 `max_generations / max_episodes_per_generation` gate，没有 byte-cap，配上 `max_generations = 60` 就爆。
> - **代码修复**（pending commit，已编译验证通过）：
>   - `routes/protagonist_ecology/runtime/TraceRecorder.{h,cpp}`：新增 `TraceRecorderConfig::max_bytes_per_file{200 MB}` 字段 + `TraceRecorder::{event,agent,world,viewer}_cap_warned_` 四个 once-flag。
>   - 新增 `checkAndCapStream(stream, warned, path, cap)` 工具方法：`stream.is_open() && tellp() >= cap` 时立即 `stream.close()` + 单次 `LOG_WARN` 一行（D-044 同款 once-warn）。
>   - 4 个 write 点全部前置守门：`writeEvent` (event_trace.jsonl) / `writeAgentSnapshot` (agent_trace.csv) / `writeWorldSnapshot` (world_trace.csv) / `writeViewerFrame` (viewer_frames.jsonl)。
>   - `loadTraceRecorderConfig()` 读 `trace.max_bytes_per_file = N`（默认 200 MB，写 0 即关 cap）。
> - **配置回滚**（pending commit）：4 个 `tmp/layer1/long_run_60gen_2d{,_curriculum,_structural,_throw}.toml` 把 `[trace].max_generations` 从 60 → 4（已验证：harmony_check 只读 CSV 不读 event_trace，4 代 trace 足够诊断回放），并叠 `max_bytes_per_file = 209715200` 兜底。其中 `_throw.toml` / 基础 `_2d.toml` 之前未入 git（`?? long_run_60gen_2d{,_throw}.toml`），改动留本地。
> - **手工清理**：`Remove-Item D:\claude-code\c++\data\runs\long_run_60gen_2d_{curriculum,structural}\20260528-*\trace\generation_{2..58}` 删除中间代，仅保留 gen 0/1/59；释放 ~144 GB，D: 重回 ~244 GB free。
> - **structural-mutation AB v2 结果**（curriculum + structural-mutation 双 60-gen 都收齐前）：harmony_check **7/7 PASS**，C5 worksites total **110** (curriculum 88、baseline 38)，C6 me_coverage peak **0.44** (curriculum 0.48、baseline 0.40)。结构性 NEAT mutation (`borderland_mutate_add_conn_rate=0.10 + add_node_rate=0.04`) 在 fix 后的 graduate pipeline 上**继续不退步且 build 数还高**。
> - **编译管线恢复**：磁盘清理副作用导致 `build_ninja_cuda/` 几乎被清空（只剩 `lib/`+`routes/`，binary + build.ninja 都没了）。用 Clash proxy (`127.0.0.1:7897`) 给 git 全局 http/https.proxy 重新拉 _deps，cmake reconfigure 76s + ninja incremental rebuild 74s → `test_core.exe` + `neural-eco-protagonist.exe` 新出炉（2026-05-28 07:44:28）。`test_core.exe` **218/218 PASS**（含我新加的 TraceRecorder cap 代码，无回归）。
> - **新长跑就位 + 验完成 (PID 14576, 07:45:13→09:00:09 UTC, 75 min)**：trace 实时被 cap 关——log 里出现 2 条 `[warning] TraceRecorder: closed ...event_trace.jsonl after reaching disk-budget cap 200 MB`，cap 实战生效。结束后 run_dir 总 **1.73 GB**（vs 旧版 ~90 GB → **52 倍降低**），D: free 244→243 GB 没动。
> - **第三次 60-gen harmony_check 7/7 PASS** (structural-mutation v2 with disk-cap)：C1 hunters PASS / C2 herbivores PASS / C3 coop peak=2766s / C4 scavengers PASS / **C5 worksites total=113** (最高，curriculum-v2=88、structural-v1=110、baseline=38) / **C6 me_coverage peak=0.400 qd=40882** / C7 no extinction。**disk-cap 不影响训练结果，与 v1 完全可比**。
> - **第四次 60-gen 完成 (PID 46200, 09:04→10:27 UTC, 84 min)**：harmony_check **7/7 PASS** (curriculum-v2 with disk-cap)。C5 worksites total=109、C6 me_coverage peak=0.400 qd=52804、C3 coop 2165s、C4 omni fit 5072。2 条 cap warnings 触发，run_dir 1.74 GB。
> - **第五次 60-gen 完成 (PID 52936, 10:30→11:12 UTC, 42 min)**：harmony_check **7/7 PASS** (throw_penalty v2 with fix + cap)。但**指标显著降级**：C5 worksites total=**34** (vs curriculum/structural-v2 109-113)、C6 me_coverage peak=**0.320** (其他 4 个 0.40-0.48)、C3 coop peak=1680s (其他 2165-2766s)、C4 omni fit=3443 (其他 3443-5072)。**关键发现**：throw_penalty `-0.05` 仍然抑制 build/coop 行为，但 fix 把它从"5/7 负优化"救到"7/7 PASS 但低位"——证明 **Stone Bootstrap commit 4 fix 是通用根因修复，不是 graduate 特例**。throw_penalty 是 reward shaping 副作用而非 NurseryProgression bug。
> - **第六次 60-gen 完成 (PID 43996, 11:15→11:59 UTC, 44 min)**：harmony_check **7/7 PASS** (clean baseline `long_run_60gen_2d.toml` with cap，无 archetype_progression_persistence / curriculum / structural / throw 修饰)。C5 worksites total=**40**、C6 me_coverage peak=**0.440** qd=66394、C3 coop 1986s、C4 omni fit=3265、protagonist gen 59 avg=**3223** (六轮最高)。3 条 cap warnings 触发。**关键观察**：无 progression_persistence 时 worksites 较低 (40 vs graduate 88-113) 但 me_coverage 更高 (0.440 vs 0.400)，protagonist fitness 也更高——这是 graduate pipeline 与 baseline pipeline 的天然 trade-off。
> - **六轮 60-gen 全部 7/7 PASS** ✅。fix + cap 在 baseline / curriculum / structural / structural+mut / throw_penalty 五种不同 config 下都稳定。
> - **第七次 80-gen 已起飞**：12:02:19 UTC 起 PID 52972 跑新建的 `long_run_80gen_2d_curriculum.toml` (curriculum-v2 with cap，`generations=80`)。目标：(1) 验证 fix+cap 在 60→80 代延长下继续稳定；(2) 验证 ME-Elites coverage / worksite count 在更多 gen 下是否继续累积；(3) 满足用户"训练跑到 gen 70-80+"诉求。预计 ~14:00 UTC 完成。
> - 下一节点：等第七跑完成 → 进 **#3 main world continuous tick** (FINAL_BLUEPRINT 最大遗留模块) — 主世界永生 + 自己 reproduce 的核心机制。
> - **六轮 AB 对照表（截止 11:59 UTC）**：
>   | run | toml | 时长 | C5 worksites | C6 me_cov peak | C3 coop | C4 omni fit | gen59 fit | run_dir | cap fired |
>   |---|---|---|---|---|---|---|---|---|---|
>   | curriculum-v1 | post-fix | 190 min | 88 | 0.480 | 2376s | - | - | ~85 GB | 否 |
>   | structural-v1 | post-fix+mut | 179 min | 110 | 0.440 | - | - | - | ~85 GB | 否 |
>   | structural-v2 | +cap | 75 min | **113** | 0.400 | 2766s | 4453 | - | **1.73 GB** | 2 |
>   | curriculum-v2 | +cap | 84 min | 109 | 0.400 | 2165s | 5072 | 1786 | **1.74 GB** | 2 |
>   | throw-v2 | +cap+penalty | **42 min** | **34** ↓ | **0.320** ↓ | **1680s** ↓ | **3443** ↓ | 2352 | 1.73 GB | 2 |
>   | **baseline-v2** | clean+cap | **44 min** | 40 | **0.440** | 1986s | 3265 | **3223** | 1.80 GB | 3 |
>
>   **关键观察**：(1) cap-on 比 cap-off 跑得快 2.4 倍；(2) 指标全程持平/略高（除 throw_penalty 实质降级）；(3) **6 个 runs 全 7/7 PASS** 跨 5 种 config；(4) throw_penalty 降级模式与 fix 前完全不同 (worksites 减半 vs 全崩)；(5) **baseline (clean toml) gen 59 protagonist avg=3223，是六轮最高** — 无 curriculum 压力时 protagonist 自由发展 fitness 反而更高；(6) graduate pipeline 用 me_cov / fitness 换 worksite 完成度（trade-off 健康）。

> **2026-05-28 凌晨 Stone Bootstrap commit 4 fix 完整验证（graduate AB 60-gen 第二轮：harmony_check 5/7→7/7）**：
> - **commit `fb2c9c5`**（`master`）：`routes/protagonist_ecology/world/NurseryProgressionLayer.cpp` +54 行。`seedFromProgressionState` 检测每个 unlock flag 的 false→true 跨 episode 转换，立即调对应 layer 重放物理世界 side-effect（stone → 10×ScatterStone via director；heavy → heavy_unlock_count_×ScatterHeavyStone；first_fire/second_fire → `campfire_->igniteNextDormant()`；worksite → `worksite_->activateNextDormant()` + 1×ScatterStone support replay）。null-check 守 collaborator 指针让原 unit test 继续过。`test_core.exe` **218/218 PASS**，`NurseryProgressionPersistence` 3/3 PASS。
> - **根因**（不是文档说的「stone 解锁路径不是 6 sticks 累积」那条假设，是真正的代码 bug）：原版 `seedFromProgressionState` 只 OR-merge 5 个 flag，但没重放物理世界的 side-effect。每个新 episode 用 `createBootstrapWorld` 起全新世界（全 Stick、零 Stone、所有 campfire/worksite 都 dormant）。seed 把 `stone_unlocked_=true` 灌进来后，`dispatchRandomPendingEvent` 里 `if (!stone_unlocked_ && deposits >= 6)` gate 短路，ScatterStone 永不再发——agent 继承 "stone_unlocked=true" 但活在零石头世界，整 60 代 deposits_stone=0、worksite=0、me_coverage workcraft 轴塌缩到 0.16。**C5 + C6 是同一个 bug 的两个症状**。文档第 23 行猜的「stone unlock 真实路径不是 6 sticks 累积」是错的——路径是对的，但 seed 阶段没复刻它对世界的副作用。
> - **三段验证**：
>   - **(1) 全测试** `test_core.exe --gtest_filter='*' --gtest_output=junit:test_results.xml`：**218/218 PASS**，无回归。
>   - **(2) 4-gen smoke**（`tmp/layer1/smoke_4gen_curriculum_fix.toml` = curriculum.toml + generations=4，binary `build_ninja_cuda/bin/neural-eco-protagonist.exe` 2026-05-28 00:35 编入 fb2c9c5）：原 60-gen builds 全 0；smoke 4-gen 在 gen 2/3 各刷出 1 个 build；世界 stones>0 ticks 0→885；deposits_stone>0 行 0→399；me_coverage gen3 终值 5/25 (0.20) **已超原 60-gen 终值** 4/25 (0.16)。
>   - **(3) 60-gen 全跑** (`tmp/layer1/long_run_60gen_2d_curriculum.toml`，WMI `Win32_Process.Create` 启动避免 SSH 父进程拖死，PID 29416 01:04:16→04:14:44 UTC，190 min)：
>     - `data/runs/long_run_60gen_2d_curriculum/20260528-010417/` checkpoint_gen{9,19,29,39,49,59} 全 dump 完整；evolution.csv 60 行 fitness.csv 60 行 borderland_metrics.csv 完整。
>     - **harmony_check 全 7 条 PASS**（baseline 是 7/7，原 5/7 修复达成）：
>       | # | 检查 | 旧 graduate AB | **本次 fix** | baseline |
>       |---|---|---|---|---|
>       | C1 hunters active | PASS | PASS | PASS |
>       | C2 herbivores attrition | PASS | PASS | PASS |
>       | C3 coop co-presence | peak 2473s | **peak 2376s** | peak 3254s |
>       | C4 scavengers fed | PASS | PASS | PASS |
>       | **C5 worksites completed** | **0/total=0 ❌** | **peak=2/gen total=88 ✅** | peak=?/total=38 |
>       | **C6 me_coverage** | **0.16 ❌** | **peak=0.480 qd=62118 ✅** | 0.40 |
>       | C7 no extinction | PASS | PASS | PASS |
>     - **关键 gen 数据**（fitness.csv me_coverage 列）：gen0=0.08, gen10=0.12, gen15=0.32 (首次破 0.30), gen30=0.36, **gen39=0.48 (peak)**, gen59=0.36。**比 baseline 0.40 还高 20%**。
>     - **关键事件**：gen 10 ep 1 stone 有机解锁（01:23:28）→ 持久化 → gen 11 ep 0 tick=0 立刻看到 `stones=1`（fix 在 seed 阶段 submit 了 10×ScatterStone）+ `active_fires=1`（first_fire replay）。gen 12 ep 1 worksite 持久化解锁。后续每代 builds=1-2 稳定。
> - **结论**：graduate pipeline (sandbox→borderland→main_world + triple_world + curriculum persistence) **真正闭环**，比 baseline 不退步反而 me_cov 略高、build 总数 88 vs 38。下一步可以直接上 structural-mutation AB。
> - **不打扰用户**：用户在线下睡觉，整个流程（智能体决策→改代码→smoke 验证→60-gen 跑→提交）全自主，零弹窗。
>
> **2026-05-26 中 D-044 disk-budget hardening（防 190GB 日志复发）**：
> - **背景**：用户报告之前出过"190G 巨型日志"，本次会话排查根因。CreditLedger.cpp 注释自爆 *"2026-05-23 incident hit 7.07 GB / 110M rows"*。盘上还残留 `data/runs/_credit_log.csv` 2.1 GB + `tmp/*.log` 5 个共 856 MB。
> - **三道护栏**：(1) `evaluatePopulation` 签名 `bool append_episode_debug_csv = false` → `const std::filesystem::path& debug_csv_dir = {}`；ablation 调用点传 `run_dir`，training 默认空。3 个 hardcode 全局路径 `data/runs/_credit_log.csv` / `_communication_metrics.csv` / `_signal_debug.csv` 全部迁到 `<run_dir>/_xxx.csv`，不再跨 run 累加。(2) `CreditLedger::appendCsv` + `CommunicationMetrics::appendCsv` 各自加 200 MB 文件级 cap，超过则 LOG_WARN 一次 + skip。(3) Runtime.cpp 内联 `_signal_debug.csv` 同样 200 MB cap。
> - **额外好处**：`scripts/protagonist_communication_mi.py` 注释本来就期望 `<run_dir>/_communication_metrics.csv`（hardcode 路径之前是 bug），路径迁移**反而修了脚本和代码的不一致**，无需改脚本。
> - **验证**：`neural_eco_route_protagonist` 编译干净；`test_core --gtest_filter=*CreditLedger*:*Communication*` **20/20 PASS**。
> - **腾盘**：删除 6 个大文件共 ~3 GB（2.1G `_credit_log.csv` + tmp 下 5 个 run.log）。
> - **影响**：未来 ablation runs 的 `_credit_log.csv` 等会落到该次 run 自己的 run_dir 下；旧分析脚本如果硬码 `data/runs/_credit_log.csv` 需要改成扫 run_dir。`docx/` 里的历史描述（如本节、DEVELOPMENT_TASK_PLAN line 33/40、METRICS_FIELD_GUIDE line 206/218、MEMORY_EVIDENCE_AUDIT line 324）保留原文以反映写入时刻状态，不是 bug 而是历史档案。

> **2026-05-25 晚 60-gen graduate AB 完整结果（关键发现）**：
> - **AB 启动 21:05 → 完成 23:47 (2h42min, 60 gen × 2 ep)**。binary 是 commit K (20:42) 编的，commit L/M/N/O 没进 binary。
> - **harmony_check 5/7 PASS** vs baseline `long_run_60gen_2d.toml` 7/7：
>   - ✅ C1 hunters / C2 herbivores / C3 coop (peak 2473s, baseline 3254s -24%) / C4 scavengers fed / C7 no extinction
>   - ❌ **C5 worksites=0/total** (baseline 38) — completed_worksites 全 0
>   - ❌ **C6 me_coverage 0.16** (baseline 0.40, 阈值 0.30) — protagonist diversity 大幅下降
> - **borderland_metrics_summary verdict: HEALTHY (partial)** — 9 archetype eval 全工作 (large_herbivore mean fit 549→4693 +755%, ambush 100→2470 +2370%, protagonist 4415→9548 +116%)，6 cycle b2m promote 全跑 (border=288/main=258), 没 extinction。但 csv 限制：背景物种 promoted_to_main 列全显 0 因 commit H 只记 protagonist 那列（这是 csv 限制不是真实 0）。
> - **关键失败原因（待下个 session 调）**：deposits_stick gen 0=5 / gen 29=1 / gen 58=46，但 deposits_stone **整个 60 代全 0**。Stone bootstrap commit 1/2/3 跨 episode/process 持久化机制工作正常，但 **stone 的实际解锁路径不是 "6 sticks 累积" 这条**（gen 58 已经 46 stick deposit），是别的（first_fire? worksite preactivate? 主世界 task 设计？）。需要查 NurseryProgressionLayer 的真正 stone unlock 条件。**worksite=0 → C5 永 FAIL**。
> - **二次失败原因**：triple_world 每代 sandbox→border 抽走 4 个 protag 精英 + 16 bg 精英，sandbox NEAT crossover 留下来的基因质量降低 → me_coverage 0.16（baseline 0.40 跌 60%）。这是 triple_world 设计的代价，可能要 promote 频率减半（period_gens 10→20）或 top_k_protag 4→2。
> - **结论**：graduate AB 暂时 **比 baseline 退步**（7/7 → 5/7）。triple_world + curriculum 全开会拖累 protagonist 主任务。下个 session 需要：(a) 查 stone unlock 真实路径并修；(b) 调 triple_world period/top_k 减少 sandbox 精英抽走；(c) 之后再启用 commit L 结构性 mutation 跑 structural-mutation AB。
>
> **2026-05-25 下午 L6 R3 commit L + M + N + O（结构性 NEAT mutation + metrics + per-archetype eval flavor）+ 60-gen graduate AB 启动**：
> - **Commit L** (`138eacb`)：`BorderlandEvaluator::mutateStructuralPool(archetype, add_conn_rate, add_node_rate, seed)`。Per-genome Bernoulli 决定是否调 add-connection / add-node mutation，mirror sandbox `NeatEvolution::addConnectionMutation` + `addNodeMutation` (24-attempt retry on add-conn, edge-split on add-node)。Pool-local `InnovationTracker` 先扫一遍池子，把 next_node_id 推过 max NodeId 防冲突。ProtagonistGenome (dense) dynamic_cast skip。Runtime hook 紧跟 mutatePool 之后，seed offset +13 跟 weight perturb rng 独立。Toml 字段 `borderland_mutate_add_conn_rate / add_node_rate` 默认 0=off。3 个新 gtest (AddsConnections / AddsNodes / ZeroRatesNoop) 全 PASS。
> - **Commit M** (`0659c8e`)：`borderland_metrics.csv` header 加 `node_count_avg, connection_count_avg` 两列。Per-row 对池里 NeatGenome 累加 nodes/connections，protagonist 池写 0 (dense 没结构概念)。`borderland_metrics_summary.py` 表加两列 + 新增 **Structural mutation: ACTIVE/OFF** 诊断行 (阈值 > 0.5 avg new nodes 或 > 1.0 avg new edges)。配套 2 个 toml-parse gtest (defaults / parses)。Total tests 现 **218/218 PASS** (commit L 加 3 + commit M 加 2 vs 上次 213)。
> - **Commit N** (`ed640a0`)：第一个 per-archetype eval flavor。`miniTickBackground` 加 `prey_pool` 参数；当 archetype 是 ApexPredator/PackHunter/AmbushPredator 时，从 LargeHerbivore borderland pool 拉一个 top-fitness 的 herbivore genome 注入 mini-world 当 hunting target。这给 predator 选择真信号 (commit F 的 1-agent isolated mini-world predator 全打 0)。LargeHerbivore 不在 background_archetypes 时 prey_pool=null，silently fall back 到原行为。Scavenger / Omnivore / Rabbit 留下次 commit。5/5 borderland gtest 仍过 (placeholder path 不变)。
> - **60-gen graduate AB 后台跑中** (`tmp/layer1/run_curriculum_60gen.bat` started 21:05)：全 pipeline 启用 — curriculum.archetype_progression_persistence + triple_world (period=10, max_pool=32, eval_period=1, min_continuity=5, sigma=0.01, perturb=0.8, reset=0.05, extinction_rescue=2)。**没启用 commit L/N** (binary 是 commit K 时刻编译的 20:42，commit L/M/N 改的代码不进当前 binary)。下次 long-run 才会用到这些。
> - **下一轮 structural-mutation AB 配置已就位** (`8c71f51`)：`tmp/layer1/long_run_60gen_2d_structural.toml + run_structural_60gen.bat`。在 graduate AB 配置基础上加 `borderland_mutate_add_conn_rate=0.10 + add_node_rate=0.04`。等 graduate AB 跑完 + binary rebuild 后即刻可启动。
>
> **2026-05-25 中午 端到端 smoke 验证 + 60-gen throw_penalty AB 收口**：
> - **Smoke `smoke_tw_eval.toml`（4 gen, sigma=0.01, perturb=0.8, reset=0.05）**：完整跑通 commit A-K wiring。9 个 archetype 全部 mini-tick + mutate；protagonist pool 3→12；large_herbivore mean fit **489→3178 (6.5×)**；omnivore **421→3159 (7.5×)**；10 protagonist b2m promoted；borderland_metrics.csv 36 行。**borderland_metrics_summary.py verdict: HEALTHY**。整套 graduation pipeline 端到端真闭环：sandbox→border→mutate→eval→cull→continuity-gate→main_world.
> - **60-gen throw_penalty AB 完整结果**：throw_attempt = -0.05 验证为**负优化**。harmony_check 对比：baseline 7/7 PASS vs throw_penalty **6/7 PASS** (C6 me_coverage 从 0.40→0.28 退到 FAIL)；C3 coop -24% / C4 omnivore peak -26% / C5 worksites total 38→28 (-26%)；所有 quantitative 指标全部退步。throw_attempt toml slot 默认 0=off 不需撤回；只是 `long_run_60gen_2d_throw.toml` 那个实验 config 别再用，改回 `long_run_60gen_2d.toml` baseline。throw spam 问题 (10000/ep × 14% 命中) 需要别的机制压制 (inventory 损失或碰撞 penalty)，不是直接每 throw 扣分。
>
> **2026-05-25 上午 L6 R3 commit J（真正的 borderland evolution）**（commit `6b1e24a`）：
> - 之前 commit E/F/G 只能做"选择"，同一 genome 永远评出同样 fitness。Commit J 加上"探索"。
> - `TripleWorldRuntime::mutateBorderlandPool(archetype, mutator)` 走 pool 内每个 genome 调 caller-supplied mutator。stashed_fitness 保持，下次 evaluateBorderland 走 max(aged, fresh)。
> - `BorderlandEvaluator::mutatePool(triple_world, archetype, sigma, seed)` 对 ProtagonistGenome 的 dense 权重 (input/hidden/action/interface) + NeatGenome 的 ConnectionGene::weight 做 Gaussian perturb。dynamic_cast 分派 (IGenome 没有 uniform mutate API)。
> - toml gate `[runtime.triple_world] borderland_mutate_sigma`（默认 0.0 = off）。
> - Runtime hook 顺序：promote → **mutate (commit J)** → evaluate → cull → continuity-gate → b2m promote → metrics csv (commit H)。
> - Smoke config `smoke_tw_eval.toml` 加 `borderland_mutate_sigma = 0.01` 让端到端 wiring 验证就位。
> - 4 个新 gtest (Mutate pool applies/no-op + 2 config parse)。**213/213 total**。
> - 整套 commit A-J 现在是真正的 "borderland 内部 evolution arena"：sandbox 提供 founder → borderland mutate + evaluate + cull 反复迭代 → 50-代物种延续 gate 决定毕业 → main world。
>
> **2026-05-25 上午 L6 R3 commit H/I（数据可视化 hook）**：
> - **Commit H** (`67f2154`)：`borderland_metrics.csv` per-(generation, archetype) 行写入。Columns: generation / archetype / pool_size / mean_fitness / max_fitness / borderland_eval_count / promoted_to_main / culled。empty pool 跳过。File created unconditionally。
> - **Commit I** (`14ba6e3`)：`scripts/borderland_metrics_summary.py` 配套审计脚本。读取 CSV，per-archetype markdown 表（pool first→last / mean fit drift / eval_count / promoted total / culled total）+ 全局 verdict (HEALTHY / GATED / STAGNANT / INACTIVE)。一行命令分析"borderland 是否真在 evolve"。
> - **Commit H/I 是分析 infrastructure**：让长 run AB 完成后能立刻判断 borderland evolution 健康度，不需要手翻 60-gen log。 mirrors `harmony_check.py` 模式。
>
> **2026-05-25 上午 L6 R3 stage 3 commit D/E/F/G 四连**（commits `8aa4b5f` + `2883226` + `5ca63ae` + `d5f8a5f`）：
> - **Commit D** (`8aa4b5f`)：BorderlandEvaluator 骨架 + placeholder 占位 (fitness=complexity\*100)。把 commit B 主 loop inline lambda 抽到独立类。
> - **Commit E** (`2883226`)：protagonist (archetype 0) 走真 mini World2D tick。每 genome → `createBootstrapWorld(route)` → `makeProtagonistAgent(id=1, g, ...)` → tick 100 substep × 0.1 sim_sec = 10 sim_sec → fitness = alive_substeps + agent.fitness()。Deterministic per-genome RNG.
> - **Commit F** (`5ca63ae`)：background archetype (1..8) 走 same mini-tick。API 加 optional `background_archetypes` vector 映射 archetype index → SpeciesArchetype。`miniTickBackground` 用 makeBackgroundAgent，try/catch wrap 防 genome 类型错配 crash。`tickAndScore` 共享 helper。
> - **Commit G** (`d5f8a5f`)：protagonist mini-world 注入 ≤ 3 个 peer（同 pool 的其他 protagonist genome），让 cooperation / signal / co-presence reward 路径真触发。focal agent fitness 才被读，peers 只是生态压力。Background 仍 1-agent (cross-archetype mix 留 commit H+)。
> - **smoke configs 已就位**：`tmp/layer1/smoke_tw_eval.toml` + `run_smoke_tw_eval.bat`（4 gen smoke, period=1 + aging=1.0）+ `tmp/layer1/long_run_60gen_2d_curriculum.toml` + `run_curriculum_60gen.bat`（60 gen AB 启用 curriculum gate, 验证 stone bootstrap 端到端）。Binary 空闲后立刻能跑。
> - **整套 L6 R3 stage 3 7 commits 总成**：toml gate `borderland_eval_period_gens > 0` 启用后，**每 archetype 都跑真 mini world** + protagonist mini-world 还带 3 个 peer。borderland fitness 真反映 borderland 选择压力。
> - **测试总数**：209/209（commit E/F/G 没加新 test，unit test 构造 ProtagonistGenome + full route 复杂度高于实施本身，留 smoke 验证）。
> - **未做**：(a) smoke 验证（binary lock 中）；(b) per-archetype eval flavors (e.g. predator 加 prey, builder 加 worksite candidate)；(c) 完整 50-代物种延续闭环（gate 已在 commit C 落，但还没在长 run 中验证 promote→demote 节奏）。这些是后续 commit H+ 工作。
>
> **2026-05-25 上午 Stone bootstrap commit 1/2/3 三件全 done**（commits `199b510` + `7cf32da` + `c087a41`，每件都有 unit test，smoke 等 binary 空闲）：
> - **Commit 1** (`199b510`)：NurseryProgressionLayer 跨 episode API。新 `ArchetypeProgressionState` struct（5 monotonic unlock bool）+ `seedFromProgressionState(state)` setter + `progressionState()` getter。setter 是 OR-merge 语义，stale-false 永不清掉已 true 的 flag。3 个 gtest（DefaultStateIsAllFalse / SeedRoundtrips / SeedIsMonotonicOrMerge）。
> - **Commit 2** (`7cf32da`)：主 loop hook + toml gate。`ProtagonistEcologyConfig::CurriculumConfig::archetype_progression_persistence_enabled`（默认 false）+ runTraining 主 loop 持有 `std::array<ArchetypeProgressionState, 9>` slot 0=protagonist；createBootstrapWorld 后调 seedFromProgressionState，evaluatePopulation 后 OR-merge 回 array；spdlog::info 日志状态变化。2 个 config gtest（CurriculumArchetypeProgressionPersistenceDefaultsOff / ParsesTrue）。
> - **Commit 3** (`c087a41`)：跨进程持久化。`MainWorldSnapshotMetadata` 加 `std::vector<ArchetypeProgressionState> archetype_progression`；save() 写 `archetype_progression` JSON 数组（empty 时跳过 → 老 snapshot consumer 看不到 schema 变化）；load() parse 数组；JsonScanner 加 readBool helper；ProtagonistEcologyRuntime 在 save metadata 时 dump 当前 array、load metadata 时 OR-merge。2 个 gtest（ArchetypeProgressionRoundtrips + EmptyProgressionLoadsEmpty）。
> - **整套总成**：API → runtime hook → JSON schema → 跨进程持久化全 closed-loop。toml gate 默认 off → 现有 train 配置完全不受影响；启用后 protagonist "stone unlock" 跨 episode 跨进程 sticky。**未做**：smoke test（需要 neural-eco-protagonist binary 空闲）+ AB 长训证明 stone deposit/gen 数字真升上去。
> - **测试总数**：200 → **207**（+7 = 3 + 2 + 2）。
>
> **2026-05-25 上午 Layer 6 R3 stage 3 三件落地 + harmony 工具 + stone bootstrap 提案 + 60-gen throw AB 后台跑**（commits `c14598b` + `be9dd43` + `8a49dbb`）：
> - **背景**：用户路线图选择"1→2→3→4→5 顺序走"，且要求"训练时手头还有可以并行跑的任务就并行跑"。本会话后台启动 throw_penalty 60-gen AB 长跑（`tmp/layer1/long_run_60gen_2d_throw.toml`，2D BD + `throw_attempt = -0.05`，run_dir `data/runs/long_run_60gen_2d_throw/20260524-185129`），同时前台推进 graduation pipeline 收尾 (#2)。
> - **Trace 配置改动**：`tmp/layer1/long_run_60gen_2d_throw.toml` 把 `[trace].max_generations` 从 4 改成 60、`agent_interval`/`world_interval` 从 10 改成 20、`viewer_frame_interval` 从 40 改成 80（控制 disk overhead）。60-gen full trace 启用第一次落地，每 gen ~50MB（event_trace 主头），60-gen 总 17GB ≤ D 盘 free space。
> - **Stone bootstrap 提案**（`docx/supporting/STONE_BOOTSTRAP_PERSISTENCE_PROPOSAL.md`，~200 行）：read-only 调研报告，给出 4 个方案对照。推荐方案 D archetype-level unlock state 持久化（与 MainWorldPersistence 哲学一致），分 3 个 commit 落地，~250 行总，4 test。
> - **Harmony check 脚本**（`scripts/protagonist_harmony_check.py`，~200 行）：内部审计工具，读 `protagonist_behaviors.csv` + `species_metrics.csv` + `fitness.csv`，给出 7 项 harmony checklist 的 PASS/FAIL 报告（hunters active / herbivores attrition / coop co-presence / scavengers fed / worksites completed / me_coverage ≥ 0.30 / no extinction）。验证上次会话 baseline `long_run_60gen_2d/20260524-171849`：**7/7 全过**（me_cov 0.40 / coop 3254s / 38 worksites total / 全 archetype 存活），说明训练已经接近"和谐"标准线。
> - **L6 R3 stage 3 commit A** (`c14598b`)：`TripleWorldRuntime::evaluateBorderland(archetype, eval_fn, aging_factor)` API。Spec § 1.2 "50 代考验 + 不灭绝才进入主世界"。stage 2 + R2-stage-3 留下 borderland 是 fitness pool accumulator，每个 genome 的 stashed fitness 是 sandbox→borderland promote 时的快照，从未重新评估。新 API 走 borderland pool，调 caller 提供的 eval_fn（让 TripleWorldRuntime 保持 World2D/brain 无关），用 `max(aged_old, fresh)` 更新 stashed fitness。aging_factor=1.0 默认无 aging；< 1.0 让停止表现的 genome 渐渐丢掉领先优势。max 语义防止一次差评把已证明的 survivor 立刻 demote。2 个 unit test（identity eval + aging factor decay）。
> - **L6 R3 stage 3 commit B** (`be9dd43`)：主 loop hook。toml gate `[runtime.triple_world] borderland_eval_period_gens`（默认 0=off）+ `borderland_eval_aging_factor`（默认 1.0）。Runtime 每 N 代调一次 evaluateBorderland，eval_fn 当前是 no-op identity（按 id 配对查 stashed fitness 返回）— commit B 只负责把 plumbing 接通，commit C 才会替换成真正的 borderland tick。INFO 日志：`TripleWorld gen N: borderland eval (no-op identity) updated K genomes (aging=X.XXX)`。2 个 config parse test（默认 disabled + toml roundtrip）。
> - **L6 R3 stage 3 commit C** (`8a49dbb`)：50-代 物种延续 gate。`WorldPopulationPool` 加 `borderland_eval_count`（每次 evaluateBorderland 自增）。toml gate `borderland_min_continuity_gens`（默认 0=off）。`promoteBorderlandToMain` 之前按 archetype 检查 eval_count ≥ min_continuity；未达标 archetype 跳过（其他 archetype 仍正常 promote）。INFO 日志：`TripleWorld gen N: borderland continuity gate blocked K archetype(s) from main promote`。2 个 gtest（eval_count tracking + early-return 不 bump counter）。
> - **L6 R3 stage 3 真正没做的 (deferred)**：commit B 的 eval_fn 仍是 no-op identity，没有"真正在 borderland 跑 World2D + tick N ticks + 评 fitness" — 那需要 reuse `createBootstrapWorld` 或新建一个 minimal borderland world factory，工程量 ~1000+ 行新代码 + brain wiring。**这是下一个 session 的最高优先级**。当前 plumbing 已就位，只等真正的 evaluator 函数实现。Smoke (`smoke_tw_eval.toml`) 也 deferred — 60-gen AB 训练在跑没法用 neural-eco-protagonist binary。
> - **测试总数**：194 → **200**。
>

> **2026-05-25 凌晨 训练收口审计：3D BD 验证为负优化 + throw_penalty 机制就位**（commits 用户 push 时 + `da3fc4f`）：
> - **背景**：用户质疑"是不是过早进可视化阶段，训练真的闭环了吗"。本会话先把 long run #10 60-gen 跑完（3D BD `tmp/layer1/long_run_60gen.toml` seed=7777, 70 min），再跑 2D BD AB（`tmp/layer1/long_run_60gen_2d.toml` 把 hunt 维度撤掉退回 5×5=25 cells）做对比。
> - **3D BD 60-gen 结果（baseline）**：avg fitness 1185→2774（2.3×），best peak 13236（gen 1 spike）后回落到 10684（gen 58），me_coverage 全程 1.5-4%（8 cells / 192）。Build chain gen 35-40 才觉醒，crafts 0→10-16，但 worksite 完成数仍 0-1/gen，stone deposit **0-2/gen**（核心瓶颈）。
> - **2D BD 60-gen 结果（AB）**：avg fitness 1185→**5917**（gen 58 峰，5×），best peak **15248**（gen 58, 1.43× 3D），me_coverage 早期就 0.08-0.16，gen 14 跳到 0.32，gen 28 顶到 **0.40 (10 cells / 25)**，qd_score 峰值 65183。Build chain gen 8-10 就有 worksite 完成事件，gen 28-32 出现 142-169 deposits + **6-13 stone deposits/gen**（stone 真正打通！）+ 18 crafts。
> - **结论**：v17 hunt-axis 3D BD 192 cells 验证为**负优化** — Vassiliades 2017 spec 早预测的 curse of dimensionality 真发生：archive 大半时间空着，selection 退化成 "≈ random NEAT"，进化没有 diversity 信号。下次训练默认应回 2D BD（`long_run_60gen_2d.toml` 已就位），或者切 CVT 模式（早已实现，从未真正测过）。
> - **Throw_penalty 机制**（commit `da3fc4f`, +73/-3 lines, 194/194 tests）：3D BD 评估发现 throws 8-22k/gen × 14% 命中 = network spam throwing 浪费 inventory（thrown stone 永远到不了 worksite）。新 toml gate `reward.goal_completion.throw_attempt`（默认 0=off）走现有 HRL 子奖励数组 slot 13；ThrowCarriedObjectCapability 调 `noteGoalCompletion(13)`。负权重生效则 N × |weight| 从 fitness 扣。`long_run_60gen_2d_throw.toml` 配 `-0.05` ≈ 10000 throws/ep × 0.05 = -500/ep ≈ 1 worksite_completion_reward 等量，"足够压但不毁灭"。AB 长跑下次会话做。
> - **Stone deposit 调查（read-only）**：`ProtagonistWorldFactory.cpp` line 121 — **training role 下 bootstrap zero stones**，全是 sticks。`NurseryProgressionLayer` 进度 gate：6 deposits unlock stone（10 个 ScatterStone），但每 episode 重置，protagonist 必须每 episode 重学整条链。是为什么 stones 在 3D BD 下永远不到位的根因之一。
> - **测试总数**：192 → 194。
>
> **2026-05-25 凌晨 Layer 6 R2 stage 3 三件全做完**（commits `45f56a6` + `2e24abf` + `39cd7da`）：
> - **Commit 1 borderland cull cap** (`45f56a6`)：stage 2 留下 borderland 永远只增不减，60 代后能涨到 240+ genome/archetype。新 `TripleWorldRuntime::cullBorderland(archetype, max_pool_size)` 按 stashed fitness 留 top-N 砍其余，每次 sandbox→borderland promote 之后调一次。toml gate `[runtime.triple_world] borderland_max_pool_per_archetype` 默认 0=关。Smoke (`tmp/layer1/smoke_tw_cull.toml`, cap=4) 验证：每 archetype 稳定 4，border 总 36 = 9 archetype × 4，culled 数学完全对应。**未做 (R3 deferred)**：真正的 "borderland 进化" 即独立 World2D + 50 代物种延续 test。这次只做 pruning。
> - **Commit 2 MainWorldPersistence pool-only snapshot** (`2e24abf`)：spec invariant #1 "主世界永不 reset" 之前只是进程内保证。新 `MainWorldPersistence::save/load` 把 borderland + main_world pool 写到 JSON manifest + per-genome 二进制 blob (复用现有 `NeatGenome::saveToFile` / `ProtagonistGenome::saveToFile`)。toml gate `persist_dir` 默认空=关；启用时启动 load + 结束 save 自动。Smoke (`tmp/layer1/smoke_tw_persist.toml`, 2 次顺序运行)：run1 end border=36 main=56，run2 start "loaded snapshot border=36 main=56"，run2 end border=36 main=112 — **第一次用具体数字证明 invariant #1 跨进程**。**未做 (R3 deferred)**：完整 World2D snapshot 即 agent/worksite/tree/water/weather/season 字段；当前 borderland/main_world 还是 pool accumulator，只能诚实持久化 pool。
> - **Commit 3 extinction rescue** (`39cd7da`)：spec invariant #4 "灭绝 → fallback drift"。新 `rescueExtinctSpecies(archetype, k)`：main_world[archetype] 空时从 borderland 同 archetype 取 top-K 克隆进 main_world (borderland 不消耗)；非空时 no-op。主循环每代 promote 之前 sweep 所有 archetype。toml gate `extinction_rescue_top_k` 默认 0=关。Smoke (`tmp/layer1/smoke_tw_rescue.toml`, period=999 让 border->main 不触发, rescue=2)：gen 0 rescue 18 = 9 archetype × 2，gen 1-3 main 已非空 rescue 全 no-op，main 稳定 18。
> - **Layer 6 R2 完整收口**：borderland 有界 + main_world 跨进程 + 灭绝自愈三件齐全。R3 仍 deferred 的：(a) borderland 独立 World2D + 50 代评估循环；(b) full World2D snapshot (agent/worksite/tree/water 等)；(c) 完整 incident → challenge → 训练 → 边境 → 迁徙闭环。
> - **测试总数**：192/192（185 + 7 = stage 3 三个 commit 各加 2/3/2 共 7）。
>
> **2026-05-24 深夜 Layer 6 R2 stage 2: 主循环接入**（commit `3254e9e`）：
> - 不再只是 isolated skeleton，TripleWorldRuntime 现在**在真实训练循环里**每代触发 promote。
> - `WorldPopulationPool` 加 `fitnesses` 字段；borderland 现在记得每个 promoted genome 当时的 fitness，所以 borderland→main 可以无需重新评估直接 re-rank。
> - 新 `TripleWorldRuntime::promoteExternalToBorderland(archetype, source, fits, lineages, k)` —— ProtagonistEcologyRuntime 保持拥有 sandbox pool (`population` + `species_contexts[i].population`)，三层 runtime 不 mirror，只取 top-K 克隆到 borderland slot。
> - 新 stored-fitness 重载 `promoteBorderlandToMain(archetype, k)` 用 slot 内 stashed fitness。
> - `[runtime.triple_world]` toml gate 默认 off：`enabled`/`top_k_protagonist_per_gen=4`/`top_k_background_per_gen=2`/`borderland_to_main_period_gens=50`/`borderland_to_main_top_k_protagonist=8`/`borderland_to_main_top_k_background=4`。
> - 主循环 hook：每代 species_evaluated 收完后，build `{genome_id → fitness}` map per archetype，把 top-K 克隆进 borderland；每 `period` 代再 promote borderland→main。INFO 日志：`TripleWorld gen N: sandbox->border protag=P bg=B | border->main protag=p bg=b | border=BT main=MT`。
> - 运行结束写 `triple_world_summary.json` 到 run_dir：每 archetype 槽位的最终 genome 数量。
> - Smoke (`tmp/layer1/smoke_tw.toml`, 4 gen, period=2, seed=9999) 完整跑通：gen 0/1/2/3 全部 promote 触发；end-of-run summary borderland=76 main_world=58，9 个 archetype 槽位 `[12,8,8,8,8,8,8,8,8]` / `[10,6,6,6,6,6,6,6,6]`，与 `top_K × gen / period` 数学完全对应。
> - **未做（stage 3 deferred）**：(a) borderland / main_world 独立 tick + 评估循环（现在只是 accumulator，没有 NEAT crossover 在 borderland 内发生）；(b) MainWorldSnapshot 磁盘序列化；(c) extinction rescue（rescueExtinctSpecies）。
> - **测试总数**：185/185（之前 184 + 新增 1 = 185）。
>
> **早 evening 段**：
>
> **2026-05-24 晚 Layer 6 R2 + Layer 7 R3**：
> - **Layer 7 R3 step 1 完整**（commit `<L7R3-trace>`）：viewer_frames.jsonl 按 LAYER7_GODOT_VIEWER_SPEC § 4.1 补齐 `trees[]`(id/x/y/iv/av) + 天气进度 `wp` + 季节进度 `sp`。view_frames.py 现在画 trees（绿色圆，半径随 integrity 缩放，砍倒后变灰木桩）+ HUD 显示 `weather=N(WP%) season=N(SP%)`。Smoke 验证：第一帧 80 棵树 + wp/sp 数据正常。
> - **Layer 6 R2 skeleton**（commit `2570aa9`）：按 LAYER6_THREE_WORLDS_SPEC § 6 优先级 1 落 in-memory 多世界迁徙骨架。
>   - `WorldRole` enum + `worldRoleName()` + `WorldPopulationPool` + `WorldSlot` 值类型（之前的 R1 只有 toml 字符串 accessor）。
>   - `MigrationScheduler`（无状态，参数化）：`extractTopK` 按 fitness 稳定排序克隆 top-K 保留 lineage_id；`seedFromCandidates` 追加不清空；`promoteTopK` 链式封装。
>   - `TripleWorldRuntime` 持有三个 `WorldSlot` + `promoteSandboxToBorderland` / `promoteBorderlandToMain`；提供 `seedSandboxPool` 测试辅助。
>   - **真实集成到 ProtagonistEcologyRuntime 主 tick 循环 deferred**（spec 明确说"分阶段做"），以及主世界 disk 序列化 + extinction rescue 也 deferred — 这次 commit 只锁迁徙算术不破坏现有流程。
>   - 7 个新 gtest 覆盖：enum 命名 / topK 排序 / k>N 截断 / size mismatch throw / append 语义 / sandbox→borderland 往返 / borderland→main 往返。
> - **测试总数**：184/184 pass（177 + 7 = 184），无任何回归。
>
> **2026-05-24 傍晚 训练就绪审计剩余 3 项收口（#6 / #9 / #10）**：
> - **#6 [MEDIUM] 主角自选 BuildingType**（待提交）：
>   - 新 `Agent::buildingTypeChoice()` uint8_t 字段（core/route 解耦），新 `routes/protagonist_ecology/capability/ChooseBuildingTypeCapability` 4-output argmax。
>   - `WorksiteLayer::activateNextDormant()` 加 vote 覆盖：在 `interaction_radius * 3` 内的 protagonist（archetype_tag==0）多数票决定该 site 的 building_type；无 protagonist 时退回 seed 期 weight 分布（向后兼容）。
>   - toml gate `population.protagonist.building_type_choice_enabled = false` 默认 off，启用时 `action_dim` 必须 +4。**所有现有 toml/checkpoint 不需改动**。
>   - 验证：`Layer5ChooseBuildingType.*` / `Layer5WorksiteVote.*` 3 个新 gtest 全过；联合 smoke `tmp/layer1/smoke_69.toml`（action_dim=17 + DNC memory 同时启用）2 gen × 1 ep 完整跑通无 crash。
> - **#9 [MINOR] 背景种 DNC memory option**（待提交，方案改进）：
>   - 原 audit 建议 "把 NeatBrain 改成可插拔 IBrain" — 实际**用项目已有的 perception/capability 扩展点更优**：新 `BackgroundMemoryLayer`（IWorldLayer，按 AgentId 持有 DNCMemory）+ `BackgroundMemoryReadPerception`（read_vectors 作为输入）+ `BackgroundMemoryWriteCapability`（decode interface vec → DNC step）。
>   - **NeatBrain 完全不动**，所以 ~7 处 `dynamic_cast<NeatBrain*>` 调用点（Hebbian / writeWeightsBackTo / ComprehensiveMetrics / HeadlessRuntime monitoring）全部保持工作。
>   - toml gate `[population.background.dnc_memory] enabled / slots / word_size / read_heads`，默认 off + 默认 tiny 配置（slots=4 word=2 heads=1 → read_vec_dim=2 / interface_dim=16，约 ~32 floats / agent，65 个背景 agent ≈ 8KB 总内存，远低于 audit 估计的 3MB）。
>   - 验证：`Layer5BackgroundMemory.*` 3 个新 gtest 全过（getOrCreate 单例语义、read/write 往返、cull dead agents）；联合 smoke 同上。
> - **#10 [MINOR] 50-100 gen 长训**（**性能证据已收**，binary 不归档）：
>   - 用 reproduction + storage_deposit_reward + must_build_to_survive + Lamarckian Hebbian writeback **全开**的 60-gen 配置（`tmp/layer1/long_run_60gen.toml`，seed=7777）跑了 **13 代 / 60 代** 后被 link 步骤打断（kill 进程腾出 exe）。
>   - 13 代结果：13 次 `PopulationLayer→NEAT bridge: injected N in-world born children into next-gen pools` 全部触发，**累计 352 个 in-world 出生注入了进化池**（16 + 28 + 38 + 85 + 62 + 44 + 29 + 16 + 17 + 2 + 2 + 4 + 9）。Gen 0 高峰是 founder Adult 集中产，后期回落到稳定 2-9/代，符合 reproduction_cooldown_sec=90 的预期节奏。
>   - **意义**：第一次实证 Layer 4 reproduction + #1 NEAT bridge + #2 Lamarckian writeback 在长跑中能持续工作不崩溃（之前所有 reproduction 报告都仅 single-episode smoke 验证）。完整 60-gen 由用户后续按需启动。
>
> **新增**：`tests/route_protagonist/test_layer5_choice_and_bg_memory.cpp` 6 gtest 全过；`test_neat_brain.cpp::InputMismatchSilentDegrade` 替换原 `InputMismatchThrows`（适配 bd07564 silent degrade 行为）。**全测 177/177 pass**。
>
> **2026-05-24 下午 训练就绪审计 9/12 项收口（接 5ccac49 Session summary 后）**：
> - **#1 [SEVERE] in-world reproduction → 跨 episode NEAT pool**（commit bd07564, b0cbfe4）：
>   `PopulationLayer::drainBornChildren()` + `runtime` bridge 把 in-world 出生孩子注入 `species_contexts[i].population`，所以孩子不再只是"画面上看到"，而是真的进化池一员。Smoke 验证：单代 8-10 个孩子注入。
> - **#2 [SEVERE] Lamarckian 真正继承**（bd07564）：新 `NeatBrain::writeWeightsBackTo(NeatGenome&)` 把 Hebbian 在线学习的权重写回 child genome（toml gate `evolution.lamarckian.enabled`）。
> - **#3 [SEVERE] kill() → killWithCause() 全量替换**（ebf4299）：8 个 `agent->kill("...")` 全部改成正确的 `DeathCause::{Starvation/Predation/Combat/Disease/Thermal/Dehydration}`。`Agent::death_counted_` one-shot flag 防止 PopulationLayer 跨 tick 重复计数。
> - **#4 [SEVERE] PopulationLayer O(N²) → O(N)**（ebf4299）：spatial bucket on (archetype_tag, cell_x, cell_y), cell_size = reproduction_radius，3×3 邻域扫描。Smoke 验证 same-seed 输出与原 O(N²) 完全一致。
> - **BUG ROOT CAUSE [pre-existing, fixed bd07564]** — 上一次会话报告"4 in-world births verified"实际是 mid-episode 后立即 STATUS_STACK_BUFFER_OVERRUN 崩溃（每次 reproduction 启用就会复现）。Root cause：`PopulationLayer` 用 `Agent::speciesId()` 配对，但本路径下背景 agent 全是 `kInvalidSpeciesId`，所以 Scavenger（6 outputs）会跟 Omnivore（7 outputs）配对，child brain 用 parent_a 的 outputDim 但 capability 用 parent_a 的 archetype，结果 `Agent::applyCapabilities` 抛 "output slice overflow"，从 parallelFor worker 抛出 → std::terminate → /GS canary。Fix: 改用 `archetypeTag()` 作为同种判定 key。同时把 `Agent::decide` / `decideUntilDense` / `applyCapabilities` 和 `NeatBrain::forward` 的所有 throw 改成 spdlog::error + 静默降级，并给 `AgentLayer::parallelFor` worker 包 try/catch — 让"诊断比崩溃更有用"。
> - **#5 [MEDIUM] Layer 4 vs Layer 5 A/B 验证**（b0cbfe4）：4-gen × 2-ep × seed=4242 同种子对比。Layer 5 reward shift Gen 0/2/3 都比 Layer 4 高 +44/+60/+51 % avg fitness，最高个体 +194%，QD score 提升 +257% 在 Gen 3。Layer 5 不是噪声。详见 `docx/supporting/LAYER5_AB_RESULT.md`。
> - **#7 [MEDIUM] Lookout sense_radius_scale 接入 4 个 perception**（a8dc76e）：`TreePerception` / `WaterPerception` / `DangerPerception` / `ObjectLogisticsPerception` 全部读 `Agent::senseRadiusScale()`。之前只有 `AgentSocialPerception` 用，所以 Lookout 建筑只能"看更远的 agent"，看不到树/水/火/物资 — 现在全部联动。
> - **#8 [MEDIUM] Storage deposit reward + building_type toml shaping**（a8dc76e）：新 `reward.storage_deposit_reward` toml binding，每次 deposit 落到完工 Storage 时按 `cooperation_weight × storage_deposit_reward` 加分。Smoke 启用后从 8 个 children 升到 10 个，me_cov 从 4/192 升到 5/192。
> - **#11 [MINOR] task_template must_build_to_survive penalty**（待提交）：新 toml 字段 `[[evolution.task_templates.must_build_to_survive]]` + `must_build_penalty`，episode 结束时给当 episode 完全没造过 worksite 的 protagonist 扣 200 fitness。把"没造就活不下去"硬编码到训练信号。
> - **#12 [MINOR] Reproduction unit tests**（b0cbfe4）：`tests/route_protagonist/test_layer4_reproduction.cpp`，5 个 gtest 覆盖 NeatBrain::writeWeightsBackTo、PopulationLayer 按 archetype_tag 配对、BornChild 队列、Agent::deathCounted、senseRadiusScale clamp。
> - **2026-05-24 下午被暂缓的 3 项已在傍晚收口**（详见上方 "傍晚 3 项收口" 段）。
>   #6 / #9 / #10 全部完成，audit 12/12 项关闭。
>
> **早一些的工作（5ccac49 之前）**：
> - Layer 1 (a0669f6, 7a098a6): 体温 + 600×600，alive 0.45→0.79
> - Layer 2 (202186b): WeatherLayer 6 type，avg_fit +167%
> - Layer 3 (e3d2b8b): Rabbit + 9 物种，avg_fit 6685 best 12390
> - Layer 4 (da86050, 1377352): LifecycleStage/DeathCause/lineage_id/PopulationLayer，4 in-world births 验证 + perf throttle（注：reproduction crash 这次会话才修）
> - Layer 5 (3501070, da86050, e598450, 5824eb7, b8326ee): BuildingType 4 type + Shelter 雨天免疫 + reward shift + building_type distribution + Storage overflow + Lookout perception bonus
> - **Layer 5 30-gen 长跑结果**: Gen 27 peak protagonist avg=4543 best=18451 (vs v17 baseline 737 = **6.2x improvement**)
> - Layer 6 R1 (b53a4e8): WorldRole borderland/main_world accessors (TripleWorldRuntime 实现 deferred)
> - Layer 7 R1 (456e09b): viewer_frames.jsonl frame dumper (agents/worksites/weather/season JSONL)
> - Layer 7 R2 (617d5df): minimal pygame Python viewer (160 LoC，PEP 723 self-contained)
> - Layer 8 R1 (4d57714): Lamarckian Hebbian online weight adaptation (toml gate)
> - 4 个 docx/supporting/LAYER{5,6,7,8}_*.md 详细设计文档
>
> **2026-05-24 蓝图驱动 Layer 1-5 完整 + 6/7/8 R1 baseline + 设计 spec**：
> - Layer 1 (a0669f6, 7a098a6): 体温 + 600×600，alive 0.45→0.79
> - Layer 2 (202186b): WeatherLayer 6 type，avg_fit +167%
> - Layer 3 (e3d2b8b): Rabbit + 9 物种，avg_fit 6685 best 12390
> - Layer 4 (da86050, 1377352): LifecycleStage/DeathCause/lineage_id/PopulationLayer，4 in-world births 验证 + perf throttle
> - Layer 5 (3501070, da86050, e598450, 5824eb7, b8326ee): BuildingType 4 type + Shelter 雨天免疫 + reward shift + building_type distribution + Storage overflow + Lookout perception bonus
> - **Layer 5 30-gen 长跑结果**: Gen 27 peak protagonist avg=4543 best=18451 (vs v17 baseline 737 = **6.2x improvement**)
> - Layer 6 R1 (b53a4e8): WorldRole borderland/main_world accessors (TripleWorldRuntime 实现 deferred)
> - Layer 7 R1 (456e09b): viewer_frames.jsonl frame dumper (agents/worksites/weather/season JSONL)
> - Layer 7 R2 (617d5df): minimal pygame Python viewer (160 LoC，PEP 723 self-contained)
> - Layer 8 R1 (4d57714): Lamarckian Hebbian online weight adaptation (toml gate)
> - 4 个 docx/supporting/LAYER{5,6,7,8}_*.md 详细设计文档
> - viewer pipeline 端到端可用：C++ runtime → viewer_frames.jsonl → Python viewer 渲染。Godot 4 + Kenney 完整 vision deferred 到下次会话。
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
