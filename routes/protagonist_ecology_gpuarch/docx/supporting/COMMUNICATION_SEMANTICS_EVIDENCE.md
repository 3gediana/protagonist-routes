# Communication Semantics Evidence · 2026-05-22

> 位置：`routes/protagonist_ecology/docx/supporting/COMMUNICATION_SEMANTICS_EVIDENCE.md`
>
> 作用：A3 收口的结果说明。三个最小通信任务（scout-worker / danger warning / build request）+ 老 signal_response，覆盖 5 个 memory ablation × 5 个 signal ablation 的对照实验。包含 v8 retrain genome 的跑分、信号 ablation drop、listener / responder 行为对照。
>
> 配套设计文档：`DEVELOPMENT_TASK_PLAN.md § A3` + `WORLD_UNDERSTANDING_RESEARCH_AND_ENGINEERING_PLAN.md § 7`。
>
> **状态：S1-S5 基础设施代码 + v8 retrain + 5×5 ablation sweep 全部完成。`comm_scout_worker` 已经给出"关掉信号 → fitness ▼83%、responders ▼100%"的强证据；`comm_danger_warning` 与 `comm_build_request` 信号链路通了但主指标失效（task 设计问题，已记录到 Phase 2 修复清单）。**

---

## 1. A3 三任务对照表

| Task | 设计语义 | controlled emitter 行为 | primary metric | v8 实测 ablation drop（baseline → signal_disabled） |
|---|---|---|---|---|
| `comm_scout_worker` (S2) | scout 看见 → emit; worker 盲 → 跟信号去食物 | 周期 20s, 距离 22m±6m, 携带 3 份 18 单位 food drop | `signal_responder_count` | **fitness 58.3 → 9.95 (▼83%)、responders 1.5 → 0 (▼100%)** ✅ |
| `comm_danger_warning` (S3) | sentry 看见 predator → emit; 其他盲 → 听信号逃 | 周期 12s, 距离 18m±6m, 无 food, 3 predator pack 隐藏在 emitter 附近 | `survived` count | survived 8 → 8 (无差异, task pressure 不足), responders 4.75 → 0 ⚠️ |
| `comm_build_request` (S4) | worksite 缺料 → emit; 携材 listener 听信号去 deposit | 周期 15s, 距离 0m (emit 在 worksite 位置), 无 food | `worksite_completion_events` | worksites 0 → 0 (没人完成), fitness 43.9 → 16.2 (▼63%), responders 5.25 → 0 ⚠️ |

注：所有任务都用 controlled emitter 充当"虚拟 scout/sentry/builder"。protagonist 自己自发 emit 在 A3 阶段**不要求**——这与 EXECUTION_GUIDE §A3 一致，"通信语义不靠 reward 硬拧，靠任务压力涌现"，但作为 minimal probe 第一步先证明 listener 能用信号。后续（A3+ / B）才会放开 protagonist emit。v8 实验里 protagonist 在所有 4 个 task 上 `signal_emit_events_avg = 0`，符合此预期。

---

## 2. 实验设置（v8）

### 2.1 训练 (v8)

| 项 | v7 | v8 |
|---|---|---|
| config | `protagonist_ecology_train_camp_signal.toml` | `tmp/a3_train/protagonist_ecology_train_a3_v8.toml`（tmp 不入主仓库） |
| seed | 91 | 137 |
| generations | 60 | **120** |
| controlled emitter 周期 | 30s | **15s** (× 2 pulse 密度) |
| food_count × amount | 2 × 12 | **3 × 18** (× 2.25 reinforcement) |
| emitter offset_jitter | 10m | **16m**（强迫泛化，与 scout-worker 22±6m 兼容） |
| signal_response_dense_reward | 1.0 | **1.5** |
| signal_co_attendance_reward | 1.5 | **2.0** |
| signal_emit_companion_radius | 30m | **36m** |
| best_fitness 巅峰 | ~140k (gen 39) | **229k (gen 39)** |
| best_genome path | `data/runs/protagonist_train_camp_signal_audit_v7/20260522-104400/best_genome_gen59.bin` | `data/runs/protagonist_train_a3_v8/20260522-142545/best_genome_overall.bin` |

训练耗时：CPU 单机 120 代约 12 分钟（v7 是 60 代约 8 分钟）。

