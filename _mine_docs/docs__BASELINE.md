# BASELINE 评判口径

> **PPO 训练成功 / 失败的判定基线，由 wander 随机策略 100 局得出。**
> 文件来源：`runs/wander_baseline.jsonl`，100 episodes，seed=42。
> 重新生成：`scripts/build.ps1` 后 `build/bin/deep_protagonist_train.exe --steps 1000000 --episodes 100 --seed 42 --metrics runs/wander_baseline.jsonl`，再 `python3 scripts/summarize_metrics.py runs/wander_baseline.jsonl`

## 100 局 wander 统计

| | mean | std | p10 | median | p90 |
|---|---:|---:|---:|---:|---:|
| **total reward** | -37.38 | 94.13 | -193 | +15.24 | +25.47 |
| r_alive | +12.43 | 4.86 | +2.12 | +15.00 | +15.00 |
| r_food | +2.04 | 2.71 | 0 | +0.24 | +6.10 |
| r_water | +2.15 | 11.32 | 0 | 0 | 0 |
| r_kills | 0 | 0 | 0 | 0 | 0 |
| r_bites | -26.91 | 41.79 | -98.33 | 0 | 0 |
| r_death | -29.00 | 45.38 | -100 | 0 | 0 |
| r_collect | +1.94 | 1.39 | +0.45 | +1.75 | +3.50 |
| r_shelter | +0.65 | 1.96 | 0 | 0 | +5.00 |
| r_craft | +0.45 | 1.15 | 0 | 0 | +3.00 |
| r_milestone | +0.40 | 3.98 | 0 | 0 | 0 |

**死亡率**：29/100 = **29%**（基本都被狼咬死）。
**平均存活步数**：7457 / 9000（活满 5 分钟）。

## PPO 学到没？四档判定

| 指标 | 必须达到 | 含义 |
|---|---|---|
| 🟥 **mean reward < -37** | 失败 | PPO 比 wander 还差，肯定有 bug |
| 🟨 **mean reward > 0** | 起步 | 学会基本求生（少死） |
| 🟧 **mean reward > +50** | 学到点 | 主动找食/水/避狼，超过 wander p90 |
| 🟩 **mean reward > +100** | 真有成果 | 进入中期建造，开始拿 milestone |

**附加单项指标**（任一明显超 wander 就是真学到）：
- **死亡率 < 10%** → 学会避狼（wander 29%）
- **r_kills > 5** → 学会主动战斗（wander 0）
- **r_milestone > 20** → 解锁了中期建造（wander 0.4 全靠运气）
- **r_food > 30** → 学会精准 eat（wander 2.04）

## 训练防退化警报

蓝图 line 108："训练曲线分别记录每种行为得分，**任一退化立刻警报**"。

具体规则（建议在训练 logger 里实装）：
- 每 100 episodes 求一次窗口均值
- 比较窗口 t 和窗口 t-1
- 任一组件 mean **下降超过 30%** 且绝对值 > 2.0 → 报警
- 这是 EWC（弹性权重固化）介入的触发线

## headless 性能

- **42-50k steps/s**（单线程 wander）
- **~900 steps/s**（PPO，含 GPU forward + 周期性 update + 主循环）
- 1 episode (wander) = 9000 steps ≈ 0.18 秒墙钟
- 1 episode (PPO) ≈ 10 秒墙钟
- 真正瓶颈在 PPO forward+backward，不在 env 模拟

## 训练日记（每次训练一条，对照基线判定）

