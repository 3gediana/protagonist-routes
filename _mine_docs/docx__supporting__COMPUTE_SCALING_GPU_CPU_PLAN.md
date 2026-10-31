# Protagonist Ecology · 计算扩展与 GPU / CPU 承接方案

> 位置：`routes/protagonist_ecology/docx/supporting/COMPUTE_SCALING_GPU_CPU_PLAN.md`
>
> 作用：承接 `MEMORY_SYSTEM_MASTERPLAN.md`。这份文档回答的问题不是“记忆系统怎么设计”，而是**当主角记忆系统和主行为脑一起变大之后，算力路线怎么接**。
>
> 状态：规划文档；2026-05-21 truth-sync：CUDA build 路径已可编译运行，但 camp-building 1-generation benchmark 暂未显示优于 CPU 的训练总耗时。

---

## 0. 总结先行

后续算力路线不应该是：

- 现在立刻把整个项目全部 GPU 化

也不应该是：

- 死守当前 CPU episode 级并行不动

正确路线是：

```text
先把记忆系统接活
    -> 把主角行为脑同步扩容
    -> 先把 CPU 并行从 episode 级提升到更能吃满 16C/32T
    -> 再把 protagonist dense brain 做成 mixed GPU
    -> 世界逻辑 / EventBus / 大部分显式记忆写入仍留在 CPU
    -> 后续 world model / 大型 dense 模块优先走 GPU
```

一句话：

**后续不是全量 GPU 重构，而是 protagonist cognition 的混合 GPU 化 + 世界仿真的深度 CPU 并行化。**

---

## 1. 当前计算实况

## 1.1 当前机器

当前机器条件：

- CPU：`AMD Ryzen 9 8945HX`
- 核心数：`16C / 32T`
- 内存：约 `32 GB RAM`
- GPU：`RTX 5060 Laptop GPU`
- 显存：`8 GB VRAM`

这个配置的结论不是“很弱”，而是：

- 继续扩大主角脑，完全有空间
- 做 protagonist mixed GPU，有现实意义
- 但做全系统 GPU 重构，仍然是大工程，不适合作为第一步

## 1.2 当前 protagonist route 的真实并行方式

当前 protagonist route 已不再只是旧版的“纯 episode 级并行”。

现在更准确的描述是：

- 外层仍由 `EvaluationPool` 按 `episode / world replica` 并行
- 内层已经有 `compute.agent_decision_threads`，把 in-world agent decision 计算切成并行
- `world advance / event resolve / 大部分状态提交` 仍主要在 CPU 受控阶段串行完成

所以真实判断应当是：

- `episodes_per_generation` 仍会限制**外层**并行度，但不再是唯一上限
- CPU 并行基础设施已经比“只有 2~3 个重任务在跑”更深一步
- 是否真的吃满 `16C / 32T`，要结合 profiling 与具体配置判断，不能只凭架构推断

最近 camp-building benchmark 的一个参考配置就是：

- `pool_threads = 3`
- `agent_decision_threads = 10`

## 1.3 当前 protagonist brain 还没到 GPU 必需阶段，但已经不小了

当前 camp-building 主训练配置下，主角脑已经大约是：

- sensory dim ≈ `128`
- hidden dim = `512`
- memory cells = `48`
- read heads = `4`
- action dim = `13`
- protagonist controller params ≈ `355,188`（约 `355k`）

但这还不是终点。

按 `MEMORY_SYSTEM_MASTERPLAN.md` 的方向，后续会进入：

- `400k ~ 800k` 第一阶段大脑
- `1.2M ~ 2.5M` 第二阶段复杂行为脑
- `3M+` 研究档

一旦走到这些档位，尤其同时有：

- 多主角并行
- 多 episode
- 长 episode
- 更丰富 memory query

那么 protagonist dense inference 就很可能成为重要瓶颈。

## 1.4 当前 CUDA reality

最新本机实况是：

