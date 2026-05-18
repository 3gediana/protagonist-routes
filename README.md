# Neural Eco

<!-- AI-AGENT-NOTE: 如果你是 AI 助手，请先读 AGENTS.md 再做任何工作。 -->

> 一个用 C++ 编写的神经进化生态箱：让一群带微型神经网络的"生物"在你设计的世界里自然选择、变异、进化，**真实存在于系统中**，不是 LLM 扮演。

## 这是什么

每个个体（Agent）有一个微型神经网络作为"大脑"。世界提供生存压力（食物、地形、捕食者），自然选择决定谁的基因传给下一代。**网络结构本身也会进化**（NEAT 算法）——从一个最简单的输入直连输出的网络开始，逐代长出隐藏神经元和复杂连接。

你扮演的不是玩家，是"造物主":只设计世界的规则和奖励，**不设计任何具体行为**。趋食、避险、合作、围猎、信号沟通——这些行为如果出现，全部是进化自己找到的。

## 项目定位

- **课程类型**：C++ 大一课程设计
- **核心评价维度**：OOP 深度、AI 真实性、涌现可观察性
- **运行环境**：Windows + RTX 5060 8G（CPU 多线程为主，GPU 用于可视化）
- **AI 协作**：AI 助手负责代码生成，你负责设计、调参、奖励函数

## 文档入口

| 文档 | 说明 | 优先级 |
|---|---|---|
| [PROGRESS.md](PROGRESS.md) | **项目当前进度** —— AI 助手接力开发用 | 🔴 必读 |
| [AGENTS.md](AGENTS.md) | AI 助手协作手册 | 🟡 AI 必读，人可跳过 |
| [docs/ROADMAP.md](docs/ROADMAP.md) | 整个项目的开发路径 | 🔴 必读 |
| [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) | 顶层架构与模块设计 | 🔴 必读 |
| [docs/DESIGN_DECISIONS.md](docs/DESIGN_DECISIONS.md) | 为什么这样设计（决策记录） | 🟡 推荐 |
| [docs/GLOSSARY.md](docs/GLOSSARY.md) | 术语表（NEAT/适应度/物种等） | 🟢 参考 |
| [docs/baseline/](docs/baseline/) | 基线阶段 M0-M5 详细文档 | 🔴 按阶段读 |
| [docs/modules/](docs/modules/) | 可选模块卡片库（F-*） | 🟡 按需查 |

## 开发哲学：基线 + 模块仓库

**未来不确定**。你现在不知道最终会加多少功能、加什么功能。所以架构是**插件化**的，开发节奏是**基线 + 模块仓库**：

### 🟦 基线（必做，按顺序）
```
M0  脚手架        → 接口、事件总线、配置、日志、插件框架
M1  连续 2D MVP   → 浮点世界 + 固定拓扑 NN + 简单 GA，跑通进化
M2  可视化接入    → raylib 渲染 + 适应度曲线
M3  NEAT 升级     → 替换 Brain，拓扑进化 + 物种分化
M4  多线程加速    → headless 训练池 + 异步个体决策
M5  真实 2.5D 地形 → 高度图 + 山洞 + 移动/视野受影响 + 等距渲染
```

### 🟨 模块仓库（按兴趣选）
- 🤝 交互类: F-COMM 信号 / F-COOP 合作 / F-FIGHT 对抗 / F-PRED 捕食 / F-TOOL 工具 / F-LEARN 终身学习
- 🌍 环境类: F-DAY 昼夜 / F-SEASON 季节 / F-DISEASE 疾病 / F-RESOURCE 资源动态 / F-CAVE 洞穴
- 🔬 观测类: F-TREE 物种树 / F-HEAT 热力图 / F-FORK 平行宇宙 / F-STAT 高级统计
- 🎬 演示类: F-GOD 上帝按钮 / F-LLM 解说员 / F-REPLAY 回放 / F-COMPARE 对照 / F-CURRICULUM 课程学习
- ⚙️ 算法类: F-NOVELTY 新颖性 / F-COEVO 协同进化 / F-NSGA 多目标

## 项目结构

```
neural-eco/
├── docs/                  # 所有文档
│   ├── ROADMAP.md
│   ├── ARCHITECTURE.md
│   ├── DESIGN_DECISIONS.md
│   ├── GLOSSARY.md
│   ├── baseline/          # 基线阶段 M0-M5 详细文档
│   ├── modules/           # 可选功能模块卡片库（F-*）
│   └── adr/               # 架构决策记录（增量决策时新增）
├── src/
│   ├── core/              # 核心逻辑（与渲染解耦）
│   │   ├── interfaces/    # 抽象接口（IBrain / IWorld / IAgent...）
│   │   ├── genome/        # 基因 / 网络结构
│   │   ├── brain/         # 神经网络推理
│   │   ├── evolution/     # 进化算法（选择/交叉/变异）
│   │   ├── world/         # 世界 / 环境
│   │   ├── agent/         # 个体
│   │   ├── observer/      # 观测者（订阅事件）
│   │   ├── events/        # 事件总线
│   │   ├── config/        # 配置加载
│   │   └── logging/       # 日志
│   ├── viz/               # 可视化（依赖 core，反之不成立）
│   ├── runtime/
│   │   ├── headless/      # 无渲染训练运行时
│   │   └── interactive/   # 交互式演示运行时
│   └── main.cpp
├── tests/
├── configs/               # TOML 配置文件
├── data/
│   ├── snapshots/         # 平行宇宙存档（F-FORK 模块）
│   ├── runs/              # 训练记录 CSV
│   └── replays/           # 回放数据
├── extern/                # 第三方依赖（vcpkg / FetchContent）
└── scripts/               # Python 数据分析脚本
```

## 立即开始

1. 读 `docs/ROADMAP.md` 理解整体节奏与设计哲学
2. 读 `docs/ARCHITECTURE.md` 理解模块边界与扩展点
3. 进入 `docs/baseline/M0_scaffold.md`，开始第一阶段

每个 baseline 阶段文档与 module 卡片都明确标出：
- ✅ **AI 助手帮你完成的部分**（代码骨架）
- 🧠 **你需要自己思考/补充的部分**（设计、调参、奖励函数）
- 🎯 **验收标准**（怎么知道这阶段完成了）
- 🔌 **接入点**（这个东西通过哪个接口插入主系统）
