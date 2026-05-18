# M3 · NEAT 升级

> 项目核心卖点。把 M1 的固定拓扑替换成可进化拓扑的 NEAT。
> **接口压力测试 #1**：M0 设计的 `IBrain` 和 `IGenome` 接口能否承载完全不同的实现？

---

## 目标

1. 实现完整 NEAT 算法（拓扑变异 + 物种分化 + 创新编号）
2. 同时实现 CTRNN 递归神经元（NEAT 允许递归边）
3. 替换 M1/M2 中的 `FixedTopologyGenome/Brain`，**进化循环代码不动**
4. 看到拓扑随代际增长、物种树形成

---

## 前置条件

- M1 机制验收到位；可视化按 [ADR-0005](../adr/0005-defer-visualization-until-mechanics-stabilize.md) 延后
- 你已读完 NEAT 原论文（Stanley & Miikkulainen 2002）—— 不需要懂细节，知道大致思想即可

---

## 关键产出

### NEAT 数据结构

```
src/core/brain/neat/
├── NodeGene.h               神经元基因 (id, type[INPUT/HIDDEN/OUTPUT/MODULATORY], time_constant)
├── ConnectionGene.h         连接基因 (in, out, weight, enabled, innovation_id)
├── NeatGenome.h / .cpp      NEAT 基因组（含拓扑 + 权重 + 元参数）
├── NeatBrain.h / .cpp       NEAT 网络的前向推理（支持递归边、CTRNN 动力学）
├── InnovationTracker.h      全局创新编号管理（跨代追踪结构创新）
├── mutations/
│   ├── AddNodeMutation.cpp     在某条连接中插入新神经元
│   ├── AddConnectionMutation.cpp  添加新连接
│   ├── ToggleConnectionMutation.cpp 启用/禁用连接
│   ├── WeightPerturbMutation.cpp  权重高斯扰动
│   └── WeightResetMutation.cpp    权重重置
└── crossover/
    └── NeatCrossover.cpp    基于创新编号的交叉
```

### CTRNN 集成（ADR-0002 决定）

每个 `NodeGene` 有 `time_constant` 字段（τ）。前向推理时：

```
对于隐藏神经元 i:
  net_input_i = Σ weight_ji × output_j        // 当前时刻所有输入的加权和
  hidden_i(t+dt) = hidden_i(t) + (dt/τ_i) × (-hidden_i(t) + activation(net_input_i))
  output_i = activation(hidden_i(t+dt))
```

**τ 也是可进化的基因**——不同神经元有不同时间常数，进化决定。
默认 τ ∈ [0.1, 5.0]，可变异。

实现备注：
- 递归 / CTRNN 更新使用**双缓冲状态**，每步只读上一时刻状态，写下一时刻状态
- 不做递归 DFS 式求值，避免递归边死循环
- 外部资料表明，相比朴素 forward Euler，至少要保证时间积分**数值稳定**；当前项目初版可先用离散积分 + 双缓冲，后续如有必要再升级为更稳的指数 Euler 近似

### 物种分化

```
src/core/evolution/speciation/
├── Speciation.h / .cpp           种群按 compatibility 距离聚类
└── CompatibilityDistance.cpp     δ = c1·E/N + c2·D/N + c3·W̄

E: 多余基因数 (excess genes)
D: 不一致基因数 (disjoint genes)
W̄: 共享基因的平均权重差
```

代表元（representative）选择：每个物种保留一个上一代代表元，新一代用代表元做 compatibility 比较。

实现备注：
- 创新号匹配是 crossover 和 distance 的核心，**连接基因按 innovation_id 对齐**
- 同一代内出现相同结构突变时，应该复用同一创新号；因此 `InnovationTracker` 除了全局递增计数，还需要维护**本代 innovation cache**，每代结束清空即可
- compatibility threshold 后续优先做成**可动态调节**，目标维持 3-10 个物种，而不是死写一个固定值

### 适应度共享（Fitness Sharing）

物种内做选择，避免某个新拓扑被立刻淘汰：
```
fitness_adjusted_i = fitness_i / |species_i|
```

### 替换 M1 调用点

