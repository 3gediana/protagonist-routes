# 前沿研究 → 本项目桥梁（2026-10-02 调研）

调研范围：CTM / 液态神经网络 / CVPR 2026 Mapping Networks / 虚拟果蝇全脑控制 / 神经科学可移植机制。
结论先行：**两条身份纠正 + 四个可移植机制**。

## 0 · 身份纠正（重要，防以讹传讹）

3. **用户所指的英伟达工作 = 开源虚拟物理机器人研究族（GitHub）**，不是果蝇脑仿真。核心成员：
   - **Isaac GR00T N1/N1.5/N1.6**（GTC 2025 起，github.com/NVIDIA/Isaac-GR00T + HuggingFace 数据）：
     世界第一个开源通用人形机器人基础模型（VLA）。**架构核心 = System 2 / System 1 双速认知**：
     慢系统（视觉-语言骨干，做推理与任务分解）+ 快系统（扩散动作头，高频反应控制）。
   - **Newton 物理引擎**（NVIDIA + Google DeepMind + Disney Research 共建，Linux 基金会托管，
     2025-03 发布 2025-09 beta 入 Isaac Lab）：GPU 加速、可微物理、为机器人学习特化；配套
     MuJoCo-Warp 号称 70× 加速。
   - **Isaac Lab**（开源机器人学习框架）：海量并行 GPU 环境训练、域随机化、**自动课程工作流**
     （抓取示例：从简单任务起步，逐步加重力/摩擦/物体重量复杂度——与传送课程同构，行业已验证）。
   - 其余：GR00T Blueprint（合成操作动作生成）、Cosmos 世界基础模型（生成多样性训练数据）。
   → 对本项目最值钱的是 **System1/System2 双速认知**（见 §3.6）与 Isaac Lab 自动课程对本项目
   teleport 课程 + 退火（C 刀）路线的背书。

1. **CVPR 2026 "Mapping Networks" 不是认知地图论文**（arXiv 2602.19134, Sen & Mukherjee,
   pp.36215-36223）。它是**权重空间参数化**：把大网络的高维权重换成"紧凑可训练隐向量 + 可学习的
   隐→权重映射"（Mapping Theorem + Mapping Loss 保证局部可解性），假设"训练好的权重住在光滑低维
   流形上"，主收益是抗过拟合。→ 对我们的价值在 **PE 的进化基因组**（见 §3.1），不在空间记忆。
2. **全脑连接组控制虚拟果蝇的论文不是英伟达的**（arXiv 2602.17997, FlyGM, 清华 Sui 组 2026-02：
   把成年果蝇全脑连接组直接实例化为图神经控制器，在 flybody/MuJoCo 里 RL 训练，样本效率优于同规模
   图/非图基线）。英伟达相关工作是 GPU 加速的果蝇脑仿真平台（Sheffield/Columbia, Neurokernel 系）
   和 Isaac/GR00T 仿真栈——即"这些仿真跑在英伟达栈上"，控制器结构贡献来自清华/DeepMind-HHMI(flybody,
   Nature 2025-04)。机制教训不变：**结构先验（连接组/分层图）当控制器骨架，RL 只学参数**。

## 1 · CTM（Sakana AI, NeurIPS 2025, arXiv 2505.05522, 代码开源）

机制三件套：①内部 tick 时间轴（与输入解耦，每个神经元自己决定想多少拍）；②神经元级模型 NLM
（每个神经元带专属权重处理自己的激活历史）；③**神经同步矩阵**（神经元后激活时间序列两两时间相关）
直接作为表征读出。关键先例：**论文自己给出了 PPO 集成配方**——obs backbone → 与 CTM 状态拼接 →
固定 N 个内部 tick → 输出神经元间同步 → actor/critic 头。且有"确定性早退"（简单样本 <10 tick 解决，
复杂的用满 50 tick），图像/迷宫/排序/QA/RL 全验证过。

## 2 · 液态神经网络 / CfC（MIT Hasani 线，AAAI 2021 起，2026=LFM2 时代）

核心数学（CfC 闭式连续时间神经元）：每神经元一个 ODE z' = −α⊙z + α⊙β，三个门
α=softplus(时间常数门)、β=tanh(目标态门)、γ=σ(输出门)，闭式积分**无求解器**，比神经 ODE 快
10~10^5 倍。卖点：每神经元自带时间常数（不同输入自适应动力学）、样本效率、抗分布漂移（无人机
穿越未见过环境的经典结果）、**LSTM/GRU 直接平替位置**。2026 年 LFM2 是语言模型混合体（卷积门+GQA），
对控制项目真正相关的仍是 CfC 单元本身。

## 3 · 可移植机制（按性价比排序）

