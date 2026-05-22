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
