# Communication Semantics Evidence · 2026-05-22

> 位置：`routes/protagonist_ecology/docx/supporting/COMMUNICATION_SEMANTICS_EVIDENCE.md`
>
> 作用：A3 收口的结果说明。三个最小通信任务（scout-worker / danger warning / build request）的设计、跑分、ablation drop、互信息（MI）矩阵、listener policy shift 的真实数据落地点。
>
> 配套设计文档：`DEVELOPMENT_TASK_PLAN.md § A3` + `WORLD_UNDERSTANDING_RESEARCH_AND_ENGINEERING_PLAN.md § 7`。
>
> **状态：S1-S4 基础设施代码已就位（commit 见 `git log`）。S5 实验跑分尚未启动，所有数值表格当前是占位符。这份文档随实验数据持续更新。**

---

## 1. A3 三任务对照表

| Task | 设计语义 | controlled emitter 行为 | primary metric | 期望 ablation drop（baseline → signal_disabled） |
|---|---|---|---|---|
| `comm_scout_worker` (S2) | scout 看见 → emit; worker 盲 → 跟信号去食物 | 周期 20s, 距离 22m±6m, 携带 2 份 18 单位 food drop | `signal_responder_count` | drop > 50% |
| `comm_danger_warning` (S3) | sentry 看见 predator → emit; 其他盲 → 听信号逃 | 周期 12s, 距离 18m±6m, 无 food, 3 predator pack 隐藏在 emitter 附近 | `survived` count | drop > 30% |
| `comm_build_request` (S4) | worksite 缺料 → emit; 携材 listener 听信号去 deposit | 周期 15s, 距离 0m (emit 在 worksite 位置), 无 food | `worksite_completion_events` | drop > 50% |

注：所有任务都用 controlled emitter 充当"虚拟 scout/sentry/builder"。protagonist 自己自发 emit 在 A3 阶段**不要求**——这与 EXECUTION_GUIDE §A3 一致，"通信语义不靠 reward 硬拧，靠任务压力涌现"，但作为 minimal probe 第一步先证明 listener 能用信号；下一步（A3 后续 / B）才能放开 protagonist emit。

---

## 2. S1 基础设施落地点

### 2.1 communication_metrics.csv

- 写入路径：`data/runs/_communication_metrics.csv`（与 `_credit_log.csv` 同位的共享日志，含 scenario_name 字段区分多 run）
- 每行字段：
  ```
  scenario_name, generation, channel,
  emit_count, unique_emitter_count, unique_listener_count,
  listener_ticks, silent_ticks, response_events,
  listener_policy_shift_l2, listener_policy_shift_cos,
  response_latency_avg_seconds, latency_sample_count
  ```
- 数据源：`runtime/CommunicationMetrics.{h,cpp}` 类的 episode-级累积器
  - `recordEmit(channel, emitter, time)` ← `AgentEmittedSignal` event subscriber
  - `recordListenerAction(listener, velocity_xy, channel_mask, time)` ← listener block 每 tick 调一次
  - `summarise(generation_index)` + `appendCsv()` ← episode 末
- **action vector = (velocity.x, velocity.y) (2 维)**。listener_policy_shift_l2 单位是 m/s。这是不改 Agent/Brain 接口的最简选择；如果后续想换成 brain output 原始动作分布，需要 expose `Brain::lastOutput()` 接口。

### 2.2 4 个 ablation 开关

`world.signal.ablation = "none|disabled|listener_blind|one_channel_only|noise"`

| Ablation | runtime 拦截点 | 效果 |
|---|---|---|
| `disabled` | (1) `emitControlledSignal` 直接 return; (2) `AgentEmittedSignal` subscriber 直接 return | 整条信号链彻底关——controlled + protagonist 自发 emit 都进不去 ledger / metrics |
| `listener_blind` | listener block + peer_signal_heard episodic 入口 guard | emit 还在，但 protagonist 完全不感知（dense reward / halo / episodic / metrics mask 全 0） |
| `one_channel_only` | subscriber 把 `effective_channel = 0` | 多通道塌缩成单通道 |
| `noise` | (1) `emitControlledSignal` 位置加 Gaussian(`noise_position_stddev`); (2) subscriber 同样对 protagonist 自发 emit 位置加 noise | listener 还能听到但位置不准 |

参数：`world.signal.noise_position_stddev = 1.5` 默认（米）。

### 2.3 MI 离线脚本

- `scripts/protagonist_communication_mi.py`
- 输入：`<run_dir>/trace/generation_*/episode_*/agent_trace.csv`
- 输出：`<run_dir>/communication_mi_report.csv`，每行 `(scenario_name, channel, variable, mutual_information_bits, ...)`
- 变量：`food_nearby / water_nearby / danger_nearby / worksite_nearby`
- MI 单位：bits（log2）

### 2.4 单测覆盖

`tests/route_protagonist/test_communication_metrics.cpp` 共 9 个 gtest：
- emit 计数 / unique_emitter 去重 / 通道隔离
- policy_shift L2 / cos 距离 / 全 0 / 正交
- 响应延迟 5s 窗口 / NaN-without-emits
- response_events 检测
- clear() 重置 / appendCsv 头+行
- 全部 PASS（128/128 tests 总绿）

---

## 3. S2 scout-worker — 实验结果（待跑）

### 3.1 跑分