```
// 旧代码（M1）
auto genome = std::make_unique<FixedTopologyGenome>();
auto brain = std::make_unique<FixedTopologyBrain>(genome);

// 新代码（M3）
auto genome = std::make_unique<NeatGenome>(input_dim, output_dim);
auto brain = std::make_unique<NeatBrain>(genome);

// 进化循环代码保持不变 → 这就是 IBrain/IGenome 接口设计的胜利
```

如果这一步要改进化循环代码，**说明 M0 接口设计有问题，回头修 M0**。

### 物种树观察者（基础版，完整版在 F-TREE）

`SpeciesObserver` 订阅事件，把每个物种的"出生代-死亡代-代表网络"记录到 `data/runs/<ts>/species_log.jsonl`，为 F-TREE 模块提供数据源。

---

## 三档完美标准

### MVP ✅⛳⛳
- [ ] NEAT 编译通过，单元测试覆盖核心数据结构
- [ ] 替换后能跑 30 代不崩溃
- [ ] 物种数量不为零（说明分化机制工作）

### 完整 ✅✅⛳
- [ ] 跑 200 代不崩溃，无内存泄漏
- [ ] 至少观察到 add_node 和 add_connection 各 5 次以上
- [ ] 物种数稳定在 3-15 之间（不收敛成 1 个物种也不爆炸到 50+）
- [ ] M1 / M2 的进化循环代码 0 修改（接口压力测试通过）

### 完美 ✅✅✅
- [ ] **跑 200 代后平均拓扑复杂度（连接数）从 ~6 增长到 ≥ 20**
- [ ] **能复现 M1 的趋食行为**（即 NEAT 不会比固定拓扑差）
- [ ] 至少观察到一次"物种灭绝"和一次"新物种诞生"
- [ ] CTRNN 时间常数有进化趋势（统计显示种群平均 τ 不再随机）
- [ ] 调参经验记入 `docs/experience_log.md`：compatibility 阈值、变异率、最终参数
- [ ] git tag `baseline-M3-done`

---

## 🧠 你的补充点

| 项 | 你需要决定 |
|---|---|
| **变异率分配** | add_node : add_connection : weight_perturb 比例 —— 默认 3% : 5% : 80%，影响进化速度和稳定性 |
| **compatibility 系数** | c1, c2, c3 默认 1.0 / 1.0 / 0.4。你可以调来改变物种粒度 |
| **物种数量目标** | 默认 3-10。这是判断 compatibility 阈值是否合适的关键 |
| **CTRNN τ 范围** | 默认 [0.1, 5.0]。范围窄网络反应快但缺记忆，宽则反应慢但有记忆 |
| **是否启用调节神经元** | M3 阶段先**不启用**（默认所有节点 type 是 NORMAL），F-NEUROMOD 模块再开 |

---

## 🔌 接入点

- F-TREE 模块用 `SpeciesObserver` 数据画完整物种树
- F-NEUROMOD 模块在 NEAT 变异中加入 `add_modulatory_node` 操作
- F-LEARN 模块在 NeatBrain 添加生命周期内学习钩子
- M5 地形会扩展感知输入维度，**NEAT 必须能动态适应输入维度**——M3 设计 NeatGenome 时确保 input_dim 不写死

---

## ⚠️ 易踩坑

1. **创新编号竞态**：多线程下分配新创新编号时若用全局计数器要加锁
2. **物种内只剩 1 个**：fitness sharing 让单成员物种被惩罚成 0，物种立刻灭绝。**改为 fitness sharing 只在物种 size ≥ 2 时启用**
3. **递归边死循环**：CTRNN 前向推理如果朴素实现可能无限递归。正确做法是按 dt 离散积分，每个神经元用上一时刻的 output
4. **物种爆炸**：compatibility 阈值太低，每个个体自成物种。检查方法：跑 50 代后看 species_log，如果物种数 > 30，调高阈值
5. **拓扑停滞**：变异率太低 → 50 代了网络还是输入直连输出。检查 add_node 触发次数

---

## 验证脚本

```python
# scripts/analyze_neat.py
import pandas as pd
import json

# 读 species_log.jsonl
# 画物种数量随代际变化
# 画平均拓扑复杂度（连接数）随代际变化
# 画 fitness 随代际变化（按物种染色）
```

完美档位需要这三张图都健康。

---

## 下一阶段

→ [M4 · 多线程加速](M4_async.md)
