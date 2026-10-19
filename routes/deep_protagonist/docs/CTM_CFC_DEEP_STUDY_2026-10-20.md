# CTM / CfC 深度研读与落地方案（2026-10-20）

> 本文是对用户批评的直接回应："没通读论文就写简化版、infra 没做就开跑"。
> 研读来源：CTM 论文方法节全文（arXiv 2505.05522v4 HTML）+ CTM 官方 RL 训练脚本
> （tasks/rl/train.py）+ ncps 官方库 CfC 源码（cfc.py / cfc_cell.py，Apache-2.0）。
> 结论先行：**我此前的 CTM-lite 与 CfC v1 都有实质性偏差（见 §2.3 / §3.3 偏差表），
> 训练必须等忠实版 + 算子优化 + flag-off 等价验证全部完成、并经用户批准后才重启。**

## 1 · CTM 论文真实机制（方法节精确提取）

1. **突触模型**：a^t = f_syn(cat(z^t, o^t))，f_syn 是 U-NET 式 MLP（宽度收窄到 16 再展开，
   带层归一与跳连）。RL 模式无注意力，o^t 换成观测 backbone 特征。
2. **神经元级模型 NLM**：每个神经元 d 一个**私有** 1 隐层 MLP g_θd，只吃自己的 pre-activation
   历史 A^t_d ∈ R^M（M≈10-100）：z^{t+1}_d = g_θd(A^t_d)。实现用 einsum，权重张量形状
   (M, d_hidden, D)——每神经元独立参数。
3. **同步矩阵**：S^t = Z^t (Z^t)^T，即**每对神经元 post-activation 时间序列的内积**，
   带每对可学习的指数衰减 r_ij（S_ij = Z_i·diag(e^{-r_ij·k})·Z_j / 归一化）。
4. **配对选择**：S 是 O(D²)，训练开始时随机采样 D_out 个神经元对（三种策略：dense/
   semi-dense/random，RL 用 random），选出 S_out 作为表征。
5. **输出与确定性**：y^t = W_out·S^t_out（每个 tick 都出 logits）；certainty = 1−归一化熵。
6. **双检查点损失**：L = (L^{t1} + L^{t2})/2，t1=最小损失 tick，t2=最大确定性 tick——
   自适应计算由此免费获得（简单样本早停模式自然涌现）。

### 1.1 CTM 官方 RL 配方（train.py 源码事实，最有价值的一份）
- **iterations = 1**（RL 只用 1 个内部 tick！）、memory_length=5、d_model=128、
  synapse_depth=1、n_synch_out=16、deep_memory=True、neuron_select_type='random'。
- **状态跨环境步携带**：pre/post-activation trace 在 env step 间持续，
  done 时用掩码重置回初始 trace：`(1-done)*trace + done*init_trace`。
- **actor/critic 直接吃同步表征**（输入维 = n_synch_out = 16，不是原始 h）。
- PPO 超参：num_steps=100 × num_envs=8（批 800）、lr 5e-4 线性退火、ent_coef 0.1、
  1 epoch、4 minibatch、clip 0.1、GAE(0.99, 0.95)、reward 归一化、无 obs 归一化。
- 任务：CartPole/Acrobot/MiniGrid（注意：都是**短视距低维**任务，无我们的 582 维长程生存
  世界先例——移植风险如实记录）。

## 2 · 我方 CTM-lite 的偏差与忠实版设计

### 2.3 偏差表（D9 已实现版 vs 论文）
| 论文机制 | 我的 CTM-lite | 严重度 |
|---|---|---|
| 每神经元私有 NLM（历史 M 步） | 单个共享 Linear(cat(h,z))，无历史 | **高**：这是 CTM 的核心创新 |
| 同步 = 神经元对时间序列内积+衰减 | [z_last; z_mean] 拼接（非相关量） | **高**：表征本质不同 |
| 跨环境步状态 trace（done 掩码） | 无（每步重建） | 中：砍掉了记忆连续性 |
| 每 tick 输出 + 双检查点损失 | 仅末 tick | 低（RL 配方下可接受） |
| 3 个内部 tick | 论文 RL 用 1 | 低 |

### 2.4 忠实版 CTM-RL 升级设计（下一步实现，训练重启前置）
- 维度照抄官方：d_model=128, M=5, n_synch_out=16, synapse_depth=1。
- NLM：einsum 实现 per-neuron MLP——W1 张量 (M, d_hidden=4, D=128)（4096 参数，
  bmm 可高效批处理），W2 (d_hidden, 1, D)。z_new = W2·silu(W1·A)。
- 同步：随机固定 16 对 (i,j)（构造时以 seed 播种采样），S_ij = Σ_k z^k_i·z^k_j（k 遍历
  当前生命内累计 trace，cap 到最近 M 步与论文一致）。头改为读 [enc; S_out(16)]，
  为保暖启动等价仍走零初始化旁路读出（同 D9 惯例）。
- 状态管道：Transition 需额外携带 CTM trace（5×128/环境），done 掩码重置——
  落点在 main_train VecEnv 循环的 v_hin 快照处，改动量中等。
- 开关：`--ctm-faithful`（与 --ctm-ticks 互斥；后者保留作消融）。

## 3 · CfC 官方源码级机制（ncps 库）

