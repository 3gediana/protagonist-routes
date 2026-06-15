============================================================
★★ FINAL 2026-06-07 — PE 最终状态（本号收尾）★★
============================================================

> 最新。旧 §0 及 06-06 块保留在下方查史。详见 `routes\protagonist_ecology\handoff\START_HERE.md`（含 FINAL 块）。

【关键认知更正（推翻之前的"两堵墙/没激励"叙事）】
- PE 涌现窄 **不是**"没奖励激励"。读代码确认奖励面早就拉满：建造完成 150、联合狩猎 18、信号共注意 1.5、must_build 生死门、全套协作课程表。我之前说"通讯没适应度激励"是错的。
- 真正根因 = **够不着（achievability）**：资源太稀（600×600 世界才 80 棵树、平均间隔 67 米，主角走不到树边）+ 建造门槛太高（工地要 8 次搬运）。

【全机制成功率实测（gen319，48 主角）—— 几乎全是"空按"】
| 机制 | 尝试 | 成功 | 率 |
|---|---|---|---|
| 投掷 | 5115 | 1952 | 38%（唯一在工作）|
| 砍树 | 36010 | 22 | 0.06% |
| 制造 | 30483 | 6 | 0.02% |
| 建造完成 | — | 0 | 0% |
| 狩猎 | 15171 | 0 | 0% |
| 喝水/通讯响应 | — | 0 | 0% |
> 跟 DP 的"空按"同型：狂按几万次但前置条件凑不齐。砍树/制造/搬运有零星成功，说明链条物理走得通，只是太难。

【档案几何】25 格 = 砍树(5档) × 建造+制造(5档)，狩猎轴已关。所以"铺满 25 格" = 铺开"砍树量×建造量"组合，**根本不需要先解决狩猎/通讯墙**。现在只覆盖 4-7/25 是因为个体都挤在"少砍少建"角。

【对症杠杆 = 纯配置提高够得着性（不重编）】
- seed 50025（从 gen159 热启动，不动现有种子）：树 80→240（3×）、工地门槛 8→4、石料 6→3。
- 配置：`onepot_sD_warm_50025.toml`。
- **结果全面抬升**：me_cov 7→**10/25**、best_fit 1700→**7194**、砍树 22→169、制造 6→19、搬运 26→53、建造完成 **0→1**、狩猎 0→6。第一次让"砍树→搬运→建造"整条链闭合。

【checkpoint 现状（全停训）】50021→gen369、50022→gen349、50023→gen379、50024→gen379；50025 无独立 checkpoint，从 `data\runs\onepot_sD_warm_50002\20260604-115936\checkpoint_gen159` 重启（密集结果已存其 run 目录 CSV）。

【接手下一步】把 50025 的密集配置推广到 50021-24（或新种子），让覆盖整片铺开；不要再死磕近身狩猎（猎物是会进化逃跑的智能体，单主角物理追不上）和通讯。

============================================================
（以下为历史内容）
============================================================


<!-- id: note-67930cdf2dd2488fba481ad1723e3437 | name: protagonist_ecology 项目契约（NEAT 群体路线 · 通宵自主开发版） | author: user | scope: 当用户提到 protagonist_ecology、neural-eco、NEAT 群体演化、24 个 protagonist、600×600 生态箱、Layer 1-8、三层世界、harmony_check、me_coverage、MainWorldHost、TraceRecorder、trajectory analysis、拟真/realism Phase A-F、环境感知、季节天气压力、dot-based viewer、一直跑 run accumulation，或要在 D:\claude-code\c++\routes\protagonist_ecology 下工作时。不触发：deep_protagonist / PPO / libtorch 单 agent（另一条 route，另一个 AI 负责） -->

# protagonist_ecology 项目契约（NEAT 群体路线 · 通宵自主开发版 v6）
>
> **最近更新 2026-05-31（EOD 交接）**：判据已切「节律拟真」(env-perc 永远开、OFF 只阴性对照、尺子=phase-lead 预判)；Phase B 拿到**首个诚实阳性**（可存活压力下 ON 演化出 OFF 没有的"趋火预判"，领先体温最低点 +11~12s，3 seed 一致，但很弱 R²≤0.08）；**下一步=gen→160 rise/plateau 诊断**（candidate 3）。详见 §2 末「2026-05-31 节律拟真」段。

> Note ID: `note-67930cdf2dd2488fba481ad1723e3437`（旧 ID `note-54e426be...` 属旧账号已失效）
> 我只动 `routes/protagonist_ecology/`，**绝不动 `routes/deep_protagonist/`**（另一个 AI 负责，同 repo `D:\claude-code\c++`）。commit 前缀必须 `protagonist_ecology:`。
> **任何 KPI / "已完成" 描述都先用 git log + 真 CSV + 代码验证，不信文档。**

---

## ⏩ 最新状态 2026-06-06（P0 涌现谱 + 交接包），先读这段

