# ADR-0006 · protagonist 记忆板块采用标准 DNC，而不是 MT-DNC

- **状态**：已采纳
- **日期**：2026-05-20
- **背景**：用户明确要求把 protagonist 的记忆板块做成完整机制，不做轻量替代版，因为记忆是否存在、是否好用，会直接体现在 agent 行为复杂度和后续可视化效果上。此前 route 中主角记忆主要由 `MemoryBank + Spatial/Episodic/Social/Goal` 几块组成，能存信息，但检索器和寻址方式过于静态。我们需要引入真正可寻址、可写入、可按时间链追踪的外部记忆。与此同时，也评估了更重的 `MT-DNC` 路线：它会进一步增加参数量、控制复杂度、训练难度和调试成本，而当前 protagonist route 还没有把标准 DNC 的行为边界摸清。

---

## 决策

protagonist 记忆板块采用**标准 DNC（Differentiable Neural Computer）**作为主记忆骨架，当前不采用 `MT-DNC`。

### 当前落地范围

1. **外部记忆骨架使用标准 DNC**
   - memory matrix
   - content lookup
   - allocation weighting
   - erase-write
   - temporal links
   - multi-read heads
   - usage / free gates

2. **主角大脑控制器改为 DNC controller**
   - `ProtagonistBrain` 读取 `DNC read vectors`
   - 隐层输出 `interface vector`
   - 再解码为 DNC 的 write/read/control interface

3. **拓扑参数正式进入 genome**
   - `memory_cells` 作为 DNC `word_size`
   - 新增 `memory_slots`
   - 新增 `read_heads`

4. **现有 Specialized Memory Banks 暂时保留**
   - `SpatialMemoryBank`
   - `EpisodicMemoryBank`
   - `SocialMemoryBank`
   - `GoalMemory`

   它们继续承担显式语义记忆和 memory cognition 输入，作为标准 DNC 上层的“可解释显式记忆层”。

5. **MT-DNC 延后**
   - 只有在标准 DNC 已跑通、trace 已经显示其瓶颈后，才考虑更重的 MT-DNC 变体。

---

## 为什么这样做

### 1. 标准 DNC 已经覆盖当前真正缺口

当前最缺的不是“更大的记忆”，而是：

- 能按内容找回
- 能决定往哪写
- 能沿时间顺序追踪
- 能同时读多条记忆
- 能管理已用/可释放槽位

这些都是标准 DNC 的核心能力。

### 2. MT-DNC 现在的边际收益不够大

相较标准 DNC，MT-DNC 会进一步增加：

- 参数量
- 搜索空间
- 训练不稳定性
- 调试难度
- 存档/恢复/进化拓扑兼容成本

在当前阶段直接上 MT-DNC，会让我们更难判断：

- 到底是“记忆机制真的变强”
- 还是“模型只是更大更复杂”

### 3. 用户要的是“记忆板块做扎实”，不是“机制名头越大越好”

用户真正关心的是：

- 行为里能明显看出记忆
- 记忆让主角更像真的在理解世界
- 后面做可视化时有足够强的展示价值

标准 DNC 已经足以支撑这一层目标。

---

## 代价与补偿

### 代价

- protagonist brain/genome/evolution/checkpoint 全链都要同步升级
- 参数量比旧 `MemoryBank` 明显增加
- 后续 curriculum / reward / trace 也必须针对 DNC 重新设计

### 补偿

- 记忆寻址能力真正升级，不再只是“固定读几条显式记忆”
- 后续做 replay / trace / 可视化时，可以展示 read/write/usage/temporal-link 这些真实记忆动态
- 为后面空间/情节/社会/目标记忆和 world understanding 机制提供统一底座

---

## 对现有代码的影响

### 已直接影响

- `routes/protagonist_ecology/brain/ProtagonistBrain.*`
- `routes/protagonist_ecology/brain/ProtagonistGenome.*`
- `routes/protagonist_ecology/runtime/ProtagonistEvolution.*`
- `routes/protagonist_ecology/runtime/BootstrapPopulation.cpp`
- `routes/protagonist_ecology/runtime/ProtagonistEcologyRuntime.cpp`
- `routes/protagonist_ecology/runtime/TraceRecorder.cpp`

### 新增核心文件

- `routes/protagonist_ecology/brain/DNCMemory.h`
- `routes/protagonist_ecology/brain/DNCMemory.cpp`

---

## 后续动作

1. 把 `MemoryCognitionPerception` 改造成真正利用 DNC 检索结果的输入层
2. 设计 retrieval-targeted curriculum，让“不检索就过不了关”
3. 为 DNC 增加更细的 trace / replay 指标
4. 只有在标准 DNC 的瓶颈被明确观察到之后，才重新评估是否升级到 `MT-DNC`
