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

格式：日期 | 步数 | 命令 | mean reward | 死亡率 | 关键观察 | verdict tag

| 日期 | 步数 | 命令 | mean R | 死亡率 | 关键观察 | tag |
|---|---|---|---|---|---|---|
| 2026-05-24 | 10000 | smoke ppo --seed 42 | +19.0 (1ep) | 0% | smoke 验证，1 ep 不算 | 🟦 |
| _待填_ | _1M_ | _ppo --seed 42 --metrics ..._ | _?_ | _?_ | _?_ | _?_ |