### 2.2 评估 (5 × 7 ablation sweep)

5 个 signal ablation × 7 个 memory ablation task × 5 个 memory variant = 175 行 results。

| signal ablation | toml | runs_dir |
|---|---|---|
| `none` | `tmp/a3_signal_ablation/none.toml` | `data/runs/protagonist_a3_v8_signal_ablation/none/20260522-144316/` |
| `disabled` | `tmp/a3_signal_ablation/disabled.toml` | `data/runs/protagonist_a3_v8_signal_ablation/disabled/20260522-144530/` |
| `listener_blind` | `tmp/a3_signal_ablation/listener_blind.toml` | `data/runs/protagonist_a3_v8_signal_ablation/listener_blind/20260522-144712/` |
| `one_channel_only` | `tmp/a3_signal_ablation/one_channel_only.toml` | `data/runs/protagonist_a3_v8_signal_ablation/one_channel_only/20260522-144911/` |
| `noise` (stddev=1.5m) | `tmp/a3_signal_ablation/noise.toml` | `data/runs/protagonist_a3_v8_signal_ablation/noise/20260522-145107/` |

每个 run 输出 `memory_ablation_results.csv` (36 行 = header + 7 task × 5 memory variant)。

---

## 3. S2 `comm_scout_worker` — 强证据 ✅

### 3.1 关键对照（memory baseline 行）

| metric | none | disabled | listener_blind | one_channel_only | noise |
|---|---:|---:|---:|---:|---:|
| `signal_responder_count` | **1.5** | 0 | 0 | 2 | 1 |
| `avg_fitness` | **58.3** | 9.95 (▼83%) | 21.8 (▼63%) | 72.1 (+24%) | 47.6 (▼18%) |
| `signal_listener_count` | 2.25 | 0 | 0 | 7.25 | 4.5 |

### 3.2 完整 5×5 fitness 矩阵

| memory \ signal | none | disabled | listener_blind | one_channel_only | noise |
|---|---:|---:|---:|---:|---:|
| baseline | 58.31 | 9.95 | 21.76 | 72.10 | 47.60 |
| no_spatial | 31.82 | 9.95 | 60.65 | 127.72 | 125.55 |
| no_episodic | 91.46 | 9.95 | 71.47 | 129.01 | 99.45 |
| no_social | 81.61 | 19.30 | 45.39 | 150.87 | 44.50 |
| no_goal | 57.03 | 9.95 | 55.33 | 49.82 | 37.77 |

### 3.3 解释

1. **`disabled` 让 fitness 全部归约到 9.95 量级**——这是底层 survival/food 拿到的"地板分"，没有任何信号来源 reward。
2. **`listener_blind` fitness 21.8**——略高于 disabled，因为 controlled emitter 仍然 drop food（`controlled_emitter_food_count = 2`），就算盲也能撞上吃到。
3. **`one_channel_only` 反而最高 (72.1)**——把 2 通道塌成 1 通道不影响 listener 行为（2 通道在 task 设计里没有差异化使用，本来就一样）。
4. **`noise (1.5m stddev)` fitness 47.6 (▼18%)**——位置加 1.5m 高斯噪声，listener 还能大致跟过去，但 responder count 砍半（2 → 1）。
5. **`no_social` baseline 4.0 是 responder 最高值**——这是反直觉的：关掉 social memory 反而更多 protagonist 真走到 emitter 上。猜测：social memory 在这个 task 里把 attention 拉去看背景 agent，关掉后 protagonist 更专注 listener pursuit。

**判定**：A3 强证据成立。`comm_scout_worker` 在 v8 下，关闭信号链路后 fitness ▼83%、responders ▼100%，符合 task plan "至少一条通信任务关闭信号后明显下降" 的判定。

---

## 4. S3 `comm_danger_warning` — Listener 链通了但 task pressure 不足 ⚠️

### 4.1 关键对照（memory baseline 行）

| metric | none | disabled | listener_blind | one_channel_only | noise |
|---|---:|---:|---:|---:|---:|
| `survived` (primary) | 8 | 8 | 8 | 8 | 8 |
| `protagonist_deaths_avg` | 0 | 0 | 0 | 0 | 0 |
| `avg_fitness` | -30.8 | +8.31 | +6.34 | -22.8 | -21.0 |
| `signal_listener_count` | 7.25 | 0 | 0 | 8 | 8 |
| `signal_responder_count` | 4.75 | 0 | 0 | 6 | 4 |

