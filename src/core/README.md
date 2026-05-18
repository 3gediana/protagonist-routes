# core

> 纯逻辑层。**永远不依赖 viz、不依赖 runtime**。

这个目录是项目的"内核"。所有进化、世界、个体、神经网络逻辑都在这里。viz 模块只能通过订阅事件来读取这里的数据，不能直接调用任何修改方法。

## 子目录速览

| 目录 | 职责 | 实现阶段 |
|---|---|---|
| `interfaces/` | 所有抽象接口（IBrain / IWorld / IAgent...） | M0 |
| `events/` | 事件总线 EventBus | M0 |
| `config/` | TOML 配置加载 | M0 |
| `logging/` | 日志包装（spdlog） | M0 |
| `genome/` | 基因数据结构 | M1 → 扩展到 M3 |
| `brain/` | 神经网络前向推理 | M1（fixed）→ M3（NEAT+CTRNN） |
| `agent/` | 个体 | M1 |
| `world/` | 世界与多层 layer | M1 → M5 加 terrain |
| `evolution/` | 选择、交叉、变异 | M1 → M3 加 NEAT speciation |
| `perception/` | 感知组件（IPerception 实现） | M1 → M5 加 terrain |
| `capability/` | 能力组件（IAgentCapability 实现） | M1 |
| `reward/` | 奖励组件（IRewardComponent 实现） | M1 |
| `observer/` | 观测者（IObserver 实现，订阅事件） | M1 |

详细架构见 [`docs/ARCHITECTURE.md`](../../docs/ARCHITECTURE.md)。

## 铁律

1. **不引用 viz/ 任何头文件**
2. **不引用 runtime/ 任何头文件**
3. **跨模块通信优先走 EventBus**
4. **新增功能优先通过现有扩展点（IWorldLayer / IPerception / IAgentCapability / IRewardComponent / IObserver）**