- `build_cuda/` 已能用 portable CUDA（`D:\NVIDIA\CUDA\v12.8\`）构建 protagonist CUDA 路径
- 运行时仍需把 `D:\NVIDIA\CUDA\v12.8\bin` 放进 PATH，否则会缺 `cudart64_12.dll`
- camp-building 1-generation benchmark：CPU 约 `5.6s/gen`，CUDA build 约 `8.5~9.2s/gen`

所以当前口径只能写成：

- **CUDA build 路径可运行**
- **但“GPU 已带来训练加速”这个结论目前不成立**

---

## 2. 为什么不是现在立刻全项目 GPU 化

因为当前项目的主要耗时结构，不是单一一个大矩阵模型。

现在系统包括：

- 世界层对象逻辑
- Layer 更新
- EventBus
- 多 agent capability
- background species 的 NEAT 异构网络
- checkpoint / resume / trace / metrics
- 大量 route-local 条件逻辑

这些东西里：

- 有些非常适合 GPU
- 有些不适合
- 有些强行 GPU 化会带来高昂的同步/重构成本

因此现在直接做“全项目 GPU 重构”，会出现几个问题：

- 世界仿真逻辑得大改
- OOP/事件驱动结构要大改
- background NEAT 的异构结构也要跟着改
- checkpoint / replay / trace 路线复杂度暴涨

而现阶段最需要的其实是：

- memory 真闭环
- protagonist brain 真扩容
- profiling 真搞清楚瓶颈在哪

所以：

**不是不用 GPU，而是现在不能用“全量 GPU 重构”这个最重方案开局。**

---

## 3. 目标计算架构

最终建议的计算架构：

```text
CPU side
├── world simulation
├── event bus
├── capability resolution
├── explicit memory writes
├── social / episodic / causal bookkeeping
├── background species NEAT evolution
├── checkpoint / trace / metrics
└── runtime orchestration

GPU side
├── protagonist observation encoder
├── protagonist memory query encoder
├── protagonist behavior fusion trunk
├── protagonist motor core
├── protagonist action heads
└── future world model / large dense predictors
```

关键点：

- 显式 memory bank 本体仍主要留在 CPU
- 但**memory query 后形成的 dense cognition 计算**优先放 GPU
- background species 暂时继续留在 CPU
- 先做 protagonist 路线，不一开始就改整个仓库

---

## 4. 计算路线分阶段

## 4.1 Phase C0 · 先做 profiling 基线

### 目标

在真正做大改前，先知道当前每代时间花在哪里。

### 必做采样项

按 generation / episode / substep 统计：

- world advance time
- protagonist decision time
- background species decision time
- capability resolution time
- memory write time
- trace/logging time
- metrics aggregation time
- reproduction/evolution time

### 推荐输出

- `performance_breakdown.csv`
- `phase_timing_summary.json`

### 为什么必须先做

后续 GPU/CPU 路线不能凭感觉做。

如果不先量化：

- 可能把不是真瓶颈的部分 GPU 化
- 可能忽略 logging / metrics / trace 的真实成本
- 可能高估 dense brain 的代价，或低估 world logic 的代价

---

## 4.2 Phase C1 · 先把 CPU 侧并行吃满到合理程度

这是第一优先级，不依赖 GPU。

## 4.2.1 先 profile，再扩大 episode / agent 两层并行的可利用空间

现在 episode 数仍会限制外层并行上界，但已经不是唯一因素。

可选方向：

- 根据 profiling 调整 `episodes_per_generation`
- 继续利用/调优 `compute.agent_decision_threads`
- 把 generation 内评估任务进一步分片
- 将 protagonist 与 background 的不同评估批次更灵活调度

注意：

- 不是盲目把 episode 数开大
- 也不是假设 `agent_decision_threads` 一开就一定吃满机器
- 要兼顾训练信号质量、lock/merge 成本与 wall-clock 成本

## 4.2.2 做 episode 内的“决策并行、状态提交串行”

最现实的 CPU 增益路线是：

```text
世界状态冻结为当前 substep snapshot
    -> 并行计算 agent decisions
    -> 统一提交阶段提交动作
    -> world advance / event resolve