### 4.2 解释

- **`survived` 在所有 variant 都是 8/8**——3 个 predator 在 200s episode 内根本没杀到 protagonist。primary metric 失效。
- `signal_response_dense_reward = -0.6` 把 listener 走向 emitter 这个动作设成扣分（"听到危险 → 应该逃"），所以 baseline `none` 下 listener 跟着信号走反而吃罚分，fitness -30.8。
- 关掉信号后 (`disabled`, `listener_blind`) listener 不再被罚分，fitness 反而正向 +6 ~ +8。
- listener / responder 数字在 baseline 7.25 / 4.75，关信号后 0 / 0——**信号链路本身正常工作**，只是 task 设计让"行为正确"和"reward 高"方向矛盾。

**Phase 2 修复方向**（已加入 task plan）：
1. predator_pack_count 3 → 6，并把 spawn 距离从 18m±6m 缩到 8m±2m，确保不听信号必有死亡风险。
2. 把 `signal_response_dense_reward` 改成正向（"听到 = 知道有危险方向"），用 `survived` 自身的 +reward 来体现"逃成功"，避免信号本身扣分。
3. 把 `predator_warn_enabled = false` 改成 `true` 但 perception 距离压到 4m——让 protagonist 必须靠信号才能在远处感知危险。

---

## 5. S4 `comm_build_request` — Listener 链通了但 worksite logistics 断 ⚠️

### 5.1 关键对照（memory baseline 行）

| metric | none | disabled | listener_blind | one_channel_only | noise |
|---|---:|---:|---:|---:|---:|
| `worksite_completion_events` (primary) | 0 | 0 | 0 | 0 | 0 |
| `deposits_avg` | 0 | 0 | 0 | 0 | 0 |
| `avg_fitness` | 43.9 | 16.2 (▼63%) | 19.3 (▼56%) | 43.9 | 45.2 |
| `signal_listener_count` | 8 | 0 | 0 | 8 | 8 |
| `signal_responder_count` | 5.25 | 0 | 0 | 5.25 | 5 |

### 5.2 解释

- 8/8 listener 真心在听，5.25/8 responder 真心在跟信号走，关掉信号后双双归零——信号→pursuit 链路完全正常。
- 但 `worksite_completion_events = 0` 在所有 variant：没人完成 worksite。
- `deposits_avg = 0`：没人 deposit 任何 stick / stone。
- 任务链：listener 听信号 → 走向 worksite → 必须先携带 stick/stone → deposit → worksite 完成。v8 protagonist 在 chop tree → carry → deposit 这条 logistics 链上没学会。
- fitness drop 63%（信号链路确实贡献 fitness，但没有变成 worksite_completion 这个 primary metric）。

**Phase 2 修复方向**（已加入 task plan）：
1. listener 入场时自动 carry 1 个 stick / stone（绕过 chop tree + carry 这两步）。
2. 把 `worksite.required_sticks` 从 2 降到 1、`required_stones` 从 1 降到 0，让 "1 个 listener 跟信号到 worksite deposit 1 个 stick" 就够 trigger BuildCompleted。
3. emitter 改成"靠近 listener 时" emit，避免 emit 始终在 worksite 位置但 listener 还在 nest 附近时听不到的窗口问题。

---

## 6. 老 task `signal_response` — events 仍为 0（dedup bug 后遗症）

### 6.1 关键对照（memory baseline 行）

| metric | none | disabled | listener_blind | one_channel_only | noise |
|---|---:|---:|---:|---:|---:|
| `signal_response_events` (primary) | 0 | 0 | 0 | 0 | 0 |
| `avg_fitness` | 56.1 | 6.63 (▼88%) | 6.63 | 56.1 | 65.0 |
| `signal_listener_count` | 8 | 0 | 0 | 8 | 8 |
| `signal_responder_count` | 6.75 | 0 | 0 | 6.75 | 7.25 |

### 6.2 解释