跑分配置（参考）：
- `memory_ablation.enabled = true`
- `memory_ablation.episodes_per_task = 4`
- 7 个 task × 5 个 variant = 35 行 + 4 个 signal ablation × 7 task = 56 行（如果 4 signal ablation 用 separate run）
- 推荐路径：先跑 baseline + 4 signal ablation 在 `comm_scout_worker` 单任务 4 episodes，验证基础设施
- 然后 baseline + 4 signal ablation × 3 个 comm task = 15 行

| variant | signal_responder_count_avg | avg_fitness | listener_policy_shift_l2 (ch0) | MI(ch0, food_nearby) bits |
|---|---|---|---|---|
| `comm_scout_worker / baseline` | TODO | TODO | TODO | TODO |
| `comm_scout_worker / signal_disabled` | TODO | TODO | TODO | TODO |
| `comm_scout_worker / listener_blind` | TODO | TODO | TODO | TODO |
| `comm_scout_worker / one_channel_only` | TODO | TODO | TODO | TODO |
| `comm_scout_worker / signal_noise` | TODO | TODO | TODO | TODO |

### 3.2 解释

待数据落地后补。

---

## 4. S3 danger warning — 实验结果（待跑）

### 4.1 跑分

| variant | survived_avg | protagonist_deaths_avg | listener_policy_shift_l2 (ch0) | MI(ch0, danger_nearby) bits |
|---|---|---|---|---|
| `comm_danger_warning / baseline` | TODO | TODO | TODO | TODO |
| `comm_danger_warning / signal_disabled` | TODO | TODO | TODO | TODO |
| `comm_danger_warning / listener_blind` | TODO | TODO | TODO | TODO |
| `comm_danger_warning / one_channel_only` | TODO | TODO | TODO | TODO |
| `comm_danger_warning / signal_noise` | TODO | TODO | TODO | TODO |

### 4.2 解释

待数据落地后补。注意：S3 用 `signal_response_dense_reward = -0.6f`（反向）来训"逃"——这是与 S2/S4 关键差异。如果 S3 跑出来 listener_policy_shift_l2 与 S2 类似（dense_reward 决定 listener 在听到信号时朝哪个方向走，sign 直接体现在 velocity 上），就说明信号→行为方向链通了。

---

## 5. S4 build request — 实验结果（待跑）

### 5.1 跑分

| variant | worksite_completion_events_avg | deposits_avg | listener_policy_shift_l2 (ch0) | MI(ch0, worksite_nearby) bits |
|---|---|---|---|---|
| `comm_build_request / baseline` | TODO | TODO | TODO | TODO |
| `comm_build_request / signal_disabled` | TODO | TODO | TODO | TODO |
| `comm_build_request / listener_blind` | TODO | TODO | TODO | TODO |
| `comm_build_request / one_channel_only` | TODO | TODO | TODO | TODO |
| `comm_build_request / signal_noise` | TODO | TODO | TODO | TODO |

### 5.2 解释

待数据落地后补。**S4 是 A3 最复杂的任务**——同时需要：
1. listener 听信号
2. listener 携带 stick/stone（GatherStick/Stone goal）
3. listener 移动到 worksite
4. listener deposit
5. worksite 完成 build

任何一环坏掉都会让 S4 失败。如果只有 S4 跑不出 ablation drop，要追查的不是信号本身而是材料-worksite 耦合（参考 audit § 7.4 worksite_return 的修法）。

---

## 6. A3 结论与遗留

待 S2-S4 跑分落地后补。预期结论模板：

- ✅ S1 基础设施 PASS（128 tests + ablation 开关都验证）
- ⚠️/✅/❌ S2 scout-worker drop 是否 > 50%
- ⚠️/✅/❌ S3 danger warning drop 是否 > 30%
- ⚠️/✅/❌ S4 build request drop 是否 > 50%
- ⚠️/✅/❌ 至少有一条任务 ablation drop > 50%，证明信号在 baseline 是真有用的
- ⚠️/✅/❌ MI 矩阵证明信号通道与至少一个环境变量有显著互信息

遗留可能性：
- 通道无语义压力（A3 用任务压力驱动，没硬绑定 channel）——NEAT 在短训中可能找不到 channel ↔ semantic 的映射。这与文献预期一致（emergent communication 通常需要长训）。
- listener_policy_shift_l2 用 velocity 不是 brain output——如果观察到 shift 很小但 NEAT 实际有学到信号响应，可能要换成 brain output 维度（需要改 Agent/Brain 接口，B 阶段任务）。
- S4 与 worksite_return 共享 logistics 链——任何 logistics bug 会同时影响两条任务。

---

## 7. 怎么跑

```bash
# 准备 toml：configs/protagonist_ecology_memory_ablation.toml 加 4 个 ablation 变体
# 例：world.signal.ablation = "disabled"

# 跑（windows）
D:\claude-code\c++\build_vs3131\bin\Release\neural-eco-protagonist.exe --config configs\protagonist_ecology_memory_ablation.toml

# 跑后读：
#   data/runs/<run_dir>/memory_ablation_results.csv  (主表)
#   data/runs/_communication_metrics.csv             (S1 per-channel 指标)
#   data/runs/_credit_log.csv                        (A2 share-based credit)

# MI 离线计算：
python3 scripts/protagonist_communication_mi.py data/runs/<run_dir>
# 输出 <run_dir>/communication_mi_report.csv
```