### 3.1 wrapper 层（cfc.py）
- 序列处理 = 逐步 Python 循环调 cell(x_t, h, **ts**)，ts=该步时间跨度（可变）。
- cell 前有可选 backbone MLP（默认 128 units 1 层）；可选 mixed_memory（先过 LSTMCell）。
- 默认激活 lecun_tanh；Xavier uniform 初始化全部 2D 参数。

### 3.2 cell 层（cfc_cell.py，逐行核对）
- x = backbone(cat(input, h))；四个投影：ff1(x), ff2(x), t_a(x), t_b(x)。
- **default 模式**：t_interp = sigmoid(t_a·ts + t_b)；
  **new_hidden = tanh(ff1)·(1−t_interp) + t_interp·tanh(ff2)**。
- **pure 模式**（真正的 LTC 闭式解）：
  new_hidden = −A·exp(−ts·(|w_tau|+|ff1|))·ff1 + A（A、w_tau 可学习）。
- **no_gate**：new_hidden = ff1 + t_interp·tanh(ff2)。

### 3.3 我方 CfC v1 偏差表
| 官方 | 我的 v1 | 处置 |
|---|---|---|
| backbone MLP 先行 | 直接三 Linear 吃 cat(enc,h) | **本次修**（v2 加 backbone） |
| default 模式=双投影时间门插值 | exp 衰减型（近 pure 变体） | **本次修**（v2 默认 default，pure 留 `--cfc-mode pure`） |
| 可变 ts | dt 硬编码 1 | v2 接 ecfg.fixed_dt（世界本来就是固定步长，合理化） |
| Xavier uniform | libtorch 默认 | **本次修** |
| 4 投影分开 | 3 个分开 Linear | **本次修**：融合为单 GEMM（见 §5） |

## 4 · 第一性原理：DP 与 PE 各取什么

**DP（单智能体 PPO，梯度学习）**
- 学习信号靠 BPTT 穿过时间 → 网络的**时间动力学可微性**是第一约束。
- CTM（每神经元历史+同步表征）与 CfC（闭式连续时间）都是为梯度训练设计的动力学
  → 天然属于 DP。成本账：官方 RL 配方 iterations=1 → 每 env step 约多一次 cell
  计算（~1.2×），NLM einsum 是 bmm 密集算子，GPU 友好。
- DP 的既有痛点对位：技能预算竞争（多时间尺度）→ CfC 时间常数；"想一下再按键"
  （深链多步决策）→ CTM 内部拍+跨步记忆。

**PE（NEAT 种群，无梯度进化）**
- 无梯度 → CfC/CTM 的可训练性优势蒸发；且内部 tick ×种群×步数的乘法成本直接爆。
  **结论：CTM/CfC 不进 PE**（记为落选，理由=成本×收益双输）。
- PE 的第一约束是**基因型搜索地形**（fitness 锯齿=高维粗糙权重空间爬山）→
  MappingGenome（隐向量→权重，CVPR 2026 的低维光滑流形主张）直接命中 → PE 主刀。
- FlyGM 的教训（结构先验当骨架）与 GR00T 双速认知记为 DP 远期组合刀。

## 5 · 算子优化（cuBLAS 视角，训练重启前完成）

1. **CfC 门融合单 GEMM**（本次实现）：4 个投影合成一个 Linear(backbone→4H)，
   一次 cuBLAS GEMM + 切片。这是 cuDNN LSTM 的经典做法（所有门一次 GEMM）；
   v1 的 3 次独立小 GEMM 在 B=8 时纯吃 kernel launch 开销。backbone 与门 GEMM
   仍分两次（中间有激活，无法再合）。
2. **NLM einsum**：per-neuron MLP 用 einsum('bdm,mdh->bdh') → cuBLAS strided-batched
  GEMM 路径，天然高效；避免逐神经元循环。
3. **CTM tick 环**：tick 间数据依赖，不可跨 tick 融合；rollout 时 B=8 是延迟瓶颈，
  官方 iterations=1 使其代价趋近常数；update 时 B=256 为计算瓶颈，无需特殊处理。
   CUDA Graphs 削 launch 开销记为远期（libtorch 支持有限）。
4. **微基准计划**（忠实版实现后、训练批准前跑，GPU 秒级不占他人）：
   GRU vs CfC-v2(fused) vs CfC-v1(3 GEMM) 各 1000 步 B=8/B=256 计时；
   CTM-faithful 增量（+NLM einsum +sync pairs）单独计时。目标：CfC-v2 不慢于
   GRU ±10%，否则回炉。
5. 显存预算：新增参数 <0.3M（backbone 0.36M+门 4H×128 0.28M+NLM 8K+头），8GB 单卡无压力。

## 6 · 训练重启前置清单（全部打勾才向用户申请开跑）
- [ ] CfC v2 忠实版实现（backbone+default 模式+Xavier+融合 GEMM+dt 接线）——本次
- [ ] CfC v2 flag-off 字节等价验证（暖启旧冠军 ckpt 行为一致）
- [ ] CTM-faithful v2 实现（NLM einsum + 16 对 sync + 跨步 trace + done 掩码）——下一步
- [ ] CTM-faithful flag-off 字节等价验证
- [ ] 微基准（§5.4）达标
- [ ] 用户批准（GPU 轻用规则：不跑满、不并发）