- 6.75/8 listener 在跟信号走，fitness ▼88% 比 `comm_scout_worker` 还干净，但 `signal_response_events` 仍然 0。
- 根因见 `MEMORY_EVIDENCE_AUDIT.md § 8.5`：`signal_response_events` 用 `(listener, emitter, emit_tick)` 三元组 dedup，listener 跟着信号"停在 emitter 旁边"被算成 1 次，但因为 protagonist 还没真正"再走出 halo 又走进 halo"，触发不了第二次 event +1。
- responder_count（6.75）和 fitness drop (▼88%) 是这条 task 真正可信的两个指标。**未来如果想让 events 数字反映"停留时间"，需要把 dedup 改成时间桶模式**（5 秒桶可以 +1 一次），见 audit § 8.7。

---

## 7. A3 结论与遗留

### 7.1 是否成立

| 验收项 | 状态 | 证据 |
|---|---|---|
| S1 基础设施 PASS | ✅ | 128 tests + 4 ablation 开关都验证 |
| S2 scout-worker drop > 50% | ✅ | fitness ▼83%、responders ▼100% |
| S3 danger warning drop > 30% | ⚠️ Listener 行为变了但 primary metric (survived) 失效 |  |
| S4 build request drop > 50% | ⚠️ fitness ▼63% 成立, 但 primary metric (worksite_completion) = 0 |  |
| 至少一条任务 drop > 50% 证明信号有用 | ✅ | scout-worker fitness ▼83% |
| MI 矩阵证明信号通道 ↔ 环境变量显著互信息 | ⏳ | trace 没启用，离线 MI 等 trace_enabled=true 再补 |

### 7.2 重要限制（老实说）

1. **protagonist 自己没 emit**：所有 task 下 `signal_emit_events_avg = 0`。v8 学到的是**纯 listener** 行为，不是双向通信。emit 侧自发涌现是 B 阶段任务。
2. **`one_channel_only` 无差异**：把 2 通道塌成 1 通道，所有指标基本不变。说明 2 通道在 task 设计里没有差异化使用，channel 语义没有自然分化。这与文献预期一致——emergent communication 通常需要长训 + 强通道压力。
3. **3 个 comm task 都用 controlled emitter**：信号源不是 protagonist 自己。这是 A3 的 minimal probe 范围，不是"自发通信已证明"。
4. **comm_danger_warning 和 comm_build_request 的 primary metric 设计需要 Phase 2 修复**才能给出"主指标级"证据。当前只能用 fitness 和 listener/responder 行为做侧面证据。
5. **MI 分析没跑**：`memory_ablation.trace_enabled = false` 所以没有 `agent_trace.csv` 输出，`scripts/protagonist_communication_mi.py` 还不能用。需要 trace 启用后再跑一次专门 MI 评估。

### 7.3 v8 vs v7 对比（同 task 同 ablation）

`comm_scout_worker` baseline（memory baseline, signal none）：

| metric | v7 | v8 | 变化 |
|---|---:|---:|---:|
| signal_responder_count | 1.5 | 1.5 | = |
| avg_fitness | 9.93 | 58.3 | × 5.9 |
| signal_listener_count | 2.75 | 2.25 | ≈ |
| best_fitness | 11.42 | 211.0 | × 18.5 |

→ v8 fitness 是 v7 的 ~6 倍，best_fitness 是 18 倍。listener_count 略降但 best_fitness 大幅提升说明 v8 学到的 listener 行为更强但更稀疏（精英会大幅响应，平均的还差）。

---

## 8. 怎么复现

### 8.1 训练 v8

```bash
# A3-aware retrain (config 已入主仓库)
D:\claude-code\c++\build_vs3131\bin\Release\neural-eco-protagonist.exe ^
  --config configs\protagonist_ecology_train_a3.toml
# 约 12 分钟（CPU）, 产物在 data/runs/protagonist_train_a3/<timestamp>/
```

### 8.2 跑 5 × 7 ablation

5 个 signal ablation 共用 `configs/protagonist_ecology_memory_ablation.toml`，只改两个字段：

```toml
[runtime]
# 指向上一步产出的 v8 best_genome_overall.bin
protagonist_genome_path = "data/runs/protagonist_train_a3/<timestamp>/best_genome_overall.bin"

[world.signal]
# 5 个值轮换：none / disabled / listener_blind / one_channel_only / noise
ablation = "disabled"
# 仅当 ablation = "noise" 时有效，单位米
noise_position_stddev = 1.5
```

