# M4 · 多线程加速

> 让训练快起来。让个体有自己的节奏。

---

## 目标

1. **训练加速**：headless 训练时多线程并行评估个体，CPU 核心吃满
2. **异步决策**：每个个体有自己的决策周期 τ，不是统一被调度——让个体之间天然存在时机差，是涌现复杂行为的关键

> 这两件事看起来一样，**实际上是两个独立的并行性**。
> 训练加速 = 数据并行（多个 agent 同时被评估）
> 异步决策 = 时间异步（同一 agent 不是每个 tick 都决策）

---

## 前置条件

- M3 完美通过（`baseline-M3-done` tag）
- M3 训练时长在你电脑上**令人不爽**（这才有动机做 M4）

---

## 关键产出

### 1. Headless 训练并行池

```
src/runtime/headless/
├── EvaluationPool.h / .cpp     线程池，并行评估多个 agent
└── ParallelGenerationRunner.cpp  代际并行调度器
```

策略：
- 每代评估时，N 个独立小世界（micro-worlds）并行跑，每个跑 K 个 agent
- 用 `std::async` 或自实现线程池
- 默认线程数 = `std::thread::hardware_concurrency() - 1`（留一核给主线程）

**注意**：每个小世界里 agent 共存（self-play），所以同一物种的 agent 在不同小世界里互相成为对方的环境。这避免单 agent 评估的"无对手训练"问题。

### 2. 异步决策周期

```cpp
// 在 NeatGenome 加新基因
struct NeatGenome {
    ...
    float decision_interval;   // 该 agent 多少 tick 决策一次（可进化）
    float decision_interval_min = 1.0f;
    float decision_interval_max = 30.0f;
};
```

每个 agent 维护自己的"下次决策 tick"：
```
Agent::tick():
  if (current_tick >= next_decision_tick) {
    output = brain.forward(perceive());
    next_decision_tick = current_tick + decision_interval;
  }
  // 即使不决策，move/eat 等 capability 仍然按当前 output 持续执行
```

**进化效果**：
- 反应快的 agent 决策频繁，更精确但消耗更多能量（如果加能耗机制）
- 反应慢的 agent 决策少，省能但容易错过机会
- 这两种策略的"军备竞赛"是**自发涌现**，不是你设计

### 3. 能量消耗与决策频率挂钩

```toml
[energy]
base_metabolism = 0.001        # 每 tick 基础消耗
decision_cost = 0.005          # 每次大脑前向推理额外消耗
movement_cost_factor = 0.001   # 移动速度的平方 × 这个系数
```

让"频繁决策"有真实代价 → 进化才会平衡决策频率。

### 4. 线程安全审计

之前的所有共享数据现在可能被多线程访问。审计清单：
- [ ] `InnovationTracker` 加锁或改 atomic
- [ ] `EventBus.publish()` 加锁（订阅者列表 + 事件队列）
- [ ] `Config` 只读访问，不需要锁
- [ ] `ResourceLayer` 食物再生：每代结束时单线程批量更新
- [ ] CSV 写入：单线程通过专用 writer 线程

### 5. 性能仪表盘

```
src/core/observer/
└── PerformanceObserver.cpp     记录每代时长、每 tick 时长、各阶段耗时
```

输出 `data/runs/<ts>/perf.csv`，便于优化时定位瓶颈。

---

## 三档完美标准

### MVP ✅⛳⛳
- [ ] 多线程评估能跑，无 race condition（用 ThreadSanitizer 验证）
- [ ] 异步决策周期作为基因可被变异

### 完整 ✅✅⛳
- [ ] 训练 100 代时长比 M3 单线程快 ≥ 3 倍（4 核机器）
- [ ] 异步决策不会导致 agent 行为崩坏
- [ ] 能量消耗系统工作（agent 会因为饥饿而死）
- [ ] perf.csv 数据有效

### 完美 ✅✅✅
- [ ] **观察到决策频率分化**：跑 100 代后，种群中决策周期的标准差 ≥ 5 tick（说明确实进化出快慢差异）
- [ ] **互动模式倍速测试**：在 interactive 模式下，64x 倍速跑 50 代不卡顿
- [ ] 训练时长基线对比：M3 vs M4 数据归档到 `docs/experience_log.md`
- [ ] git tag `baseline-M4-done`

---

## 🧠 你的补充点

| 项 | 你需要决定 |
|---|---|
| **每代并行小世界数** | 默认 = CPU 核心数。你可以调小让训练更稳，调大让训练更快 |
| **决策周期范围** | 默认 [1, 30] tick。范围决定"快慢策略空间" |
| **能量消耗权重** | 三个系数（基础/决策/移动）的相对比例 —— 影响"懒"与"勤"哪种策略胜出 |
| **是否启用 self-play 多小世界** | 默认启用。关闭后所有 agent 在同一大世界里跑（更视觉但训练慢） |

---

## 🔌 接入点

- F-LLM 解说员订阅 perf 事件，可以播报"训练加速到 12x 实时"
- F-GOD 上帝按钮可以现场调整决策周期范围，看种群如何重新适应
- F-COMPARE 对照实验本质就是**两个 EvaluationPool 并行跑**，结构借鉴 M4

---

## ⚠️ 易踩坑

1. **过早多线程化**：M3 跑得慢的根因可能是其它（fitness 计算复杂度、内存分配频繁），先 profile 再优化。perf.csv 帮你定位
2. **隐式共享状态**：`std::default_random_engine` 是全局共享会被多个线程争抢。每个线程一个独立的 RNG（用 thread_local）
3. **过度细粒度任务**：把单个 agent 的单 tick 当作并行任务，调度开销 > 计算时间。**正确粒度**：一个小世界跑完整个 episode 才算一个任务
4. **死锁**：EventBus 加锁要小心订阅者在事件回调里又发事件。**用 deferred queue**：publish 把事件入队，flush 阶段单线程派发
5. **NeatBrain 状态被多个线程读写**：每个 agent 有自己的 NeatBrain 实例，**不共享 Brain**，但共享 Genome（只读）

---

## 验证

```bash
# Linux/WSL 上跑 ThreadSanitizer
g++ -fsanitize=thread ... && ./neural-eco --mode headless

# Windows 上用 Visual Studio 的 Address Sanitizer + Concurrency Visualizer
```

至少跑 50 代不报任何 race。

---

## 下一阶段

→ [M5 · 真实 2.5D 地形](M5_terrain.md)
