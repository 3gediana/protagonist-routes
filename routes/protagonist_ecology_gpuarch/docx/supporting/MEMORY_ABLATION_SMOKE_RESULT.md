# Memory Ablation · Smoke Result（首次基础设施验收）

> 日期：2026-05-21
> 配置：`configs/protagonist_ecology_train_smoke.toml`，3 代 × 200 秒 × 2 episodes × 8 protagonist
> 驱动脚本：`scripts/protagonist_memory_ablation.py`
> 输出根目录：`data/runs/protagonist_memory_ablation/`

---

## 1. 这一轮 ablation 想验证什么

**不是科学结论**。这是 Memory Phase A Step A6 的"开关接线 + 数据闭环"基础设施验收。

具体只验证三件事：

1. `population.protagonist.{spatial,episodic,social,goal}_*_enabled = false` 这四个开关真的能从 TOML 读到、真的能改变 runtime 行为
2. 关闭任一开关后，行为指标（chop / deposit / hunt / signal_co_attend）和 DNC summary 都会移动
3. 全开 baseline 与 4 个 ablation 之间没有任何一个变体崩溃 / 段错误 / 写出空 trace

---

## 2. 结果摘要

| variant | avg fitness (last gen) | best fitness | chop_succ | deposits | completed_worksites | signal_emits | signal_co_attend | hunt_succ | survived | dnc_usage_mean | dnc_read_peak |
|---|---|---|---|---|---|---|---|---|---|---|---|
| baseline    | 642.7         | 1038.5 | 28 | 25 | 0 | 48 | 475 | 4  | 14 | 0.274 | 0.064 |
| no_spatial  | 698.4 (+55.7) | 1341.7 | 45 | 28 | 0 | 48 | 504 | 6  | 15 | 0.228 | 0.035 |
| no_episodic | 850.2 (+207.5)| 1278.4 | 35 | 31 | 0 | 48 | 848 | 10 | 14 | 0.182 | 0.045 |
| no_social   | 950.8 (+308.0)| 1711.4 | 26 | 43 | 0 | 48 | 373 | 8  | 16 | 0.279 | 0.042 |
| no_goal     | 1073.7 (+431.0)| 1977.9| 26 | 51 | 0 | 48 | 646 | 11 | 16 | 0.333 | 0.065 |

---

## 3. 怎么读这张表

### 3.1 已经达成的事

- 每个开关都让多个独立指标动起来。这意味着记忆 / goal 写入路径**真正接进训练循环**了，没有"开关定义但没人读"的死代码
- `dnc_usage_mean` 和 `dnc_read_peak` 在不同 ablation 下也变化，说明 DNC 也参与了输入路径

### 3.2 一定不要把这表读成"记忆有害"

各 ablation 的 fitness 都比 baseline 高——这看起来像"关掉记忆反而更好"，但它**不是**真实的 Phase A 结论。原因：

- 只有 3 代。NEAT 进化不可能在 3 代内学会"如何使用 44 维 memory perception"
- 在没学会用之前，memory perception 只是 44 维噪声，加进 brain 输入会**拖累早期采样**
- 因此早期"关 memory 反而更好"是非常预期的过拟合反应

只有在**长训**（数百代）之后，记忆才有机会被 NEAT 选择压力**真正利用**起来，那时关掉记忆才会出现"显著下降"。

### 3.3 这一轮真正的有效结论只有一句

> **4 个 ablation 开关已接线、可执行、可观测，可以作为 Phase A 后续科研测试的基础设施。**

---

## 4. 后续要做什么（不在这一轮内）

- 长训（≥ 100 代）下重跑这 5 个变体，再看 fitness 是否真出现"关 memory 显著下降"
- 把 ablation eval 接到固定 task（water return / danger avoidance / worksite return / signal response）而不是开放式 smoke，从指标层面剥离"开放探索 vs 任务完成"的混淆
- 把 `memory.enable_*` 配置从当前 `population.protagonist.*` 命名空间提到独立 `[memory]` section（更易读，Phase B 再做）

这两步都属于"开始正式训练"范畴，按用户规则需要本人决策后再启动。