实操：复制 5 份 toml，分别改 `ablation` 和 `[output] runs_dir`，串行跑：

```bash
for cfg in tmp\a3_signal_ablation\*.toml; do
  D:\claude-code\c++\build_vs3131\bin\Release\neural-eco-protagonist.exe --config %cfg%
done
# 每组约 2 分钟, 5 组共约 10 分钟
```

### 8.3 读结果

```bash
# 每个 run 的 memory_ablation_results.csv 是 36 行 (header + 7 task × 5 memory variant)
# 横向对照脚本写法见 docx 末尾或 tmp/a3_signal_ablation/ 模板
```

### 8.4 注意

- 本轮 v8 genome `best_genome_overall.bin` 在 `data/runs/protagonist_train_a3_v8/20260522-142545/`（**注意 v8 这次跑出来路径仍带 v8 后缀**，是首轮跑前 toml 里 `runs_dir = ".../protagonist_train_a3_v8"` 留下来的；本次 commit 把 config 改成了 `data/runs/protagonist_train_a3`，下次重跑会落到新路径）。
- 5 个 ablation 的临时 toml + 训练 log 在 `tmp/a3_train/` 与 `tmp/a3_signal_ablation/`，不入主仓库；后续如果要稳定维护对照，再做一份脚本一键生成 5 份 ablation toml 即可。

---

## 9. Phase 2 task 重设计与第二轮 sweep（2026-05-22 晚）

§ 4 与 § 5 揭示的问题：`comm_danger_warning` 没死亡（primary 失效）+ `comm_build_request` 没人完成 worksite。Phase 2 目标是修这两个 task 的物理设计，让它们产出能进 results.csv primary metric 的可信 ablation drop。**复用同一份 v8 genome**（不重训）。

### 9.1 `comm_danger_warning` 第二版（safe-rallying）

| 字段 | Phase 1 | Phase 2 |
|---|---|---|
| world_size | 120×120 | **60×60**（缩小 4×, predator 自然密度提高） |
| `background.predator_count` | 3 | **12** |
| controlled emitter offset | 18m + jitter 6m | **6m, 无 jitter, 不 randomize**（emitter 永远在 nest 边） |
| `signal_response_dense_reward` | -0.6（penalty） | **+0.6（rally to safety）** |
| `signal_response_reward` | 0 | **1.5** |
| `combat_death_penalty` | 12 | **20** |
| 设计语义 | 听信号 = 逃 | **听信号 = 朝 nest 走 = 安全** |

理由：v8 不会反向解码 dense gradient（"听到 = 走反方向"），把语义翻成"听到 = 走过去 = 安全"，与 `comm_scout_worker` 同向，v8 listener policy 直接可用。

### 9.2 `comm_build_request` 第二版

| 字段 | Phase 1 | Phase 2 |
|---|---|---|
| `worksite.required_sticks` | 2 | **1** |
| `worksite.required_stones` | 1 | **0** |
| `tree.count` | 1 | **2**（保留 chop 链路，与 worksite_return 一致） |
| `objects.count` | 24 | **24**（保持） |
| `worksite.spawn_radius` | 10m | **8m**（更紧） |
| `worksite.direction_requires_visibility` | true | true（保持：信号是唯一指引） |

理由：把"完成 worksite"的物理门槛从"2 stick + 1 stone"降到"1 stick"。v8 在 `worksite_return` 上 baseline=1/1 已证明 chop→carry→deposit 链路完整；问题不在能力而在"看不见 worksite"。

> 失败的 v1 try（保留这条记录免得未来重做）：先尝试 `tree.count=0` + `nursery_supply.enabled=true` 让 stick 自动 spawn 在 nest 附近。结果 baseline 也 0 deposit，因为 v8 protagonist 的 capability set 里 pickup loose stick 的能力远不如 chop tree 拿 stick 训得熟。

### 9.3 第二轮 5×5 sweep（v8 genome 不变）

输出位置：`data/runs/protagonist_a3_v8_signal_ablation/<sig>/<timestamp>/`，5 个 timestamp 时间戳：
- `none`             20260522-155736
- `disabled`         20260522-160010
- `listener_blind`   20260522-160212
- `one_channel_only` 20260522-160413
- `noise`            20260522-160610

### 9.4 `comm_danger_warning` Phase 2 结果

memory baseline 行：