```

原因：

- world mutation 仍集中处理，避免竞争写
- dense decision 计算能切出并行
- 对当前架构侵入小于“全 world 并行”

优先对象：

- protagonist agents
- 再考虑 background agents

## 4.2.3 metrics / trace / logging 异步化

如果 profiling 发现：

- trace recorder
- csv 写入
- 大量日志拼接

占比较高，那么需要：

- ring buffer
- writer thread
- generation 结束时 flush

这样可以减少训练主循环被 I/O 拖慢。

## 4.2.4 capability 级局部并行

不是所有 capability 都适合并行。

但以下类型较适合分阶段并行：

- perception aggregation
- dense brain forward
- 某些无共享写冲突的 local scoring

不建议一开始就并行的：

- 会直接写 world shared state 的 capability
- 会即刻触发复杂事件链的部分

## 4.2.5 CPU 目标图景

在不改 GPU 的前提下，CPU 最终应至少做到：

- 不再只靠 `episodes_per_generation` 决定并行度
- protagonist decision 计算能跨 agent / batch 分摊到更多核心
- trace / metrics 不阻塞主训练循环
- 16C / 32T 利用率明显提升

---

## 4.3 Phase C2 · Mixed GPU：只先 GPU 化 protagonist dense brain

这是最推荐的 GPU 第一阶段。

## 4.3.1 GPU 化范围

只 GPU 化以下 dense 组件：

- `ObservationEncoder`
- `MemoryQueryEncoder`
- `BehaviorFusionTrunk`
- `MotorCore`
- `ActionHeads`

明确不在第一阶段 GPU 化的：

- world simulation
- EventBus
- Spatial/Episodic/Social/Goal bank 本体
- background species evolution
- checkpoint / trace / metrics

## 4.3.2 核心思想

每个 substep：

```text
CPU 收集 protagonist observations + memory queries
    -> 打包成 batched tensor
    -> GPU 执行 protagonist dense forward
    -> 返回动作 logits / continuous controls / write hints
    -> CPU 应用 capability / world mutation / memory writes