### 3.1 DP-CTM-lite：内部思考拍（旗舰候选，`--ctm-ticks N`）
CTM 论文的 PPO 配方直接对应我们的 ActorCritic：GRU 输出 h 后，不再直接进头，而是跑 N 个内部
tick 的轻量 NLM 循环（逐神经元小线性 + 元胞历史），再用神经元历史的同步特征（两两相关降维后的小
向量）读出到各头。**挂点**：`PPO.cpp forward_step`，`ActorCriticImpl` 加 ctm 分支（始终注册、
零初始化读出、tolerant 加载，flag off 字节等价——同 ψ/D1 手术惯例）。**验证什么**：深科技链是
"多步犹豫"型决策（走到矿边→想一下→按合成），内部拍是否提升链解锁率；顺带白得可解释性（同步模式图）。
**风险**：内部拍 ×每步是纯计算开销，N 要小（2~4）；实现是简化版（完整 CTM 的逐神经元历史序列在
libtorch 里太贵，用滚动累积近似）——文档里如实标注。

### 3.2 DP-CfC：时间常数 GRU（`--cfc`）
CfC 单元数学只有三个门+闭式更新，libtorch 手写一个 CfCCell 完全可行（比 CTM-lite 还简单）。
**挂点**：`--cfc` 时 GRUCell 换成 CfCCell（参数量相近），tolerant 加载；或混合（GRU 主干 +
CfC 侧链）。**验证什么**：D8 发现的"技能预算竞争"本质是时间尺度错配（生存秒级 vs 科技链分钟级），
每神经元时间常数正是为多时间尺度设计的；观察它是否缓解配比互斥。

### 3.3 PE-MappingGenome：隐向量基因组（CVPR 2026 论文的进化线用法，`genome.mapping_latent = K`）
NEAT 基因组不再直接编码全部权重，而是编码 K 维隐向量 + 一个小型映射器（映射器可进化或固定随机）
生成权重。**为什么该试**：直接进化原始权重 = 在高维粗糙地形上爬山（我们 fitness 曲线锯齿的根因之一）；
低维光滑流形搜索是 Mapping Theorem 的主张，且抗过拟合对 30 代小种群尤其对口。**挂点**：
ME-NEAT 基因组结构 + 输出权重生成处，config 开关 default off。**验证什么**：相同代数下 ME-NEAT
coverage/qd 的爬升速度与稳定性。

### 3.4 PE-E2：emit-on-discovery（已立项，非本次新招）
信号回应为零的病根是发射语义（饱和、无上下文、荒野里喊）——28m halo 实验已排除半径。改造：
找到食物才 emit，发射位置与资源相关，listener 靠近有真实收益。config 开关
`reward.signal_emit_on_discovery` default off。

### 3.6 DP-双速认知（GR00T N1 架构的映射，设计级洞察）
GR00T N1 的 System2（慢推理）/System1（快反应）分野，恰好是 D8"技能预算竞争"的架构级解法：
生存反应（吃/喝/躲）本来就该是**快系统**的本能，不该和科技链决策（慢、需思考）抢同一套神经资源。
本文件的 CTM-lite（§3.1，内部拍=慢系统）与 CfC（§3.2，时间常数=快系统多尺度）**天然构成这个
双系统的两半**——闲时任务把它们实现为两个独立开关后，"双速"只是把两者的输出按 GR00T 的方式
组合（慢系统产出建议/门控，快系统执行），留作 D10 级别的组合刀。Isaac Lab 的自动课程工作流同时
背书了 teleport 课程 + 退火（C 刀）的形状：先简单后复杂、逐步撤辅助。

### 3.5 落选与理由（备查）
- FlyGM 全脑连接组图先验：PE 的 NEAT 图已是结构先验，换果蝇连接组骨架工程量大、叙事违和（我们的
  生物不是果蝇）——记为远期。
- 多巴胺 RPE 门控学习率（neuromodulated LR）：我们已有 per-stream advantage 归一化，等价物已存在，
  增益不明——记为远期。
- 蘑菇体两阶段记忆：PE 已有 DNC + damage_memory，重复建设——记为远期。
- CTM 的自适应计算早退：推理期优化，训练期无收益——不做。

## 4 · 参考
CTM: arXiv 2505.05522 / github.com/SakanaAI/continuous-thought-machines ·
CfC: Hasani et al. AAAI 2021 + closed-form (Lechner 2021) ·
Mapping Networks: arXiv 2602.19134 (CVPR 2026) ·
FlyGM: arXiv 2602.17997 (清华) · flybody: Nature 2025-04 (DeepMind/HHMI) ·
NVIDIA 果蝇脑仿真: developer.nvidia.com/blog/modeling-fruit-fly-brains-to-better-understand-alzheimers ·
NVIDIA GR00T N1: nvidianews.nvidia.com/news/nvidia-isaac-gr00t-n1-open-humanoid-robot-foundation-model-simulation-frameworks (GTC 2025) ·
Newton/Isaac Lab CoRL 2025: nvidianews.nvidia.com/news/nvidia-accelerates-robotics-research-and-development-with-new-open-models-and-simulation-libraries ·
Newton 技术博客: developer.nvidia.com/blog/announcing-newton-an-open-source-physics-engine-for-robotics-simulation