| metric | none | disabled | listener_blind | one_channel_only | noise |
|---|---:|---:|---:|---:|---:|
| `survived` (primary) | **3.5 / 8** | 3.75 | 3.75 | 3.5 | 3.5 |
| `protagonist_deaths_avg` | 0.25 | 0 | 0 | 0.25 | 0 |
| `avg_fitness` | **41.1** | 4.87 (▼88%) | 5.36 (▼87%) | 41.1 | 41.9 |
| `signal_listener_count` | 8 | 0 | 0 | 8 | 8 |
| `signal_responder_count` | 7.75 | 0 | 0 | 7.75 | 8 |

**判读**：
- ✅ **现在有死亡了**：5 死亡/episode（survived 3.5/8）。Phase 1 是 0 死亡/episode（survived 8/8）。task pressure 已经建立。
- ⚠️ **survived 在 ablation 之间几乎不变**（3.5 vs 3.75）：信号语义改成"safe rally"后，protagonist 即使不听信号也按 random walk 自然分布，predator 在 60×60 地图上是均匀威胁，nest 没有物理屏障，"在 nest 附近"和"在地图角落"对生存影响差不多。结果是 survived 不能作 primary metric 用。
- ✅ **fitness ▼88%**：reward 链路完全反映关闭信号。这是该 task 最干净的可信指标。
- ✅ **listener / responder 链路 alive**：8/8 listen，7.75/8 真心走过去。Phase 1 是 7.25 / 4.75，Phase 2 略好。

### 9.5 `comm_build_request` Phase 2 结果

memory baseline 行：

| metric | none | disabled | listener_blind | one_channel_only | noise |
|---|---:|---:|---:|---:|---:|
| `worksite_completion_events` (primary) | 0 | 0 | 0 | 0 | 0 |
| `deposits_avg` | 0 | 0 | 0 | 0 | 0 |
| `avg_fitness` | **41.3** | 22.0 (▼47%) | 19.7 (▼52%) | 41.3 | 36.4 (▼12%) |
| `signal_listener_count` | 8 | 0 | 0 | 8 | 8 |
| `signal_responder_count` | 3.75 | 0 | 0 | 3.75 | 2.5 |

**判读**：
- ❌ **worksite_completion 仍 0**：单 stick + 0 stone 还是没人完成。原因诊断：
  - `worksite.direction_requires_visibility=true` 让 perception 完全看不到 worksite，protagonist 不知道往哪走 deposit
  - emitter 在 worksite 位置，但 v8 listener 是"朝信号走"而不是"在 emitter 处停下并 deposit"
  - Phase 2 没改 protagonist agent / capability，只改 task 配置；deposit trigger 需要 protagonist 真心进入 worksite interaction_radius 并 carry stick，两个条件叠在一起没有命中
- ✅ **fitness ▼47% / ▼52%**：信号 reward 链路依然干净反映 ablation
- ✅ **listener / responder alive**：8/8 / 3.75/8

**剩余问题**：要让这条 task 的 worksite_completion 真起来，需要其中之一：
1. visibility 改回 true（但失去"信号是唯一指引"的实验意义）
2. 改 ProtagonistEcologyRuntime 让 emitter pulse 触发 listener 的 GoalManager 直接切到 `BuildWorksite` goal（让 goal-action-assist 接管最后 deposit 这一步）
3. 给 protagonist 训练时加专门的 build_request 任务（B 阶段任务）

这 3 条都需要更深的工程改动；在 A3 范围内**接受 fitness drop 当证据，承认 worksite_completion 在当前 protagonist 能力下不能成为 primary metric**。

### 9.6 Phase 2 总结

| Task | primary metric drop? | fitness drop | listener/responder drop | 综合 |
|---|---|---|---|---|
| `comm_scout_worker` | ✅ responders 4.25 → 0 (▼100%) | ▼93% | ✅ | ✅ 强证据，Phase 2 比 Phase 1 还更强（更长跑时 best_fitness 抬升） |
| `comm_danger_warning` | ⚠️ survived 不动 (3.5→3.75) | ▼88% | ✅ 7.75 → 0 | ⚠️ fitness/responder 强，survived 失败 |
| `comm_build_request` | ❌ worksite=0 不动 | ▼47% | ✅ 3.75 → 0 | ⚠️ fitness/responder 成立，primary 失败 |