```

这个路线的优势：

- 能利用 GPU 最擅长的 batched dense compute
- 不要求整世界迁移到 GPU
- 与显式 memory 系统兼容
- 更适合渐进接入

## 4.3.3 为什么先只做 protagonist

因为 protagonist 是你最关心的大脑。

而且 protagonist：

- 数量可控
- 结构更容易统一
- 后续参数量最大
- 与 memory system 强绑定

相比之下 background species：

- NEAT 拓扑异构
- GPU 化复杂度更高
- 当前不必和 protagonist 一起首批迁移

## 4.3.4 数据布局要求

如果未来要 mixed GPU，现在就要逐步改成对 batch 友好的布局。

推荐方向：

- observation buffer 改为 SoA / contiguous batch layout
- memory query 输出固定维度向量
- protagonist dense weights 统一 layout
- 尽量避免每个 agent 单独小对象式调用 GPU

### 不推荐的做法

- 每个 agent 单独发一次 GPU kernel
- 每层都 CPU/GPU 来回切很多次
- 把高频细碎逻辑放在 CPU/GPU 之间反复抖动

## 4.3.5 RTX 5060 8GB 的现实约束

8GB 显存不是无限的，但做 protagonist mixed GPU 是够用的，前提是：

- 不把整个 world state 搬上去
- 不同时塞太多大型模型
- batch 尺寸受控
- 推理精度可灵活选 `fp32 / fp16`

所以这张卡更适合：

- protagonist dense inference
- 后续 world model

不适合一上来全量 world sim + 全 species 全部 GPU。

---

## 4.4 Phase C3 · World Model 与大模块优先吃 GPU

在 protagonist dense brain GPU 化后，下一批最适合 GPU 的通常不是 world logic，而是：

- world model predictor
- observation encoder
- curiosity / novelty auxiliary heads
- future latent dynamics modules

原因很简单：

- 这些模块 dense、规则、可 batch
- 比 OOP world sim 更适合 GPU

所以后续 GPU 扩展顺序建议是：

1. protagonist dense brain
2. world model / predictor
3. 再评估是否值得继续推进更多模块

---

## 5. 何时启动 mixed GPU

不是今天就开做，而是满足以下条件后启动：

## 5.1 架构条件

- `Memory Phase A` 已完成
- `ProtagonistBrainV3` 的第一版接口稳定
- memory query 维度和 action head 结构不再频繁改

## 5.2 性能条件

- profiling 显示 protagonist dense inference 成为主要耗时之一
- CPU 并行已做过一轮优化，但仍不够
- 单 agent dense 参数量至少进入 `0.5M ~ 1M+`

## 5.3 训练规模条件

- protagonist 数量 >= `24~64`
- `episodes_per_generation` 有提升
- episode 持续时间较长
- 行为任务更复杂，需要更大脑而不是更小脑

如果这些条件没到，过早 GPU 化会让重构成本大于收益。

---

## 6. CPU 与 GPU 的职责边界

这是后续最重要的架构纪律。

## 6.1 CPU 负责

- 世界状态
- capability 结算
- collision / resource / build / combat
- EventBus
- 显式记忆写入
- social / episodic / causal bookkeeping
- trace / checkpoint / metrics
- evolution / mutation / crossover / speciation

## 6.2 GPU 负责

- protagonist dense cognition forward
- observation encoding
- memory query fusion
- motor core dynamics
- action head inference
- 后续 world model / predictor

## 6.3 边界通信原则

每个 substep 尽量保持：

- 一次 observation batch 上传
- 一次 protagonist forward
- 一次动作结果回传

而不是让 CPU/GPU 每层、每小步骤都来回同步。

---

## 7. 需要提前做的工程准备

即使现在不开始 mixed GPU，也应该提前做这些准备。

## 7.1 统一 protagonist dense brain 的数据结构

后续 `ProtagonistBrainV3` 最好不要继续以“很多零散小 vector + agent 对象内分散访问”的方式无限堆叠。

应尽量逐步靠近：

- 固定 layout
- 连续内存
- batch-friendly buffers

## 7.2 把 memory query 固定为清晰接口

后续 mixed GPU 非常依赖：

- CPU 显式 memory -> fixed-size query vector

如果 query 接口不稳定，GPU 路线会不断返工。

## 7.3 增加 profiler / observer

必须长期输出：

- protagonist_forward_ms
- background_forward_ms
- world_step_ms
- event_bus_ms
- memory_write_ms
- trace_ms
- io_ms

## 7.4 注意 checkpoint 与 replay 的独立性

mixed GPU 不应该破坏：

- checkpoint schema
- replay/trace 格式
- 单线程/CPU fallback 路线

最好保持：

- CPU path 是 canonical reference
- GPU path 是 acceleration backend

---

## 8. 不建议做的事情

以下路线不推荐：

### 8.1 不要先全量 GPU world sim

成本太高，收益不确定，且会严重打断当前机制迭代。

### 8.2 不要为了适配 GPU 反过来把记忆系统做小

这和项目方向相反。

正确路线是：

- 先按认知需求把主角脑做对
- 再让算力路线跟上

### 8.3 不要让 background species 和 protagonist 首批一起 GPU 化

这会把复杂度抬得过高。

### 8.4 不要在没有 profiling 的情况下凭感觉改算力架构

必须先量化，再开刀。

---

## 9. 推荐的实际执行顺序

## 9.1 近期

- 完成 `Memory Phase A`
- 继续 CT-2 / CT-3，减少残余离散动作语义
- 增加 profiling 输出

## 9.2 中期

- 做 `ProtagonistBrainV3` 第一版
- 主行为脑扩到 `400k ~ 800k`
- CPU 并行从 episode 级扩大到 agent decision / batch 级

## 9.3 中后期

- protagonist dense brain mixed GPU
- 主角数量和复杂度继续上探
- 同时保持 CPU world sim 稳定

## 9.4 更后期

- world model / predictor 上 GPU
- 视收益决定是否继续推进更多模块 GPU 化

---

## 10. 最终目标画面

当这条路线走通后，训练计算结构应该变成：

```text
CPU：世界在跑，事件在流，记忆在写，合作/因果在记账
GPU：主角脑在大批量地思考、融合记忆、生成持续动作意图
```

这时候：

- 主角可以有更大的脑
- 记忆系统可以更深
- 世界仍保持可解释、可追踪、可检查点恢复
- 计算设备也能真正被吃起来

---

## 11. 一句话总纲

后续 protagonist 路线的算力方向应该定为：

**先把 Memory V2 接活，再把主角脑结构化扩容；CPU 侧把并行从 episode 级提升到更能吃满 16C/32T，GPU 侧只优先承接 protagonist dense cognition 与后续 world model，而不是现在就强行全项目 GPU 化。**