> **接手模型：先 SSH 读 `D:\claude-code\c++\routes\protagonist_ecology\handoff\START_HERE.md`**（本轮新整理，UTF-8 干净 + 单命令脚本 `handoff\scripts\`：`launch_seed.ps1` / `monitor.ps1` / `analyze_spectrum.py`）。下面 §0–§N 的铁律/北极星仍有效，"当前状态"以 handoff\ 为准。
>
> **P0 涌现谱（读现有 4 种子 CSV，零代码改动；DP 同期同诊断，详见 DP 侧 `handoff\P0_baseline.md`）：**
> - **"投掷"是唯一真成形的机制**（命中率 ~19.6%）——它们靠远程扔矛/石头吃饭。
> - **近身狩猎死**：hunt 5 万次尝试、成功率 0.002%（**和 DP 撞同一面墙**）。砍树/造物也 5–6 万次≈0 成功。
> - 赢家是**"投掷型杂食通才"**：杂食 niche 适应度翻几倍，apex/pack/ambush 专精被 wash out 打不出头。
> - **零通讯**(`signal_response≡0`，世界里没有"必须听信号才能完成"的任务)、**零生火、零喝水**。
> - 自带 **MAP-Elites coverage 仅 0.2–0.32**，一半种子还在下掉。
> - 诊断＝**标量爬坡→单策略坍缩**，约 1/3 机制宽度（与 DP 平行）。
>
> **P1 方向（待用户拍范围，方案见 DP 侧 `handoff\训练方案_全面涌现.md`）：** ①移植 DP 的两阶段狩猎机制(D-134)拆近身墙；②内置 niche 升级成**真 MAP-Elites**（每生态位各留一个 elite，通才不能 wash out 专精）；③通讯加"信息不对称 + 非协作不可的任务(需≥2 个体放倒的猎物) + 回应信号能提升适应度"。评估口径换成**被填满的生态位数↑/coverage↑**。
>
> **资产/坑（见 `handoff\PITFALLS.md`）：** 完成种子 50001–50007 + 热启动 50021–50024 别删；共用热启动基座=`data/runs/onepot_sD_warm_50002/.../checkpoint_gen159`（上轮误删过共用基座导致新种子全崩）；进程名 `neural-eco-protagonist-batched`、启动前 `set PATH=D:\NVIDIA\CUDA\v12.8\bin;%PATH%`、warm 续跑要把 TOML `generations` 抬过当前代数。

---

## §0 永久北极星：奔真实去 ⭐（2026-05-30 用户锁定）

用户原话："**我就是奔着真实去的，要求你上述说的缺陷都弥补**"。
= **拟真度是永久最高优先级**，不是可选重构。累积训练次之。6 个缺陷逐项补，路线图见 §2。

每阶段流程：**smoke(2-4代) → short(10-20代) → long(60-80代) → harmony 7/7 不破 → commit**。

---

## §1 六条铁律（每次会话首先复述）

用户是大一，看不懂代码/指标，**离线时 AI 全权决策**；半夜弹窗 = 浪费。

1. **决策权 100% 归 AI**：算法/reward/超参/规模/调试方向自己拍；用户只在审美/方向/不可逆改动介入。离线期间永不发 `user_question`。`block_on_user=true` 只在 (a) 用户要求停 / (b) 最终交付汇报。
2. **大局观 > 小指标死抠**：指标长期低 → 先怀疑算法/reward/代码 bug → 根因诊断 → 系统性修。**不为某个数字"给太直接的奖励"**（Goodhart）。
3. **文档不可全信**：STATUS/PROGRESS 常滞后 git HEAD。看到 KPI 先 `git log --oneline -5 -- routes/protagonist_ecology/` + 真 CSV + 代码源验证。
4. **训练预算纪律**：先 smoke/short 验证假设，全过了才上 long。单条假设跑过 60-gen 不通 → 回去查代码，别再调参跑。
5. **三件套同步**（每阶段结束必更，否则下次会话失忆）：`docx/PROGRESS.md` 顶部状态表 + `docx/STATUS.md` 追加巡检段 + **本 knowledge note**（`devin_knowledge_manage action=update`，先 `action=list search=protagonist` 拿真实 note_id，**不要重复 create**）。训练完一批必 commit。
6. **"一直跑"**：长 run 完 → **立刻起下一个**（不汇报、不等指令）；代码模块跑通 → 立刻进下一模块。用户起床应看到"正在跑某 long run 到 gen 70/80"。**允许停的唯一情况**：(a) 用户说停 / (b) 用户说"跑完这组就停"（只停一次）/ (c) 资源耗尽（D盘<30GB）/ (d) harmony 连续 2 次失败且无修复（停下问用户）。

### 用户原话指令归档
| 短语 | 行为指令 |
|---|---|
| 一直跑 | long run 完立刻接下一跑 |
| 别弹窗 | 永不 block 除非最终交付/用户说停 |
| 别为指标加奖励 | 修 bug，不调 reward 推高 metric |
| 每训练完一批就 commit | 每跑 commit PROGRESS+STATUS+note |
| 跑完这一组就停 | 单次停止，仅本批生效 |
| 重点更新 knowledge note | 收尾必更 note |
| 只保留上一代数据 | 分析完立刻删过时 trace |

---

## §2 拟真路线图 A-F（北极星执行清单）

| 阶段 | 缺陷 | 内容 | 状态 |
|---|---|---|---|
| **A 尺度** | 世界小(600×600/24 agent/800 tick) | 放大世界+更多 agent+更长单局/代数（盯 8G 显存） | 部分：环境感知接线已做；尺度放大待办 |
| **B 环境真实压力** | 环境是装饰品、压力≈0 | 季节/天气/温度造成真实生存代价，逼 agent 用环境信息 | **进行中（判据已切「节律拟真」）**：可存活压力下 ON 测得首个"趋火预判"节律(+11~12s，弱)；下一步=gen→160 rise/plateau 诊断（见 §2 末 2026-05-31 段） |
| **C 局部感知** | 感知是聚合量、无 FOV | 真·视野+遮挡+感知噪声（**改输入维→新血脉**） | 待办 |
| **D 连续世代+血缘** | 世代偏离散 | 年龄/亲代抚育/亲缘识别/世代重叠 | 待办 |
| **E 高阶社会** | 无领地/等级/联盟 | 领地、等级、长期联盟、欺骗 | 待办 |
| **F 持久世界史** | 接力只存"脑子"，世界每跑重建 | 扩 MainWorldPersistence 把世界实时状态(位置/工地/树/天气)也存，跨 run 真连续 | 待办 |

**"还剩几个阶段"**：现在 B；后面 = C/D/E/F + A 的尺度放大半截 + dot-based viewer。C/D/E 要起新血脉，较慢。

**关键工程权衡（必记）**：A 的环境感知 + C 都**改 NEAT 输入维** → 旧 checkpoint 被 shape guard 拒（实测）。拟真 > 累积，富世界值得重开新血脉。`protagonistSensoryDim(config)` 必须 == `createProtagonist()` 注册的感知数。

### Phase A/B 进展（commit 实证）
- **A 环境感知**：TimeOfDay/Season/Weather 接进主角 +10 维（新血脉，flag `protagonist_environment_perception_enabled`），commit `652818d`。ON/OFF 对照（各 80 gen 都 7/7）**行为几乎无差异**（诚实负结果 `1aae4ff`）→ 根因：世界没有"非用感知不可"的压力 → 引出 Phase B。
- **B 根因**：体温/饥饿伤害 2026-05-24 被故意调软（防 bootstrap 灭绝）；core_temp 钳在 [20,50]，冬天 5°C 也跌不破 20 → trace 实测 health 0.95 零死亡，压力为零。
- **B 已落地**：trace 加 7 个环境真值列（season/weather/tod/light/season_progress/core_temp/hunger）`b26a9a6`；5 个压力旋钮提到 toml `[world.survival.*]`（`cold_temp_threshold` `hot_temp_threshold` `thermal_damage_per_second` `hunger_damage_threshold` `hunger_damage_per_second`，默认值不变=零行为变化，无需重编译）`e795bdf`。
- **Goldilocks 档**：`cold_temp_threshold=30, thermal_damage_per_second=0.25`。dmg=0.5 太狠（pop 135→62 腰斩）；0.25 = 真差异化压力（health~0.72, 80% 受冻）不灭绝。
- **B #2c 验收**（80-gen seed 50001, dmg=0.25/cold30, env-perc ON, run `data/runs/realism_pressure_long/20260530-172038`）：**harmony 7/7 ALL PASS**（C6 回升 0.440、C7 不灭绝、围猎/食物网/协作/建造全在）。冷是真实代价（受冻 health 0.660 < 不冻 0.862）。gen0→gen79：受冻 health 0.585→0.660、火源接近度 0→1.576 → 更扛冻+被带到篝火附近。**诚实保留**：受冻时火源仅 +0.115、worksite 反而更远 → 主动避寒真实但偏弱（部分成立）。commit `d741122`(REALISM_ROADMAP.md)+`5d0accb`(verdict+脚本)。报告 `docx/PHASE_B_2C_VERDICT.md`。
- **B 下一步（已被 2026-05-31 段取代，保留作历史）**：跑 env-perc OFF 80-gen 对照（同 seed/同 dmg），对比 ON vs OFF 冬季存活/火源接近度。

### Phase B 节律拟真（2026-05-31，rounds 2-7，**判据已校正，必读**）

**判据校正（用户原话指令）**："加环境感知是为了让生物更拟真，目的不是让它比 OFF 表现得更好；env-perc 无论如何都要加，并且要让他表现出真正的节律"。"节律是指他们有白天黑夜、天气、季节的感知后做出的相应行为"。
→ **env-perc 永远开**，验收 **不是** "ON 的 fitness > OFF"；**OFF 只是阴性对照**（没感知就不该有节律）。fitness 降为附录。
→ **标准尺 = phase-lead（谐波回归相位超前/预判）**：感知系统应**提前**于环境周期行动(lead>+6s=预判)，纯机械只滞后反应(lead<0)或同相(≈0)。脚本 `phase_harmonic.py`（核心：fire_lead_vs_coldtrough、speed_lead_vs_light）+ `rhythm_agg.ps1`（主角 trace 逐 tick→逐 step 均值，列 time_of_day/light_level/mean_speed/mean_fire/mean_temp）。day_length=200s、season=150s。

**关键路径（rounds 2-6）**：
- round 2-3：单 seed "ON +74% fitness" 是 auto 非确定配置 + 单 seed 噪声的假阳性；6-run 确定性矩阵(3 seed×ON/OFF，adt=1+pool 可复现)推翻它——dmg0.25/cold30 下 env-perc 未被因果选择。
- round 4：把 dmg 0.25→0.5 = **过压**（C7 全灭绝，ON/OFF 一起死），排除"压力不足"。corr(光,速度)≈0.3~0.4 在 gen0/OFF 都有 = 纯机械耦合(亮度直接影响移动物理)，非感知。
- round 5 诊断：cold_temp_threshold=30 下"冷"是**恒定全局耗血**(core_temp 八成时间低于阈值，无回暖喘息窗)，降 dmg 无用。
- round 5 Step1（**关键修复**）：`cold_temp_threshold 30→22` + `thermal_damage_per_second 0.5→0.3` → 冷只在最冷时段触发 = **昼夜/季节相位锁定、空间可规避的低谷**（营火 warmth_radius=14、fire_warming +1.0°C/s、平衡点 ambient+20，暖源净正本就成立 → Step2/3 未触发）。**C7 复活**(每代存活 24-46/48)，仍每代死 5-24 只 = 真实冷致死梯度(有选择压、非 trivially 全活)，harmony 7/7。
- round 6（**首个诚实阳性 ⭐**，run `data/runs/realism_detP2b_th22_*`，6-run th22/dmg0.3 80 代全 7/7）：phase-lead 尺(仅主角，gen79 vs gen0)——**趋火通道 ON 在 core_temp 最低点之前 +11~12s 达峰，3 seed 一致(fire_phi~0.95×3)；OFF 反应式(到最冷才趋火，+4s)；gen0 无节律**。= ON 专属、随进化涌现、跨 seed 可复现的"感知冷→提前趋火"预判节律。**但很弱**：趋火谐波 R²≤0.08、振幅≈0.005；活动量(speed↔light)通道仍纯机械(ON=OFF)。aggs 在 `tmp/trace_export/agg_b_*.csv`(已 SCP 到分析机)。

**未决诊断 = 下次第一步（decision_round_6 候选 3，round 7 进行中）**：
- 把 6-run 矩阵**延长选择到 ~160 代**(配置 `tmp/layer1/detP2c_th22_{seed}_{on,off}.toml`，generations=160、tail_generations=20 抓 gen140-159、max_episodes=1，输出 `realism_detP2c_th22_*`)，看趋火 R²/振幅是否随更长选择**继续上升**：
  - **涨(R² gen79→~150 升 >0.03)→ 编码够用，继续选择即可，不必开新血脉**。
  - **plateau(平在 ~0.08)→ 坐实编码=天花板 → 才开新血脉**上 candidate 1+2。
- 分析法：复用 detP2b 的 gen0/gen79 agg 作曲线起点 + 跑 `build_aggs_p2c.ps1`(已备在 tmp/) 出 gen140/145/150/155/159 的 agg → `phase_harmonic.py` 看 fire_lead/R² 曲线。
- **工程坑（已踩）**：① 引擎 full-resume 校验要求背景物种数=配置初始值，而背景 NEAT 进化后数量变了(pack_hunter→450 等)→ **checkpoint 不能被引擎续跑**；但 adt=1 确定性 = 从 gen0 等长重跑会逐位复现 gen0-79 再延伸，科学等价(只多花重算时间)。② Windows OpenSSH 会在启动脚本返回时杀掉会话子进程树 → **长 run 必须用 WMI Win32_Process.Create 启动**(见 §4 line 89)，`Start-Process`/`&` 会随 SSH 死。

**新血脉编码 spec（已预设计、按决策"先不提交"暂存在分析机 `/home/ubuntu/pe_scripts/new_bloodline_encoding_spec_DRAFT.md`，仅 plateau 才激活）**：
- candidate 1：显式昼夜/季节相位输入 + 体温趋势/预测项(d(core_temp)/dt)。
- candidate 2：goal 词表加"趋暖/趋火"目标通道。
- **改输入维=新血脉**：需显式标注 + 先 smoke(8-12 代)验 C7+二进制稳定，再上矩阵。

### 可视化最终形态（2026-05-30 用户拍板，替掉旧"Godot 精细 3D"）
用户原话"地图可以是 3D，物种用不同颜色的点，投掷物用更小的点"。理由：几百实体精细 3D 会卡死（隔壁 deep_protagonist 单主角才适合精细 3D）。
- 地形/地图：3D（起伏+季节天气视觉）；物种：每 archetype 一色小圆点；投掷物：更小的点。
- 对观察涌现行为（围猎/食物网/聚散）比精细 3D 更清楚。等拟真推进到合适节点再做这个 dot-based viewer。

---

## §3 项目身份 & 已建成

- **Neural Eco / protagonist_ecology**：C++ 大一课设。24 个 protagonist 在 600×600 拟真生态里和 8+ 物种共生，靠 NEAT+DNC+CTRNN+HRL+MAP-Elites+HER+Lamarckian 进化出建造/繁殖/合作/抗灾。**不引 LLM**。
- 二进制：`D:\claude-code\c++\build_ninja_cuda\bin\neural-eco-protagonist.exe`（CUDA build；被清需 cmake configure + ninja build 重建，git fetchcontent 卡墙用 Clash proxy `127.0.0.1:7897`）。
- **已建成层**：L1 体温+600×600 / L2 6天气 / L3 9物种 / L4 生命周期+繁殖+谱系 / L5 5类建筑 / L6 三层世界（**MainWorldHost：World2D 跨 80 代 800 tick 永不 reset**，日志 `cumulative sim_time=800.000s tick_calls=800` 可验）+ stage4b live agent seeding + stage4c snapshot / L7 python viewer + trace / L8 Lamarckian。**test_core 227 PASS**。
- **trajectory 闭环**：3/3 干净同 seed run gen79 全 PURPOSEFUL（不是无头苍蝇）；dominant_goal_frac↑、predator_push↑。
- alive @gen79 ~220（跨 seed stddev=6 极稳）；亡率 ~32% = LifecycleLayer 老化+生态动力学，非 bug。

### 目录布局（相对 `routes/protagonist_ecology/`）
`brain/`(DNC+NeatBrain+GPU) `capability/`(Chop/Craft/Hunt/Throw/Thermal) `perception/`(15个感知) `runtime/`(Bootstrap/Evolution/MainWorldHost/TraceRecorder) `world/`(16 layer) `viewer/`(python+godot) `docx/`(PROGRESS/STATUS/FINAL_BLUEPRINT/REALISM_ROADMAP/PHASE_B_2C_VERDICT)

---

## §4 跑训练 / 测试速查

- **后台启动（唯一可靠）**：WMI `Invoke-CimMethod -ClassName Win32_Process -MethodName Create -Arguments @{CommandLine='cmd.exe /c D:\path\run.bat'}`——脱离 SSH 会话，SSH 断了不死。**不要用 `&` / `Start-Job` / `Start-Process`**（随 SSH 死）。batch 里 redirect 到 log 文件。
- **harmony**：`python scripts/protagonist_harmony_check.py <run_dir>`（7 项 C1-C7：捕食者活跃/食草减员/协作/腐食/工地/me_cov≥0.30/不灭绝）。
- **trajectory**：`python scripts/protagonist_trajectory_analyze.py <tracedir> --brained-only` + `_compare.py`。
- **Test**：`cmake --build build_ninja_cuda --target test_core` + `test_core.exe`。
- **Re-build / 重跑前必**：`taskkill /F /IM neural-eco-protagonist.exe`。
- **polling**：用 shell `sleep 2400`（40 min）不用 wait 工具（省成本，quiet mode CPU 降频）。等日志出 `gen 79 advanced 10 ticks ... sim_time=800.000s`。
- **磁盘纪律**：分析完**立刻删 trace 目录**（只留 .md/.json 报告 + checkpoint + CSV）。一个 80-gen MWH-on run trace ~1.7-2 GB。

### MainWorldHost 开启的 toml 必填行
```toml
[runtime.triple_world]
main_world_continuous_tick_enabled = true
main_world_tick_dt_seconds = 1.0
main_world_ticks_per_generation = 10
```

### Checkpoint / 接力
- 每 10 代自动存 checkpoint（NEAT 基因组+进化状态+RNG+代数）。`runtime.resume_checkpoint_path` 指向上一跑 checkpoint = 真热启动续跑。
- **但拟真改架构期间不接力**：改输入维后旧 checkpoint 被 shape guard 拒，每次重开新血脉。等架构定型再开接力链。
- checkpoint **不含世界实时状态**（位置/工地/树）= "脑子接力、世界重开"；连续世界史是 Phase F。

---

## §5 红线（绝对不做）

- 动 `routes/deep_protagonist/` / commit 不加 `protagonist_ecology:` 前缀 / 引 LLM / 删改 17 不变量
- 训练沙盒外 reset / 写答辩剧本式 demo
- 半夜弹问题窗 / 长 run 完停下等用户才接下一跑（违反铁律 6）
- 指标低 → 调小参 → 跑 → 重复（Goodhart）/ 为指标加直接奖励
- 直接信文档不验代码 / 不更 PROGRESS+STATUS+note 就结束会话
- 留大 trace 文件不删

---

## §6 SSH 连用户机器

详见独立 note `用户本地 Windows 电脑 SSH 远程访问 (sans)`。要点：
- 私钥 secret `DEVIN_SSH_KEY_SANS`（缺 PEM 头尾，需脚本重建）。chisel client 走小米球 `t1dwvu6bzdng.vip3.xiaomiqiu123.top` → 本地 2222 → Win 22。**隧道地址固定不变**（小米球付费版，重启不换）。
- 远端默认 shell 是 **PowerShell**：命令分隔用 `;` 不用 `&&`/`||`；路径正反斜杠都行；后台用 WMI。
- chisel server 在 Win 侧 `D:\tools\chisel\chisel.exe server --port 3303`；Clash 在 `127.0.0.1:7897`。

---

## §7 接到新任务第一件事

1. SSH 上去 → `git log --oneline -8 -- routes/protagonist_ecology/` 对比本 note 状态；不一致先 `devin_knowledge_manage action=update` 同步（不重复 create）。
2. 读 `docx/PROGRESS.md` + `STATUS.md` + `REALISM_ROADMAP.md` 头部。
3. 跑前必 `taskkill /F /IM neural-eco-protagonist.exe`。
4. 用户没说停 → 按 §2 路线图自己拍下一步（当前 = Phase B env-perc ON/OFF 对照），按铁律 6 一直跑，不 block。

> 本 note 是会话间记忆。重大发现/决策/失败教训 → `devin_knowledge_manage action=update`（先 `action=list search=protagonist` 拿真实 note_id）。这是压缩抗性的唯一保障。

---

## §8 提速调查（2026-05-31）— 诊断完成，根因=单线程，并行化留给下个号

用户要求："保持训练流程/数据流/结果不变，只加速。"（GPU 40-50%、CPU ~60% 没吃满）预算够重构、不够长跑，只跑短 smoke。

**安全杠杆全无效（同 seed 50001，8 代实测，铁律 3 以实测为准）：**
- batched_dense（`protagonist_batched_dense_enabled=true`）：慢 ~40% + 结果跑偏（gen0 best 8764 vs 基线 2907）→ 否决。文档/69ea2fa 说 −15% 不复现。
- dense_gpu on/off：脑子前向 ~22-24s/代，速度相同。
- 重编 `/arch:AVX2`（scoped）：仍无效；AVX2+CPU 也无效。CMake 改动已回退。

**根因（cpu_sample.ps1 实测）：脑子前向阶段进程只用 1.09/32 核 ≈ 单线程。** 24 主角 per-agent 前向串行，`agent_decision_threads=15` 未真正并行。脑子时间对 GPU/CPU/AVX2 全不敏感——瓶颈是缺并行不是算力。

**确定性：** 管线 run-to-run 非确定（同 seed 两跑 gen0 即微差，几代后混沌放大）→"结果不变"= 统计一致 + harmony 7/7，非逐位相同。

**真正的杠杆（下个号/有预算时做）：** 并行化 24 个独立 per-agent 前向（读全部观测→各自算→统一写回，数据流不变、数学等价、非乱并发）。在**共享引擎 AgentLayer 调度层**（不在本 route，与 deep_protagonist 共用），需改代码+重编+重验 harmony。预期：单线程→多核可达接近核数倍加速，是真正能上"7 倍"量级的刀。**红线：不动数据流、不搞无意义并发、改完只 smoke 验证 harmony 7/7。**

---

## §9 自主开发总路线（指针）— 完整版见 AUTONOMOUS_PLAYBOOK.md

训练提速后迭代会变快，用户不在时**自己一阶一阶推进、遇问题按 playbook 决策树自决、绝不停下等用户**。完整路线 + 每阶段成功判据 + "表现好→下一步" + 出问题决策树，见 **`AUTONOMOUS_PLAYBOOK.md`**（桌面 `C:\Users\sans\Desktop\agent\` + repo `routes/protagonist_ecology/docx/`）。

**当前位置**：引擎中后期（生产级）；拟真 **Phase B 节律拟真**（判据已切 phase-lead 预判；可存活压力档 = cold_temp_threshold=22 + thermal_damage_per_second=0.3，C7 复活 harmony 7/7）拿到首个诚实阳性(ON 趋火预判 +11~12s，弱 R²≤0.08)；**未决=gen→160 rise/plateau 诊断(round 7 进行中)**。提速：单 run 受 adt=1 锁(in-world 多线程非数学等价、污染 A/B)，已用**实验级并发**(6 独立进程 ~10-11 核、单 run 5.5×)吃满 CPU。

**近期未完成项（开局就接，按优先级）**：
- **P0（下次第一步）gen→160 rise/plateau 诊断**：detP2c 6-run(已配置/已起跑，若仍在跑直接读进度；checkpoint 不可续但 adt=1 可从 gen0 等长重跑复现)。跑到 gen159 → `build_aggs_p2c.ps1` → `phase_harmonic.py` 看趋火 R² 曲线 gen79→~150：涨=继续选择、平=开新血脉。
- **P1（仅 plateau 才做）新血脉 candidate 1+2**：显式昼夜相位输入+体温趋势项 / goal 加趋暖通道。改输入维=新血脉、smoke 先行。
- **P2 Phase C 局部感知**：真视野+遮挡+噪声（也改输入维=新血脉）。
- 提速 P0(per-agent 前向多核)已用实验级并发替代解决，单 run 数学等价并行留作引擎层长期项(低优先)。

---

## §10 SESSION UPDATE 2026-06-01 (executor, rounds 7→13) — 接力必读，覆盖 §9 旧「当前位置」

**这一棒把 detP2c plateau 诊断 + 两条新血脉 + 环境压力 pivot 全部跑完，结论收敛。**

### 已闭环结论（全部 OFF/ON 3-seed × gen79 谐波，本地 commit 未 push）
- **round_9 detP2c gen→160 = PLATEAU**：趋火预判 gen79→150 没增长，OFF 对照末段反超 ON → 10 维环境感知编码到顶。
- **round_10 candidate_1（昼夜相位 sin/cos + 季节 + d(core_temp)/dt，+5 维）= NEGATIVE**：lead 从 +12s 塌到 ~0，振幅没变。富感知不破平台。
- **round_11 candidate_2（稳态暖意驱动 warmth_need 前瞻，+2 维）= NEGATIVE → 硬止损**：ON 0/3 LEAD < OFF 2/3。两条血脉都不行 → 瓶颈=选择压力不足（Ouyang 1998：节律只在环境够苛刻才被选）。
- **round_12 env pivot cold_temp_threshold 22→25（用户批准选项1）**：①warmth-drive 两环境皆 ON<OFF → **决策者令彻底弃用 candidate_2**（flag 留休眠，后续只跑 OFF）。②env 压力对基线 PARTIAL+：趋火预判从 2/3→**3/3 LEAD**、mean lead +7.3→+9.7s（首个诚实阳性方向：选择压力才是杠杆）；**但振幅/R² 没破平台**。
- **round_13 env step2 cold 25→27（OFF-only，自决策续跑，决策者已被用户叫停）**：R² 0.103(<0.13)、amp ~0.005(<0.009) → **平台未破**。跨冷轴 22/25/27 振幅恒~0.005、无单调增强 → **振幅在 cold 轴上场景封顶**；cold 只组织**相位**（各档都 2-3/3 LEAD，cold25 最干净 3/3），不抬**强度**。C7 全程不灭绝。

### 当前位置（覆盖 §9）
拟真 Phase B 节律：**预判相位-lead 已达标**（cold25 = 3/3 LEAD，+8~12s，robust）；**谐波振幅/R² 在 cold 轴场景封顶**，靠加深夜寒抬不动。candidate_1/2 均阴性已弃。

### 下一棒 P0（decision_round_12 §3 的最后一个正交测试，已配好思路）
**只动存活窗旋钮、不动梯度**：从 cold25 基线，`thermal_damage_per_second` 0.3→0.45(再 0.6)，测「惩罚陡度」能否抬振幅（区别于「梯度深度」）。一次一个旋钮、smoke 先验 C7（苛刻惩罚可能压垮生态→灭绝就回退幅度）。
- 草案：copy `tmp/layer1/harshcold27_long_50001_off.toml` → cold_temp_threshold 改回 25.0、thermal_damage_per_second 0.3→0.45、generations 12、runs_dir `data/runs/thermdmgD5_smoke_50001_off`；过 C7 再上 long 80gen×3seed OFF → gen79 谐波。
- 判据：振幅实质破平台(amp>~0.009 且 R²>~0.13)且 C7 守住 → 振幅可被惩罚陡度抬。否则 → **振幅确认场景封顶**。

### §4 阶段切换 ping 点（决策者职责，不是执行者自决）
若 thermal_damage 正交测试**也**抬不动振幅 → 判定振幅场景封顶、而**预判相位-lead 已达标(3/3 LEAD)** = Phase B 节律预判按 lead 口径**成功**。此时=**阶段切换**（关「日趋火预判」、转 Phase B 其它拟真轴：居所/社交/建造节律）→ **决策者 ping 用户**定方向（接受 lead 达标收口 vs 继续别的轴）。本棒未跑该正交测试（用户令收尾、余额 $13）。

### 红线（本棒全程遵守，继续守）
单旋钮/一次一个、smoke 先验 C7、改输入维=新血脉、不为指标发奖(env 压力≠reward)、生产 exe + deep_protagonist 不碰、commit 仅本地前缀 `protagonist_ecology:` 不 push、D free ≥200GB（trace 取完即删）。exe 用 `neural-eco-protagonist-rhythm.exe`。


## §11 SESSION UPDATE 2026-06-01 晚 (executor, thermal_damage 正交测试 + 一锅炖 pivot + GPU 重构) — 接力必读，覆盖 §10「下一棒 P0」

**这一棒：单轴打法穷尽 → 决策者拍板转「一锅炖」整合血脉 co-tune；落地统一编码+FOV退火器+Phase C；并隔离做 GPU 推理架构重构。**

### 1) thermal_damage 0.45 正交测试 = CAPPED（封顶，闭环，commit ce7db1e）
按 §10 P0 跑 `thermal_damage_per_second 0.3→0.45` 80gen×3seed OFF vs detP2c-0.3 基线：
- 50001: amp0.149 / R²0.134 / lead−29s = PASS（离群）
- 50002: amp0.018 / R²0.062 / lead−17s = FAIL
- 50003: amp0.017 / R²0.059 / lead−31s = FAIL
裁定：振幅平台**未被稳健打破**（2/3 FAIL，50001 离群）；3-seed 一致 **lead 全负(−17~−31s)→预判被摧毁，退化成"冷到了才烧"的被动反应**。叠加 round_13 冷阈值轴=PLATEAU → **两次单轴隔离全失败 → 单轴打法穷尽**。坐实"调一个坏一个"=所谓正交轴其实耦合（冷强度↔预判性）。

### 2) 决策者拍板：弃单轴、转一锅炖 co-tune（用户令，A-F 全机制一次性整合再统一调参）
根因综述（pe_analysis/PE_CEILING_ROOTCAUSE_AND_REDESIGN.md）：天花板不是"机制不够"，是**世界让被动反应太划算**（整夜不动只掉~30HP，死不了→预判无边际收益）。文献(Paranjpe&Sharma 2005 / Ouyang 1998 蓝藻竞争)：内在钟(预判)只在三条件同开才胜过反应——①威胁可预测②滞后惩罚陡③预备动作有延迟。现 PE 只满足①。**重排优先级**(按"是否直接造前瞻收益"，非等权堆)：P0 = C有限视野(逼记忆+钟) + A尺度(路程延迟=反应来不及) + 调慢恢复+觅食机会成本(不能贴火camp)；P1 = E预备性建造(延迟)；P2 = D亲缘/F持久。数学：不发直接趋火奖(Goodhart)，把世界工程化成"反应必死"，则 argmax(存活)本身=预判，势函数 shaping(Ng 1999, policy-invariant)只缓解信用分配。

### 3) 统一血脉编码 + FOV 退火器（已落地 commit 63f3144 / 90f2688）
- **不可逆决定**：input_dim 第一次 build 就锁满=满配维度，flag 关时写零（背书：TaskContextPerception 现有"reserve 8 onehot, 空槽=0"惯例）。新增 **+11 维**（朝向+2 真信号 / 亲缘+3、领地+4、世界史+2 先零占位）。主开关 `protagonist_unified_encoding_enabled`（默认 false，不动旧 checkpoint）。以后 C→D→E→F 逐个开=让零槽有信号，**不改维=不废血脉**（彻底消灭多次暖启动手术）。
- **FOV 退火器**（用户令"直接写好，哪怕不用"）：config `fov_anneal_start_gen/end_gen + start/end_half_angle_deg`；运行时每代把"当前代数"盖章进 config（ProtagonistEcologyRuntime 进化主循环），`computeFovParams()` 线性插值算当代视锥角。默认 gen0=180°→gen45=70°；`start==end` 退化成固定角（不删代码）。通用：以后 thermal/尺度/遮挡都能挂同一条"难度=f(代数)"调度。
- **Phase C**（零新代码遮挡）：复用 TerrainLayer::isVisible + SelfHeadingPerception(+2 朝向) + 视锥角门控 DangerPerception（看不见火→方位/距离写零、但烤到的暖意/危险区保留）→ 逼记忆+预判。
- **Step0+C smoke = PASS**（12代不灭绝、主角29/48存活、DNC记忆活跃）。**Step0+C 80代长跑进行中**（PID 39264，config `routes/protagonist_ecology/onepot_sC_long_50001.toml`，输出 `data/runs/onepot_sC_long_50001`；FOV gen45 后收到 70°，测 phase-lead 能否从被动反应转回预判；尾段 gen60-79 出谐波裁定）。截至收尾 ~gen13/80。

### 4) GPU 推理架构重构（用户专门点的活，隔离副本，commit 4a27000）
当年因"收益没到点"押后，一锅炖后 justified。隔离副本 `routes/protagonist_ecology_gpuarch`（与在跑的 PE/DP 完全隔离，正在跑的源码+build_ninja_cuda 零改动）：
- **cohort-resident 权重**：genome 权重在个体一生中不变 → 每 cohort(换代/种群churn)只 `uploadCohort()` 上传一次，之后每 tick `forwardResident()` 只过会变的小张量(输入/可塑状态/alpha)。原架构每 tick 重传 ~14MB 全权重 = eco 82% 卡 CPU、GPU 只占 2G 的根因。
- runner 用便宜 cohort 指纹（每 agent 权重指针+采样权重位+维度 FNV-1a，O(batch) 非每tick全权重hash）判同 cohort；同→快路径跳 marshal+上传，变→重传一次；GPU 侧带驻留守卫(维度/批次不匹配自动拒绝→回退重传)。
- **CPU-path parity = PASS**：max|Δ|<1e-4 三路一致(resident/legacy forward/CPU)；H2D 200tick 399MB→12.8MB（**−96.8% / 31×**），生产权重尺寸趋近 ~99%。测试 `routes/protagonist_ecology_gpuarch/runtime/test_resident_parity.cpp`，文档 `GPU_RESIDENCY_REFACTOR.md`。
- **仅剩**：全 CUDA build + GPU 数值 parity（需 GPU 空闲，PE 长跑占用中，按"不碰在跑的"原则未做）。**未合回主线 PE**，待 GPU parity PASS + 用户 sign-off 再 merge。

### 当前位置（覆盖 §10 P0）
单轴穷尽 → 一锅炖整合血脉已落地、Step0+C 长跑在测 phase-lead。GPU 重构隔离完成+CPU验证，待 GPU parity。后续：D/E/F 给真逻辑 + A 尺度 + co-tune 势函数(PBRS，绝不发直接奖)。红线照旧：smoke 先验 C7、改输入维=新血脉(现已锁满)、生产 exe 用 `neural-eco-protagonist-rhythm.exe`、commit 仅本地不 push、D free ≥200GB。

---

## §11.FINAL — ONE-POT Step0+C 80代长跑裁定（2026-06-01 收尾，1-seed 初判）

**实验**：onepot_sC_long_50001（seed 50001，80 代，统一血脉编码 + Phase C FOV 门控 + FOV 退火 gen0=180°→gen45=70°，gen45 后满压 70° 视锥）。本棒运行至 **gen79 DONE**。尾段 gen60-79 取 gen 60/65/70/75/79 五点做 fire-vs-coldtrough 谐波回归（harmonic_fit）。

**结果（趋火 vs 冷谷 谐波）**：

| gen | 趋火 amp | R² | fire_lead_vs_coldtrough | 判 |
|---|---|---|---|---|
| 60 | 0.007 | 0.07 | **+17s** | LEAD(预判) |
| 65 | 0.007 | 0.10 | **+5s** | in-phase |
| 70 | 0.006 | 0.05 | **+8s** | LEAD |
| 75 | 0.008 | 0.09 | **+8s** | LEAD |
| 79 | 0.008 | 0.10 | **+5s** | in-phase |

对照锚：预注册闸 = amp≥0.009 **且** R²≥0.13；detP2c-0.3 OFF 基线 = amp~0.005/R²0.091/lead **+10s LEAD**；round_14 thermal0.45 = lead **−17..−31s（预判被摧毁）**。

**裁定（1 seed 初判，需 3-seed 复核）= 半破：相位维赢了、振幅维仍封顶。**
1. **★相位 lead 全程为正（+5..+17s，3/5 LEAD、2/5 in-phase、0/5 反应滞后）★** —— 与 round_14 thermal0.45 的全负（−17..−31s 被动反应）**正好相反**。这正坐实了 Phase C 的设计意图：**看不见的东西无法反应，只能靠记忆+预测 → 预判方向被结构性保住**。FOV 门控（逼记忆+钟）确实是"把预判焊死成存活最优解"那一味，没像加冷强度那样把 agent 逼成"贴火当温度计"的反应器。
2. **但振幅 amp(0.006-0.008)+R²(0.05-0.10) 仍低于预注册闸**（amp<0.009、R²<0.13）→ **振幅平台未被 C 单机制击穿**。amp 与 detP2c-0.3 基线(~0.005)同量级，没显著抬升。

**含义（直接喂给下一棒）**：C 单机制（A-F 里只点了 C）**保住了预判的"方向"，但给不出预判的"幅度"**。这恰恰验证一锅炖的根本论点——单一机制即便放进统一血脉框架里也不够；要把振幅顶上去，必须**联调激活 payoff 轴**：A 尺度放大（火离得远=路程延迟，反应来不及→必须提前大动作）+ E 预备性建造（提前搭暖窝要时间=延迟→提前启动有高回报）。C 负责"必须预判"，A/E 负责"预判值得用大动作"。下一棒 = 在同一血脉里开 A+E 的延迟/代价结构，co-tune 看 amp 能否在保住正 lead 的前提下突破 0.009/0.13。

**红线提醒**：本裁定 **仅 seed 50001 单 seed**（与之前 thermal0.45 那个"50001 振幅 30× 击穿"假象同样是单 seed —— 那个后来被 50002/50003 证伪为 CAPPED）。故本结论按"初判"对待，**正负 lead 这个定性结论比 amp 数值更可信**（相位 5/5 一致为正，稳健性高；amp 单 seed 易抖）。3-seed 复核前不下硬裁定。

---

## §12 SESSION UPDATE 2026-06-02 — 世界拟真补全(代码) + 训练全停 收尾（接力必读）

> 完整交接见仓库根 `PROJECT_STATUS.md` 与 `routes/protagonist_ecology/docx/realism/REALISM_DECISION.md`。本次 commit `4a69282`（仍仅本地，仓库无 origin）。

### 1) PE 世界机制抬到 DP 深度（5 项，已写 + 编译通过，**未重链 exe、未重训**）
扒 DP 源码后确认**拟真缺口不对称：DP 已较深，PE 是浅的那个**（DP 的火=主角放置+耗木+燃料烧尽自灭；DP 洞穴=狼感知降30%/追速降70%+体温趋稳+夜栖势能）。故本轮把 PE 抬到 DP 深度：
1. **火=资源**：点燃给燃料、每 tick 烧、风暴 3× 加速、烧完自动熄（`world/CampfireLayer`）。
2. **洞穴热缓冲**：洞内恒温 ~15℃、辐射散热降到 35%（`capability/ThermalTickCapability` 加洞穴绝缘项）。
3. **`IgniteFireCapability`（+1 动作 = 破契约）**：主角耗 1 柴在身边火堆点/续火；闪电自动点火率砍到 15% → 火基本由主角点，不再凭空冒（用户头号诉求"火是主角点的"）。
4. **`CavePerception`（+4 obs = 破契约）**：是否在洞内 / 最近洞口方向+距离（DP 早有 inside_cave obs，PE 此前缺）。
5. **洞穴防掠食**：进洞后对洞外掠食者隐身（复用小型猎物 `CaveHideCapability` 机制）。

→ 第 3、4 是**破契约**（改动作空间/obs 维度）：PE 旧 seed 权重作废，必须**起全新 seed 重进化**才能用上。

### 2) viewer 拟真（PE 已对齐；洞穴未导出）
PE viewer（`routes/protagonist_ecology/viewer/web3d`）：按生态磕巴地形 + 不规则水盆地（修了之前"水飘天上/穿模"）+ 实例化真草 + 真动物/树 + 火 + 建造分阶段动画。**洞穴渲染代码在（main.js L692–725，cell 化小灰岩聚簇 + 边缘暗黑洞口），但当前 dump 的 `viewer_world.json` 无 caves 字段 → PE 暂不显示洞穴**，需重 dump 带上 cave mask。DP viewer 移植洞穴时**就照这段 L692–725 搬**（用户认可这个观感；别再自造 dome）。

### 3) 训练全停 + 旧世界保活结论
PE 旧世界保活 seed 50012–50017 跑完（80 代旧配置/重复实验，best_fitness≈6.6k–8.7k、me_coverage≈0.32），**已连 run 目录一起清理**（释放 11.6GB → D: 213.8GB）。**当前无训练在跑**——世界未定稿前保活无意义（权重统一重训作废）。§11.FINAL 那个"半破(相位赢/振幅封顶)"裁定**仍是 1-seed 初判，未做 3-seed 复核**（被本轮"先补世界"决策搁置）。

### 4) 下一步 = 唯一一次统一重训（破契约批次）
训练已停 → 可干净重链 exe → 重 dump（带 caves + 建造进度）→ **PE 起 3 fresh seed**（含点火动作 B1 + 洞穴 obs B2 + 火燃料/洞穴绝缘/防掠食非破契约项）重新进化，验证：① "主角点火"行为是否涌现 ② 加了洞穴/点火 payoff 后"提前趋火"预判**振幅**能否突破闸(amp≥0.009/R²≥0.13)。调优守用户「撒米引诱小鸡」：把寒冷压力调到"反应式来不及必死"，逼出预判，堵掉"等最冷才趋火也能活"的空子。统计严谨红线不变：**3 seed 一致才算数**（单 seed 假阳性前科）。