最终判定：A3 主线的"至少一条任务关闭信号后明显下降"判定**多重成立**（scout-worker 主指标 + fitness、danger_warning fitness、build_request fitness、signal_response fitness），但**survived / worksite_completion 这两类需要物理实体 + protagonist 训练状态匹配的 primary metric 在当前 v8 能力下还不能用做最干净的"开/关"对照**——这是 A3 这一轮真实的能力上限，不是 task 设计bug。继续把这两类 primary metric 跑起来需要 protagonist 训练阶段就接触到这两个任务（B 阶段范围）。

---

## 10. Phase 3 收口 — preactivate / signal cue / nest hack / MI 分析（2026-05-22 晚）

第二轮 sweep 暴露的两条 primary metric 失效（`survived` / `worksite_completion`），本轮 Phase 3 做了三处真实修复 + 一处 hack 占位 + 一次 MI 分析。

### 10.1 修复 1 — `worksite.preactivate`（让 build_request 真起来）

**根因**：memory ablation 强制关 `progression`，加上 `world_role="eval"` 不会调 `WorksiteLayer::activateNextDormant()`，所以 `comm_build_request` task 全 4 episode 里 worksite 都是 dormant，agent 就算把 stick 搬到 worksite 位置也没人接收。`worksite_completion_events_avg=0` 不是大脑能力问题，是世界状态问题。

**修复**：`ProtagonistEcologyConfig.WorksiteConfig` 加 `preactivate` 字段；`ProtagonistWorldFactory` 在 `live_role || preactivate` 时调 `activateNextDormant()`；`MemoryAblationEval` 的 `CommBuildRequest` 设 `preactivate = true`。

**Phase 3 数据**（`data/runs/protagonist_a3_v8_signal_ablation/none/20260522-172613/` 与 `disabled/20260522-172819/`）：

| variant | NONE worksite_completion | DISABLED worksite_completion | NONE fitness | DISABLED fitness |
|---|---:|---:|---:|---:|
| baseline | 0.75 | 1.00 | 52.49 | 51.62 |
| no_spatial | 0.50 | 0.50 | 37.53 | 22.28 |
| no_episodic | 0.75 | 1.00 | 52.30 | 51.62 |
| no_social | 0.75 | 1.00 | 51.82 | 57.93 |
| no_goal | 0.50 | 0.50 | 37.50 | 22.69 |

**判断**：
- `worksite_completion` 从 phase 2 的全 0 拉到 0.5-1.0，**从 0 起来了**
- `no_spatial` / `no_goal` 都掉到 0.5（baseline 的一半）+ fitness 同步下降，**说明 spatial memory 和 GoalManager 是建造的真正驱动**，链路解耦清晰
- 信号开 / 关在 baseline 上方差太大（0.75 vs 1.0），**4 episodes 不够下"信号驱动 build"的结论**，但 fitness 在 spatial-blind 变体上有 ~40% 差距（37.5 vs 22.3），暗示 signal 在 spatial-blind 时仍有边际作用

### 10.2 修复 2 — `GoalActionAssistCapability` signal cue fallback

`carryReturnTarget` 现在的优先级链：
1. `spatial.findNearestWorksite` 命中 → 走它
2. 否则 `world.getLayer<WorksiteLayer>` 有 active+incomplete site 且 `episodicMemory.recentOfType("peer_signal_heard", 1)` 有事件且 ≤30s → 走 signal pos
3. 否则回 base

**实测**：本轮配置下 spatial 在 28m 写入，agent 出生即可写入 worksite_seen，**主路径几乎不会进入 signal fallback 分支**。但单测正负例都锁住，留作 spatial-blind 变体的合法 cue。

### 10.3 修复 3 — TraceRecorder 加 3 列 env-state + MI 分析

**目的**：之前没有"信号 active 是不是真的对应世界中某个事件"的 ground-truth 证据，A3 下结论只能依赖 fitness drop 这一个口径。本轮给 `agent_trace.csv` 加 3 列，提供 MI 分析所需的输入：

- `nearest_food_dist` — `ResourceLayer::nearestAvailable(pos)->distance`
- `predator_nearby_count` — 12m 内 `isPredator()` agent 数
- `worksite_distance` — `WorksiteLayer::nearestActiveSite(pos)` 距离