| 日期 | run | 网络 | 步数 | mean R (last 20) | 死亡率 | r_water | r_bites | 关键观察 | tag |
|---|---|---|---|---|---|---|---|---|---|
| 2026-05-24 | smoke | MLP 145→256 | 10k | +19.0 (1ep) | 0% | 0 | 0 | 1 ep smoke 验证 | 🟦 |
| 2026-05-24 | **ppo_run1_buggy** | MLP | ~600k(killed) | reward hacking | 0% | +6000 spam | 0 | drink reward 无 cap，PPO 锁定刷水路径，发现并修复 D-036 | 🟥 |
| 2026-05-24 | **ppo_run2** | MLP 145→256 | 1M | +16.29 | 0% | 0 | -7.17 | reward hacking 修复后 baseline。1M 步 plateau，找水靠运气 | 🟨 |
| 2026-05-24 | **ppo_run3_gru_sm** | **GRU+SpatMem 564→256→GRU(256)** | 1M | **+18.57** | **0%** | **+1.79** | **-2.17** | **稳定学会找水！避狼 -70%，但 milestone 频率没起来。1M 步对 GRU 不够** | 🟨 |
| 2026-05-24 | **ppo_run5_multi_cost** (killed) | + multinomial(17) + trigger_cost 0.001 | ~1.5M | +16.13 (surv) | 14% | +1.36 | -14.27 | D-039：Bernoulli sigmoid 全 0.5 病修复。drink 锁死 32%（用户嫌烦）→ kill 调 thirst | 🟨 |
| 2026-05-24 | **ppo_run6_thirst_slow** | 同上 + thirst_decay 12→4/min | 2M | -29.75 (surv +10.82) | 24% | +0.99 | -34.00 | D-040：drink 5%（用户目标 ✅）但 NOOP 22%→52% 飙升，milestone 0，死亡率高。thirst 调慢过头了 | 🟧→🟨退化 |

### ppo_run3 vs ppo_run2 详细对比（last 20 ep）

| 指标 | run2 (MLP) | run3 (GRU+SpatMem) | Δ |
|---|---:|---:|---:|
| mean reward | +16.29 | **+18.57** | +14% |
| r_food | +1.52 | +0.79 | -48% |
| r_water | 0.00 | **+1.79** | +∞ (首次稳定) |
| r_kills | 0.00 | +1.25 | +∞ |
| r_bites | -26.46 | **-2.17** | **避狼提升 92%** |
| r_collect | +1.70 | **+2.85** | +68% |
| r_shelter | +1.00 | 0.00 | -100% |
| r_milestone | 0.00 | 0.00 | 0 |
| 死亡率 | 0% | 0% | = |

**关键结论**：
- GRU+SpatialMemory 让 PPO 在 1M 步内**学会了找水**（run2 1M 步都没学到）
- 避狼能力**大幅提升 92%**（spatial memory 标记 danger cell + GRU 短期记忆配合）
- 但 1M 步对 GRU 还是不够：milestone 0、shelter 退化、food 略降，说明 policy 在"探索环境/避狼/找水"上偏多，没分到"建造 / 工具"
- 需要 5-10M 步才能进 🟧 +50 区间

---

## D-043 新评判口径（2026-05-24 19:00 起）

D-043 reward 哲学重设后，旧的 mean reward 数字不再可比（量级变了 10×）。新评判用 **score 几何均值**（Crafter 标准）+ 行为指标：

### Wander baseline (D-043 reward)

10 局 wander, seed 12345：

| 指标 | wander |
|---|---:|
| mean reward | +14.0 (drink/food/milestone 全部新公式) |
| score (geometric mean) | **0.0718** |
| 死亡率 | 1/11 = 9% |
| 撩狼率 (bites>50 episode) | 1/11 = 9% (偶然事件) |
| Φ (state value) | +3-5 / ep (inventory only) |

### PPO 判定基线（D-043）

| 指标 | 必须达到 | 含义 |
|---|---|---|
| 🟥 **score < 0.07** | 失败 | 不如 wander 广度 |
| 🟨 **score > 0.10** | 起步 | 比 wander 多解锁几种 milestone |
| 🟧 **score > 0.15** | 学到点 | 解锁广，stably across episodes |
| 🟩 **score > 0.20** | 真有成果 | 接近 Crafter PPO 学术水平 4.6% (注：Crafter 是 22 ach × 22 维度，难度不同) |

**附加判定**（一票通过）：
- **撩狼率 < 5%** → reward shape 工作（D-043 主目标）
- **r_potential mean > +30** → state-value 持续奖励生效（agent 保持状态）
- **milestone unique > 5/10** → 故事弧广度
- **死亡率 < 10%** → drive_reduction 引导生存优先

### D-043 PPO 早期数据 (前 20 ep, 2M 训练进行中)

| 指标 | D-043 ppo_d043_2M | wander | run7 |
|---|---:|---:|---:|
| score | **0.18-0.22** | 0.07 | (旧公式) |
| 撩狼率 | ~30% 早期 → 0% 后期 5 ep | 9% | 高（first_hunt+20）|
| 死亡率 | 33% → 0% (ep15-19) | 9% | 24% |
| r_potential | +27 累积 / ep | +3-5 | - (新字段) |

**早期信号**：PPO 在 200k step 已经 score = wander 3 倍。等 2M 训练完看长期趋势。