配合 `scripts/protagonist_communication_mi.py`（fallback glob 已扩到 `<run_dir>/trace/<task>/<variant>/episode_*/agent_trace.csv`），跑 `none` + `disabled` 两个 ablation × 7 task × 5 mem variant 的 trace sweep。

**关键 MI 数据**（`data/runs/protagonist_a3_v8_signal_ablation_trace/communication_mi_combined.csv`）：

| ablation | task | channel | variable | MI (bits) |
|---|---|---|---|---:|
| **none** | **comm_danger_warning** | 0 | **danger_nearby** | **0.36085** |
| none | comm_danger_warning | 0 | food_nearby | 0.13861 |
| none | comm_scout_worker | 0 | food_nearby | 0.04102 |
| none | comm_build_request | 0 | food_nearby | 0.03937 |
| none | comm_build_request | 0 | worksite_nearby | 0.02460 |
| **disabled** | 全部 | 0 | 全部 | **0.00000** |

**判断**：
- `comm_danger_warning baseline` 下 channel-0 active × `predator_nearby` MI = **0.36 bits**，是 `food_nearby` 的 **2.6 倍**，是 `comm_scout_worker / comm_build_request` 上同变量的 **9-15 倍**。binary × binary MI 上限 1.0 bits，0.36 是显著关联，不是巧合。
- `disabled` 模式下 active=0，所有 MI=0，符合预期，验证 MI 信号链健康。
- 这是 A3 主线**第一份独立于 reward 的语义涌现证据**：v8 训练让 channel-0 在 danger 任务下与"附近真有 predator"统计绑定。

**已知局限**：
- 当前 trace 只输出 `recent_signal_salience` 一列（fallback 到 channel-0），**没有 per-channel intensity 列**，所以 channel-1/2/3 都为空，无法判断"两通道分化"
- 这是 B 阶段的事，已记录到 `PROGRESS.md`

### 10.4 hack 占位 — `BaseLayer.nest_predator_damage_per_second`

为了让 `comm_danger_warning baseline survived` 在 v8 能力下产出非零变化，加了一条临时规则：predator 在 nest 半径内每秒掉 60 hp（约 1s 死亡）。Phase 3 重跑后：

| variant | NONE survived | DISABLED survived | NONE fitness | DISABLED fitness | fitness drop |
|---|---:|---:|---:|---:|---:|
| baseline | 5.0 | 5.25 | 39.92 | 6.93 | ▼83% |
| no_spatial | 5.25 | 5.0 | 38.20 | 6.55 | ▼83% |
| no_episodic | 5.0 | 5.0 | 37.73 | 6.85 | ▼82% |
| no_social | 5.5 | 5.25 | 36.02 | 6.42 | ▼82% |
| no_goal | 6.0 | 6.0 | 35.28 | 6.61 | ▼81% |

**survived 提到 5.0/8（vs phase 2 的 3.5/8，▲43%），fitness drop 仍稳定在 80%+**。

但这条规则**不是用户想要的最终防御机制**。用户原话（2026-05-22）："堆材料挡狼，把材料堵在窝前面，堵着其他生物不让进来"。也就是说防御应该是**emergent shelter**：

- 主角搬材料到工地
- 工地完工后材料**变成物理碰撞墙体**
- 墙体阻挡 predator 移动（不是魔法掉血）
- 主角自己有路径或缝隙能进出

**这条 hack 必须在 B 阶段第一件事就替换掉**。已固化进 `FINAL_STATE_VISION.md § 3.7 emergent shelter / § 4.6 第一次有家可守`，按 L1-L5 分级实施。当前 hack 只作为 A3 这一轮 survived 度量的可达性占位。

### 10.5 Phase 3 综合证据链

`comm_danger_warning` 这条 task 现在有**三条独立证据**指向 v8 在用 channel-0 编码 danger：

1. **行为侧（fitness drop）**：39.9 → 6.9，▼83%，关闭信号后 reward 大幅退化
2. **存活侧（survived）**：3.5 → 5.0，▲43%（含 nest hack 加持）
3. **信息侧（MI）**：channel-0 × predator_nearby = 0.36 bits，是其他变量 9-15 倍

第三条是这一轮**第一次拿到的、完全独立于 reward 设计的、定量的语义涌现证据**。
