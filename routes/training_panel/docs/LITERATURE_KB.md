# LITERATURE_KB — 决策者的文献弹药库（deep_protagonist & protagonist_ecology）

> **这是协议 §8 指定的唯一文献文件**。目的：决策者别每轮从零查文献（"查一波就完事"）。这里是预先整理、贴着两个项目阶段的**可执行**知识点。决策时**先查这里**取"决策钩子"直接用；KB 没覆盖的具体问题，才做定向新检索（≥3 来源、抽出可执行参数），并**按模板追加回本文件**（KB 只增不减，越攒越厚）。
>
> **每条格式**：`[条目ID]` 标题(链接) → 大白话总结 → 可执行参数/做法 → 适配(项目/阶段) → **决策钩子**(一句可直接抄进 outbox 决策的话)。
> **引用纪律**：outbox 决策的"文献支持"栏必须写 `[条目ID]` + 取到的具体参数/数值，泛泛而谈（"参考了 PPO 论文"）不算。

---

## 索引表

| ID | 主题 | 项目 | 一句话钩子 |
|----|------|------|-----------|
| DP-PPO-1 | PPO 实现细节（决定成败的不是公式） | DP | 先把 8 个工程细节锁死再调超参 |
| DP-PPO-2 | PPO/on-policy 默认超参 | DP | clip0.2 / γ0.99 / GAEλ0.95 / lr3e-4 起步 |
| DP-PPO-3 | PPO 学不会稀疏任务时先排查项 | DP | 0% 存活先查 advantage 归一化+熵塌缩，别急着加奖励 |
| DP-BC-1 | 行为克隆的协变量漂移（复合误差） | DP | BC 单独误差 O(εT²)，必须配 DAgger/在线纠偏 |
| DP-BC-2 | DAgger（在线聚合 oracle 纠偏） | DP | 在 learner 自己的状态上查 oracle，β 指数退火 |
| DP-BC-3 | VPT（IDM→海量 BC base→微调） | DP | 小标注集训 IDM 标大数据，BC base 再 RL 微调 |
| DP-PBRS-1 | 势函数奖励塑形（策略不变） | DP/SHARED | 只用 F=γΦ(s')−Φ(s) 形式，别加裸 bonus |
| DP-EXP-1 | RND 内在奖励（硬探索） | DP/SHARED | 双价值头+内在奖励归一化，攻克 0-reward 区 |
| DP-SR-1 | HER 后见经验回放（稀疏奖励） | DP | 失败轨迹重标目标，仅 off-policy |
| DP-KL-1 | 从预训练策略 KL 锚定微调 | DP | BC→PPO 加 KL 到 BC 先验，防开局遗忘崩溃 |
| DP-DR-1 | 域随机化 + 记忆策略 | DP | 随机化世界物理，配 recurrent 防过拟合单设定 |
| DP-CL-1 | 课程学习 / 自动课程 | DP/SHARED | 用"学习进度"信号自动排难度，别一上来上满配 |
| PE-NEAT-1 | NEAT（拓扑+权重共进化） | PE | 历史标记对齐+speciation 保护创新+从最小起步 |
| PE-NOV-1 | 新颖性搜索（放弃目标反而更好） | PE | 目标欺骗时改奖行为新颖度，behavior 空间 k-NN |
| PE-POET-1 | POET（环境与解共进化） | PE | 环境难度自生成+stepping-stone 迁移 |
| PE-CIRC-1 | 共振时钟提升适应度（实证） | PE | 时钟周期匹配环境周期才有竞争优势 → 节律要"对相位" |
| PE-CIRC-2 | 在硅演化时钟（选"预测相位"） | PE | 要真时钟需季节性光周期+噪声+多输入，单一光暗不够 |
| PE-CIRC-3 | 自持节律由季节性日照变化促成 | PE | 恒定环境下能自持振荡=真内源时钟的判据 |
| PE-POMDP-1 | 部分可观下的记忆策略 | PE | 局部 FOV=POMDP，需 recurrent/记忆才能预判 |
| PE-ANTI-1 | 预判 vs 反应（相位超前） | PE | 用 phase-lead（领先量）而非相关性证明"预判" |
| SH-EVAL-1 | 评估/消融方法论 | SHARED | held-out 泛化+多种子+消融，单点指标会骗人 |
| SH-IMPL-1 | 实现细节决定深度RL复现 | SHARED | 归一化/初始化/裁剪/退火是大头，先固定再比较 |
| DP-MAXENT-1 | 最大熵RL / SAC（off-policy稳） | DP | 目标加熵项+自动温度α，连续控制稳且省样本 |
| DP-TD3-1 | TD3（双Q+延迟更新+目标平滑） | DP | 双critic取min治高估，actor每2步更一次 |
| DP-GAIL-1 | GAIL（对抗模仿，免造奖励） | DP | 判别器当奖励，专家少量轨迹学策略不漂移 |
| DP-HACK-1 | 奖励黑客 / 规范博弈 | DP/SHARED | 裸奖励必被钻空子，PBRS+held-out 抓规范博弈 |
| DP-EXP-2 | ICM 好奇心内在奖励 | DP | 逆动力学特征空间预测误差当bonus，免噪声电视 |
| DP-EXP-3 | Go-Explore（先回访再探索） | DP | 存档状态+确定性回访+模仿robustify破稀疏 |
| DP-EXP-4 | NGU/Agent57（情节记忆探索） | DP | 情节内k-NN新颖+UVFA一族探索/利用策略 |
| DP-PBT-1 | 群体训练（超参随训进化） | DP/SHARED | 并行群体exploit/explore，发现超参schedule |
| DP-BENCH-1 | Crafter 生存基准（成就） | DP | 22项成就当语义评估，比单一reward信息密 |
| DP-BENCH-2 | Voyager（技能库+自动课程） | DP/SHARED | 技能库只增不减=本KB同构，组合复用防遗忘 |
| DP-CF-1 | 灾难性遗忘 / EWC | DP | 阶段切换防旧技能崩，重要权重Fisher加锚 |
| DP-DT-1 | Decision Transformer（序列建模RL） | DP | return-to-go条件序列预测动作，离线起步 |
| DP-SIL-1 | 自模仿学习（重用高回报轨迹） | DP | 把自己过去好轨迹当专家再学，稀疏奖励提速 |
| PE-HYPERNEAT-1 | HyperNEAT/CPPN 间接编码 | PE | CPPN按几何坐标生成权重，规模/对称免演化 |
| PE-ES-1 | 进化策略（OpenAI-ES/CMA-ES） | PE | 黑盒免反传易并行，抗稀疏/长程延迟奖励 |
| PE-QD-1 | MAP-Elites 质量多样性 | PE | 行为网格每格存精英，illuminate整空间 |
| PE-COEVO-1 | 捕食者-猎物共进化军备竞赛 | PE | 耦合适应度驱复杂度，防红皇后空转用CIAO量 |
| PE-CIRC-4 | KaiABC 体外蛋白钟（无转录） | PE | 3蛋白+ATP即24h温补振荡，钟可纯翻译后 |
| PE-CIRC-5 | 相位响应曲线PRC / entrainment | PE | 用PRC量化扰动→相移，正式刻画entrain能力 |
| PE-CIRC-6 | 温度补偿（真钟定义性质） | PE | 周期对温度近不变(Q10≈1)才算真内源钟 |
| SH-REPRO-1 | 深度RL复现性危机（Henderson） | SHARED | 种子/实现/环境致结果剧变，须多种子报方差 |
| SH-STAT-1 | rliable 统计严谨（IQM+CI） | SHARED | 少run别比均值，用IQM+bootstrap CI+性能profile |
| SH-GEN-1 | Procgen 泛化（程序生成） | SHARED | 训练/测试用不同关卡抓过拟合，大模型更泛化 |
| DP-MB-1 | Dreamer/DreamerV3（世界模型想象训练） | DP | 学RSSM在想象里训AC，单配置免调超参省样本 |
| DP-MB-2 | MuZero（学到的模型+MCTS规划） | DP | 只学reward/policy/value三件套+树搜索免模拟器 |
| DP-WM-1 | World Models（VAE+RNN在梦里训） | DP | VAE压观测+RNN预测+极小控制器，可在梦里训 |
| DP-OFF-1 | 离线RL（IQL/CQL） | DP | 纯离线榨干oracle数据，IQL取上expectile不高估 |
| DP-RLHF-1 | 偏好奖励建模/RLHF | DP/SHARED | oracle成对偏好训奖励模型，<1%交互且抗刷分 |
| DP-HRL-1 | 分层RL/Options | DP | 高层选技能+低层执行，拆长程旗舰链可复用 |
| DP-EMP-1 | Empowerment 内在动机 | DP | 最大化动作→未来互信息，自发求生且不可刷分 |
| DP-GAE-1 | GAE 优势估计（λ调偏差方差） | DP/SHARED | γ0.99/λ0.95起步，长程信用传不回时调高 |
| PE-KURA-1 | Kuramoto 耦合振子（群体同步） | PE | 序参量r量化同步，弱耦合过K_c即自发锁相 |
| PE-SCN-1 | SCN 耦合（噪声钟→稳群体钟） | PE | 粗糙单体钟靠耦合变精准，精度来自网络非单体 |
| PE-COSINOR-1 | Cosinor 节律统计 | PE/SHARED | 拟合M+A·cos给振幅CI+零幅检验，别靠肉眼 |
| PE-MASK-1 | Masking vs 真钟（恒定释放） | PE | 关环境光周期，自持才是真钟，跟随=masking |
| PE-OE-1 | 开放式进化（放弃单一目标） | PE | 单轴穷尽转新颖性/QD/共进化让节律涌现 |
| PE-EVOLV-1 | 可进化性/模块化（连接代价） | PE | 罚连接数促模块化→更可进化、改一处不崩全局 |
| PE-ARNOLD-1 | Entrainment 范围/Arnold 舌 | PE | 扫多周期T测锁相范围，范围外失锁证有固有周期 |
| SH-FEP-1 | 自由能原理/主动推断（跨项目锚） | SHARED | 智能=减小预测误差；DP理解世界/PE预判同构 |
| SH-GOODHART-1 | Goodhart 定律（指标成靶即失效） | SHARED | 单代理指标必被钻，用多互补指标+held-out交叉 |
| DP-EXP-5 | 大规模纯好奇心（54环境/noisy-TV） | DP | 纯好奇心常与外在对齐，随机特征够，防noisy-TV |
| DP-MEM-1 | R2D2 循环回放（部分可观记忆） | DP | 存序列隐藏态+burn-in治漂移，52/57 Atari超人 |
| DP-BC-4 | 因果混淆（信息越多越差） | DP | BC非因果把伪相关当因果，砍伪输入+DAgger纠偏 |
| DP-IRL-1 | AIRL 可迁移奖励 | DP | 判别器=g+γh'−h恢复抗动力学变化可迁移奖励 |
| DP-PER-1 | 优先经验回放（按TD误差） | DP | p^α(α0.6)优先采+IS权重(β0.4→1)，重放稀有成功 |
| PE-CTRNN-1 | CTRNN 可演化振子 | PE | 连续时间递归网自激振荡，center-crossing促振 |
| PE-GOODWIN-1 | Goodwin 分子钟（延迟负反馈） | PE | 3变量负反馈，极限环需Hill>8，TTFL原型 |
| PE-CPG-1 | 中枢模式发生器/半中心振子 | PE | 两神经元互抑+慢适应即无外钟自激节律 |
| PE-MCC-1 | 最小判据共进化（开放式） | PE | 两群体各设最小判据即繁殖，无archive复杂度自增 |

---

## 用法（决策者）
1. 写决策前，按本轮问题在下面 DP / PE / SHARED 段里搜相关条目，取**决策钩子**。
2. KB 没覆盖 → 定向检索(≥3 来源) → 抽可执行参数 → 按模板**追加**到对应段 + 在上面索引表加一行。
3. outbox "文献支持"栏：`[ID] + 具体参数/数值`。

**追加条目模板**（复制下面这块）：
```
### [SEG-XXX-N] 标题
- **来源**：<标题>（<链接>）
- **总结**：<大白话 2-4 句>
- **可执行参数/做法**：<具体数值/步骤>
- **适配**：<DP/PE 哪个阶段，怎么用>
- **决策钩子**：<一句可直接抄进 outbox 的话>
```

---

# DP 段 — deep_protagonist（PPO + libtorch 单 agent，BC/DAgger 打底）

### [DP-PPO-1] PPO 实现细节比公式更决定成败
- **来源**：Engstrom et al. 2020, *Implementation Matters in Deep Policy Gradients*（https://arxiv.org/abs/2005.12729）；Huang et al. 2022, *The 37 Implementation Details of PPO*（https://iclr-blog-track.github.io/2022/03/25/ppo-implementation-details/）。
- **总结**：PPO 相对 TRPO 的大部分增益其实来自"代码级细节"而非裁剪目标本身。把这些细节做错，调多少超参都白搭。
- **可执行参数/做法**（8 个必锁项）：① 观测归一化 + 裁剪到 ±10；② 奖励缩放/折扣归一化；③ 优势 **per-minibatch 归一化**（mean0 std1）；④ 正交初始化（policy 输出层 gain=0.01）；⑤ 价值函数损失裁剪；⑥ 全局梯度范数裁剪 0.5；⑦ Adam lr 线性退火到 0；⑧ tanh 激活 + 独立 actor/critic（或共享但分头）。
- **适配**：DP 进 S4 PPO 阶段前，先逐条核对训练代码是否落实这 8 项；任一缺失会表现为"学不动/方差大/复现不了"。
- **决策钩子**：`PPO 调参前先按 [DP-PPO-1] 核对 8 个实现细节（优势归一化/正交初始化gain0.01/grad-clip0.5/lr退火/obs归一化），缺哪个先补哪个，别先动超参。`

### [DP-PPO-2] PPO / on-policy 默认超参起点
- **来源**：Schulman et al. 2017 PPO（https://arxiv.org/abs/1707.06347）；Andrychowicz et al. 2020, *What Matters in On-Policy RL*（https://arxiv.org/abs/2006.05990）；SB3 zoo 默认（https://github.com/DLR-RM/rl-baselines3-zoo/blob/master/hyperparams/ppo.yml）。
- **总结**：有一组"几乎不会错"的默认值，新环境先用它跑通再说，不要一上来魔改。
- **可执行参数/做法**：clip ε=0.2；γ=0.99（长程任务可 0.999）；GAE λ=0.95（Andrychowicz 实测 0.9 常更稳）；Adam lr=3e-4；epochs/批=3–10；minibatch 充分大；value coef=0.5；entropy coef=0.0–0.01；grad-clip=0.5；并行环境数尽量多（吞吐+稳定）。
- **适配**：DP S4 首训用此组；survival 长程建议 γ=0.999、GAE λ=0.95。
- **决策钩子**：`S4 PPO 首训用默认 [DP-PPO-2]：clip0.2/γ0.999(长程)/GAEλ0.95/lr3e-4/ent0.01/grad0.5，跑通基线后再单变量调。`

### [DP-PPO-3] PPO 在稀疏/觅食任务 0% 时的排查顺序
- **来源**：综合 [DP-PPO-1][DP-EXP-1][DP-SR-1] + PPO plateau 研究（https://www.arxiv.org/pdf/2603.06009）。
- **总结**：DP 当前症状=PPO 学不会蛋白觅食（held-out 0% 存活 vs oracle 47%）。这是典型"探索+稀疏奖励+实现细节"复合问题，不是"加奖励"能解。
- **可执行参数/做法**（按序排查）：① 熵是否过早塌缩（监控 policy entropy，塌了就抬 ent_coef 到 0.01–0.03）；② 优势是否归一化、reward 尺度是否离谱；③ 有效奖励信号密度（几乎全 0 → 上 PBRS [DP-PBRS-1] 或内在奖励 [DP-EXP-1]）；④ 是否该用 BC base 起步（[DP-BC-3]）而非从零 PPO；⑤ 域随机化是否过强淹没信号。
- **适配**：DP 觅食卡点直接套这个清单。
- **决策钩子**：`觅食 0% 先按 [DP-PPO-3] 排查熵塌缩→优势归一化→奖励密度→BC起步，确认是探索问题再上 [DP-EXP-1]/[DP-PBRS-1]，不许直接硬加觅食奖励。`

### [DP-BC-1] 行为克隆的协变量漂移（复合误差）
- **来源**：Ross & Bagnell；Ross/Gordon/Bagnell 2011 DAgger（https://arxiv.org/abs/1011.0686）。
- **总结**：纯 BC 在专家没走过的状态会犯错，错一步进入更陌生状态→错误滚雪球。理论上单纯 BC 的误差随时域 **O(εT²)** 增长，而在线纠偏（DAgger）能压到 **O(εT)**。
- **可执行参数/做法**：BC 训完务必在**自己 rollout 的状态分布**上评估（不是专家分布）；若 rollout 偏离则需 DAgger 或加噪声注入。
- **适配**：DP 的 oracle demo→BC base 链路；解释"BC 在新 bank 泛化掉崖"的根因。
- **决策钩子**：`BC base 掉崖大概率是协变量漂移 [DP-BC-1]，按学习者自身分布评估，配 [DP-BC-2] DAgger 纠偏，而不是堆更多专家数据。`

### [DP-BC-2] DAgger：在 learner 状态上查 oracle 纠偏
- **来源**：Ross et al. 2011, *A Reduction of Imitation Learning to No-Regret Online Learning*（https://arxiv.org/abs/1011.0686）；imitation 库实现（https://imitation.readthedocs.io/en/latest/algorithms/dagger.html）。
- **总结**：迭代地"让 learner 跑→在它到的状态上问 oracle 正确动作→并入数据集重训"，让训练分布逼近 learner 实际遇到的分布。
- **可执行参数/做法**：β 混合系数指数退火（β_i = p^{i-1}，如 p=0.5；i=1 时几乎纯专家，之后逐步交给 learner）；数据集**累积不丢**（aggregate）；每轮新增 rollout 后全量重训。
- **适配**：DP oracle 可随时查询，正适合 DAgger。把"叉鱼觅食"做成 oracle 在线纠偏。
- **决策钩子**：`觅食用 [DP-BC-2] DAgger：β 指数退火(p=0.5)、数据集累积、每轮在 learner 自身状态查 oracle 重训，比纯 BC 抗漂移。`

### [DP-BC-3] VPT：小标注集训 IDM → 海量 BC base → 微调
- **来源**：Baker et al. 2022, *Learning to play Minecraft with Video PreTraining*（https://openai.com/index/vpt/，代码 https://github.com/openai/Video-Pre-Training）。
- **总结**：先用少量带动作标签的数据训一个逆动力学模型(IDM)给海量无标注数据打动作标签，得到强 BC 基座，再用 BC/RL 微调到具体任务。证明"强 BC base + 微调"能解 24000 步级长程任务（挖钻石）。
- **可执行参数/做法**：BC base 先把"通用生存行为"学全；下游用少量任务数据微调或 RL 微调（配 [DP-KL-1] 防遗忘）。
- **适配**：DP 蓝图"BC base 演示叉鱼觅食 + PBRS 引导"正是 VPT 范式的小型版。
- **决策钩子**：`按 [DP-BC-3] VPT 范式：先把 BC base 的通用求生学扎实，觅食等专项用微调(BC/PPO)叠加，别在 base 阶段塞专项。`

### [DP-PBRS-1] 势函数奖励塑形（不改变最优策略）
- **来源**：Ng, Harada & Russell 1999, *Policy Invariance under Reward Transformations*（https://people.eecs.berkeley.edu/~russell/papers/icml99-shaping.pdf）。
- **总结**：唯一**保证不改变最优策略**的塑形形式是 F(s,s')=γΦ(s')−Φ(s)（Φ 任意势函数）。任何"裸 bonus"都可能让 agent 学歪（reward hacking）。等价于用 Φ 初始化 Q。
- **可执行参数/做法**：Φ 取"到目标的负距离"或子目标势；务必带 γ 折扣的差分形式，不能直接 +b。
- **适配**：DP 蓝图明确"BC 阶段不加塑形奖励"，但 PPO 阶段若要引导觅食，**必须**用 PBRS 形式（如 Φ=−到食物距离），保持北极星"数学等价不污染因果"。
- **决策钩子**：`若要给觅食加引导，只允许 [DP-PBRS-1] 势函数形式 F=γΦ(s')−Φ(s)（Φ=−到食物距离），禁止裸 bonus，符合"不污染因果信号"红线。`

### [DP-EXP-1] RND：随机网络蒸馏内在奖励（硬探索）
- **来源**：Burda et al. 2018, *Exploration by Random Network Distillation*（https://arxiv.org/abs/1810.12894，代码 https://github.com/openai/random-network-distillation）。
- **总结**：内在奖励 = 一个可训练网络预测"固定随机网络"输出的误差；没见过的状态预测误差大→鼓励去新地方。攻克 Montezuma 等 0-reward 硬探索。
- **可执行参数/做法**：观测归一化（对 RND 尤其关键）；内在奖励做 running-std 归一化；**双价值头**：内在用非情节式 γ_int=0.99，外在 γ_ext=0.999；两者优势相加；gamma_ext=0.999 实测重要。
- **适配**：若 [DP-PPO-3] 确认 DP 是"找不到食物"的探索问题，用 RND 让 agent 主动探索觅食区。
- **决策钩子**：`觅食确属硬探索时上 [DP-EXP-1] RND：obs 归一化+内在奖励归一化+双价值头(γ_int0.99非情节/γ_ext0.999)，而不是加密集外在奖励。`

### [DP-SR-1] HER：后见经验回放（稀疏二元奖励）
- **来源**：Andrychowicz et al. 2017, *Hindsight Experience Replay*（https://arxiv.org/abs/1707.01495）。
- **总结**：把"失败"轨迹的实际到达状态当作"本来就想要的目标"重新标注，于是每条轨迹都有学习信号；是一种隐式课程。仅适用 off-policy。
- **可执行参数/做法**："future"重标策略，每个 transition 采样 k=4 个未来达成态做替代目标；需目标条件化策略 + off-policy 算法（DDPG/SAC/DQN）。
- **适配**：DP 若改用目标条件化（如"到达某资源"）且用 off-policy，可显著提样本效率。**注意**：PPO 是 on-policy，用不了 HER——要么换 off-policy，要么走 PBRS/RND 路线。
- **决策钩子**：`HER [DP-SR-1] 只对 off-policy 有效；DP 现用 PPO(on-policy) 则走 [DP-PBRS-1]/[DP-EXP-1]，除非决策切到 off-policy 目标条件化。`

### [DP-KL-1] 从预训练(BC)策略 KL 锚定微调，防开局崩溃
- **来源**：Luo et al. 2023（https://arxiv.org/pdf/2303.17396）；PROTO 2023（https://export.arxiv.org/pdf/2305.15669v1.pdf）；WSRL 2025（https://proceedings.iclr.cc/paper_files/paper/2025/file/504491292cb71e7681eedfe0e602b72f-Paper-Conference.pdf）。
- **总结**：用 BC/offline 策略初始化后直接 RL 微调，开局常因分布失配导致价值发散、"遗忘"预训练能力。加一个到先验策略的 KL 正则（或 warmup）能稳住，再逐步放松。
- **可执行参数/做法**：PPO 微调时加 KL(π‖π_BC) 惩罚，系数从大到小退火（trust-region 式）；或 WSRL：先用少量预训练策略 rollout 预热再正式微调。
- **适配**：DP 从 BC base 切 PPO 微调时套用，防止"BC 好不容易学的求生被 PPO 开局打没"。
- **决策钩子**：`BC→PPO 微调按 [DP-KL-1] 加 KL 到 BC 先验(系数由大退小)，或先 warmup，防开局遗忘崩溃；这正解释了"微调后掉质量立即 revert"的坑。`

### [DP-DR-1] 域随机化 + 记忆策略
- **来源**：Tobin et al. 2017（https://arxiv.org/pdf/1703.06907）；Chen et al. 2021, *Understanding Domain Randomization*（https://arxiv.org/pdf/2110.03239）。
- **总结**：训练时随机化世界参数（物理/外观），让真实/新设定只是"又一种变体"，提升泛化。理论指出：随机化下**带记忆（history-dependent）策略**很重要，否则单纯随机化反而难学。
- **可执行参数/做法**：随机化范围要覆盖目标分布但别过宽淹没信号；配 recurrent/帧堆叠给策略记忆；逐步加大随机化（配课程 [DP-CL-1]）。
- **适配**：DP "v4 加域随机化"——务必同时确保策略有记忆能力，且随机化强度别一步到位（见 [DP-PPO-3] 第⑤条）。
- **决策钩子**：`域随机化按 [DP-DR-1]：范围覆盖但不过宽+配记忆策略+课程渐增；若加域随机化后觅食更差，先怀疑随机化淹没了信号。`

### [DP-CL-1] 课程学习 / 自动课程
- **来源**：Narvekar et al. 2020 JMLR 综述（https://www.jmlr.org/papers/volume21/20-212/20-212.pdf）；Portelas et al. 2020 IJCAI ACL 综述（https://www.ijcai.org/proceedings/2020/671）。
- **总结**：把难任务拆成由易到难的序列能解决"从零学不会"的问题；自动课程用"学习进度(learning progress)"信号动态选当前最该练的难度。
- **可执行参数/做法**：手工：先固定世界、再逐步开机制（雨/夜冷/资源稀缺）；自动：以近期成功率/回报增量为信号，挑成功率 ~50% 的难度档。
- **适配**：DP 蓝图"机制相互耦合"链（下雨→灭火→夜冷…）天然是课程；建议按耦合链逐级解锁，而非一次上满。
- **决策钩子**：`机制耦合链按 [DP-CL-1] 做课程：逐级解锁(先单机制达标再叠加)，自动档选成功率~50% 的难度，避免一次上满导致 0% 学不动。`

### [DP-MAXENT-1] 最大熵 RL / SAC（off-policy、稳、省样本）
- **来源**：Haarnoja et al. 2018, *Soft Actor-Critic*（https://arxiv.org/abs/1801.01290）；Haarnoja et al. 2018, *SAC Algorithms and Applications*（自动温度版，https://arxiv.org/abs/1812.05905）。
- **总结**：在标准回报上**加一个策略熵项**（J=Σ E[r + α·H(π)]），即“既要拿奖励又要尽量随机”。带来两大好处：探索更充分、对超参/种子**鲁棒**（论文卖点：不同种子曲线几乎重合，正好治 DP “复现难/方差大”）。SAC 把它做成 off-policy actor-critic，样本效率远高于 PPO。
- **可执行参数/做法**：clipped double-Q（两个 critic 取 min 治高估）；目标网络软更新 τ=0.005；replay buffer 1e6；lr=3e-4；γ=0.99；**自动温度 α**：以 target entropy = −dim(A)（连续动作维数的负值）为约束自动调 α，免手调；v1 对 reward scale 敏感，v2 用自动 α 后基本免调。
- **适配**：DP 若 PPO 在觅食上持续 0%/高方差，SAC 是**强力备选**——off-policy 重用经验更省 RTX5060 的算力，且熵正则天然缓解 [DP-PPO-3] 的熵塌缩。注意 SAC 原生面向连续动作；离散动作用 discrete-SAC 变体。
- **决策钩子**：`PPO 觅食长期 0% 或种子间方差大时，按 [DP-MAXENT-1] 评估换 SAC：自动温度α(target entropy=−dimA)+双Q取min+τ0.005+buffer1e6，off-policy 更省算力且抗熵塌缩。`

### [DP-TD3-1] TD3：双 Q + 延迟更新 + 目标平滑（治 Q 高估）
- **来源**：Fujimoto, van Hoof & Meger 2018, *Addressing Function Approximation Error in Actor-Critic Methods*（https://arxiv.org/abs/1802.09477）。
- **总结**：DDPG 不稳的根因是 critic **系统性高估** Q 值并被 actor 放大。TD3 三招修：① 双 critic 取较小者算目标（clipped double-Q）压高估；② **延迟策略更新**（critic 更 d=2 次才更 actor 一次）；③ **目标策略平滑**（给目标动作加裁剪噪声，避免 Q 在尖峰过拟合）。
- **可执行参数/做法**：actor 更新频率 d=2；目标平滑噪声 σ=0.2、裁剪到 ±0.5；探索噪声 σ=0.1；τ=0.005；lr=3e-4；γ=0.99；其余同 DDPG。
- **适配**：DP 若走连续控制路线（连续移动/朝向），TD3 是比 DDPG 稳得多的确定性策略基线；与 SAC 二选一（SAC 随机策略+熵，TD3 确定性+省一点）。
- **决策钩子**：`连续控制基线别用裸 DDPG，按 [DP-TD3-1] 上 TD3：双Q取min+actor每2步更1次+目标动作加噪σ0.2裁±0.5，先治 Q 高估再谈调参。`

### [DP-GAIL-1] GAIL：对抗模仿，免手工造奖励
- **来源**：Ho & Ermon 2016, *Generative Adversarial Imitation Learning*（https://arxiv.org/abs/1606.03476）。
- **总结**：不显式恢复奖励函数（IRL 慢），而是把模仿当 GAN：**判别器 D** 学着区分“专家(s,a)”与“策略(s,a)”，**策略**则把 −log(1−D(s,a))（或 log D）当奖励去骗过 D，最终匹配专家的状态-动作占用分布。因为策略是**在环境里自己 rollout** 训练，不像纯 BC 那样有协变量漂移的复合误差，少量专家轨迹即可。
- **可执行参数/做法**：策略优化用 TRPO/PPO；判别器是二分类器，输出当奖励信号；专家轨迹可很少（论文数十条即超过 BC）；注意对抗训练不稳——判别器学习率别太高、可加梯度惩罚。
- **适配**：DP 已有 oracle 能产生觅食/求生 demo。若 BC base 因协变量漂移 [DP-BC-1] 掉崖、又不想手工设计觅食奖励，GAIL 是“用 oracle demo 直接逼出在线策略”的路子，且天然规避 [DP-HACK-1] 的奖励黑客（没有可被钻的手工奖励）。
- **决策钩子**：`不想手工设计觅食奖励、又怕 BC 漂移时，按 [DP-GAIL-1] 用 oracle demo 跑 GAIL：判别器输出当奖励+PPO优化策略，在线 rollout 训练免复合误差，少量专家轨迹即可。`

### [DP-HACK-1] 奖励黑客 / 规范博弈（reward hacking / specification gaming）
- **来源**：Krakovna et al. 2020, DeepMind *Specification gaming: the flip side of AI ingenuity*（https://deepmind.google/blog/specification-gaming-the-flip-side-of-ai-ingenuity/，附~60例清单）；Clark & Amodei 2016, OpenAI *Faulty Reward Functions in the Wild*（https://openai.com/index/faulty-reward-functions/）。
- **总结**：智能体会**满足奖励的字面定义却违背意图**。经典例：OpenAI CoastRunners 赛艇不冲线、原地绕圈刷补给分，得分反而更高；DeepMind 乐高任务奖励“红块底面高度”，智能体直接把红块翻过来而不是叠上去。DeepMind 已收集约 60 个真实例子。根因=奖励设定有漏洞+优化器无情钻空子。
- **可执行参数/做法**：① 奖励塑形只用**势函数** PBRS（[DP-PBRS-1]，F=γΦ(s')−Φ(s)，策略不变、不引入可刷的裸 bonus）；② 必须在 **held-out** 环境用“真实意图指标”评估（如真实存活/觅食成功，而非代理奖励，见 [SH-EVAL-1]）；③ 警惕代理指标与意图脱钩。
- **适配**：DP 给觅食/求生加任何 bonus 前必看这条——尤其“加奖励引导觅食”极易被钻（如反复触碰食物源刷分而不真吃）。
- **决策钩子**：`加任何觅食/求生 bonus 前过 [DP-HACK-1]：塑形只许用 PBRS 势函数(不加裸bonus)+用真实意图指标在 held-out 评估，否则智能体大概率刷代理分而非真求生。`

### [DP-EXP-2] ICM：好奇心内在奖励（在特征空间预测误差）
- **来源**：Pathak et al. 2017, *Curiosity-driven Exploration by Self-supervised Prediction*（https://arxiv.org/abs/1705.05363）。
- **总结**：内在奖励 = 智能体对“自己动作后果”的**预测误差**，但预测发生在由**逆动力学模型**学到的特征空间里（只编码“agent 能控制/与 agent 相关”的部分），从而**自动忽略不可控噪声**（解决“噪声电视”问题——RND 也有此优点）。VizDoom / Super Mario 上即使**没有外在奖励**也能探索过关。
- **可执行参数/做法**：两个网络——逆模型 g(s,s')→â 学特征 φ；前向模型 f(φ(s),a)→φ̂(s')；内在奖励 r_i = η·‖φ̂(s')−φ(s')‖²；总奖励 = r_ext + r_i；典型与 A3C/PPO 合用。
- **适配**：DP 觅食硬探索的另一选项，与 RND（[DP-EXP-1]）并列。ICM 更强调“只对可控部分好奇”，在物理多变的 3D 世界里比裸预测像素更稳。
- **决策钩子**：`硬探索除 [DP-EXP-1] RND 外可选 [DP-EXP-2] ICM：逆动力学特征空间的前向预测误差当 bonus，自动忽略不可控噪声(噪声电视)，适合物理多变的3D世界。`

### [DP-EXP-3] Go-Explore：先回到有希望的状态，再从那探索
- **来源**：Ecoffet, Huizinga, Lehman, Stanley & Clune 2019/2021, *Go-Explore* / *First return, then explore*（https://arxiv.org/abs/1901.10995，Nature 590:580–586）。
- **总结**：硬探索失败常因“探索时把好不容易到的远处状态又忘了”。Go-Explore：① 用 **cell 存档**记住到过的有代表性状态；② **先确定性地直接返回**某个有希望的状态（不在路上瞎探索），③ 再从该状态探索；④ 最后用**模仿学习把脆弱的解 robustify** 成抗噪策略。Montezuma 均分 **43k**（约前 SOTA 的 4 倍），带领域知识 **650k**、最高 **18M**；Pitfall 首个 >0（均分 ~60k）。
- **可执行参数/做法**：cell 表示用降采样图像/领域特征聚合状态；存档保留每 cell 的最佳轨迹与返回方式；阶段二 robustify 用 [DP-BC-2]/[DP-BC-3] 类模仿。
- **适配**：DP 觅食/求生本质是稀疏+可能欺骗的硬探索；Go-Explore 的“存档关键状态+确定性回访”思路即使不全套实现，也能启发——例如缓存“已发现食物源/安全火堆”的状态做定向重置。
- **决策钩子**：`稀疏觅食久攻不下时参 [DP-EXP-3]：缓存“已到达的关键状态(食物源/火堆)”+确定性回访再从那探索，最后用模仿 robustify；Montezuma 靠这套从~0 到 43k。`

### [DP-EXP-4] NGU / Agent57：情节记忆驱动的定向探索
- **来源**：Badia et al. 2020, *Never Give Up*（https://arxiv.org/abs/2002.06038）；Badia et al. 2020, *Agent57*（https://arxiv.org/abs/2003.13350）。
- **总结**：NGU 用**情节记忆**做内在奖励——在 episode 内对“近期访问过的状态”用 **k-NN** 算新颖度（鼓励一局内不断访问新状态），再叠一个跨 episode 的终身新颖度（RND 式）调制；并用 **UVFA** 在同一网络里学一**族**从“纯探索”到“纯利用”的策略。Agent57 在其上加自适应地选用哪条策略+更稳的架构，成为**首个在全部 57 个 Atari 上超人类**的智能体；NGU 在 Pitfall 取得均分 ~8400、中位数人类归一化 1344%。
- **可执行参数/做法**：情节内新颖度=embedding 空间 k-NN 距离（embedding 由逆动力学自监督学，偏向可控特征）；终身调制用 RND 误差；UVFA 输入含“探索程度”超参 β，一网多策略。
- **适配**：DP 若要在单 agent 上**同时**保留探索与利用，UVFA “一族策略”思路 + 情节内新颖度可作进阶探索方案（比单一 ε/熵更结构化）。实现重，列为方向性参考。
- **决策钩子**：`需要“探索与利用兼得”的进阶方案时参 [DP-EXP-4]：情节内 k-NN 新颖度+终身 RND 调制+UVFA 一族策略；实现较重，DP 资源有限可先取“情节内新颖度”轻量版。`

### [DP-PBT-1] 群体训练 PBT：超参随训练在线进化
- **来源**：Jaderberg et al. 2017, *Population Based Training of Neural Networks*（https://arxiv.org/abs/1711.09846）。
- **总结**：并行跑一**群**模型，周期性地 **exploit**（表现差的复制表现好的权重）+ **explore**（扰动其超参），从而发现的是一条**超参 schedule**（随训练阶段变化的超参），而非全程固定的一组——后者通常次优。比网格/随机搜索省算力、且边训边调。
- **可执行参数/做法**：群体规模常 ~10–40；每隔若干步按验证表现排序，底部 exploit 顶部，超参乘 0.8/1.2 扰动；可调 lr、entropy coef、batch 等。
- **适配**：DP 算力有限（单 RTX5060）跑不起大群体；但 PBT 的**核心洞见**直接可用——**好超参是分阶段的**（如 entropy coef 早高晚低、lr 退火），呼应 [DP-PPO-1] 的 lr 退火。决策时可手工模拟“schedule 而非定值”。PE 侧本就是群体进化，PBT 思路天然契合。
- **决策钩子**：`超参别全程定死，按 [DP-PBT-1] 思路设 schedule：entropy coef 早高(0.03)晚低(0.005)、lr 线性退火；算力不够跑真 PBT 群体就手工分阶段调。`

### [DP-BENCH-1] Crafter：开放世界生存基准（用成就做语义评估）
- **来源**：Hafner 2021, *Benchmarking the Spectrum of Agent Capabilities (Crafter)*（https://arxiv.org/abs/2109.06780）。
- **总结**：Crafter 是 2D 程序生成的开放世界**生存**游戏（觅食、喝水、睡觉、躲怪、采集、合成工具），单一环境内用 **22 个语义成就**（unlock achievements）评估广谱能力。评分=各成就成功率的**几何均值**（“Crafter score”），比单一 reward 信息密得多，且需要强泛化+深探索+长程推理才能解锁全部成就。
- **可执行参数/做法**：用“成就解锁谱”而非单标量回报评估；几何均值惩罚“偏科”（某成就 0% 会拉垮总分）；区分 reward agent 与 unsupervised agent。
- **适配**：**与 DP 高度同构**（DP 也是 3D 求生、机制耦合）。强烈建议 DP 借鉴其评估法：把“生火/避雨/御寒/觅食/护火”等做成**成就向量**，报几何均值，而不是只看单一存活率——能立刻暴露“只会一招”的偏科智能体。
- **决策钩子**：`DP 评估别只看单一存活率，按 [DP-BENCH-1] 学 Crafter：把求生子目标(生火/避雨/觅食/御寒)做成就向量+报几何均值，几何均值会惩罚偏科，比单标量更能反映真求生能力。`

### [DP-BENCH-2] Voyager：技能库 + 自动课程（开放式终身学习）
- **来源**：Wang et al. 2023, *Voyager: An Open-Ended Embodied Agent with LLMs*（https://arxiv.org/abs/2305.16291）。
- **总结**：Minecraft 里的 LLM 终身学习体，三件套：① **自动课程**（自提目标最大化探索）；② **不断增长的技能库**（把学会的行为存成可执行代码、可检索复用）；③ 带环境反馈/自检的迭代提示。技能库**只增不减、可组合**，因而能力**复利式**增长并**缓解灾难性遗忘**。比基线多 3.3× 物品、远 2.3×、解锁科技树快 15.3×，并能把技能库迁到新世界。
- **可执行参数/做法**：技能=带文档的可执行单元，按描述向量检索；课程目标按“当前能做什么”自适应升级；失败→反馈→改写→入库。
- **适配**：**直接同构于本 KB 与协议本身**——Voyager 的“技能库只增不减、检索复用”正是 LITERATURE_KB.md（条目只增、按问题检索决策钩子）和协议 §8 文献纪律的设计哲学。可作“为什么 KB 要累积而非每轮重查”的权威背书；也启发 DP 把“已验证有效的训练配方/课程片段”沉淀成可复用条目。
- **决策钩子**：`为 KB“只增不减+检索复用”找背书时引 [DP-BENCH-2] Voyager：技能库复利增长且缓解遗忘，正说明决策者该沉淀可复用配方而非每轮从零查(对应协议§8)。`

### [DP-CF-1] 灾难性遗忘 / EWC（阶段切换别把旧技能学没）
- **来源**：Kirkpatrick et al. 2017, *Overcoming catastrophic forgetting in neural networks (EWC)*, PNAS（https://arxiv.org/abs/1612.00796）。
- **总结**：神经网在学新任务时会**覆盖**旧任务的权重→旧技能崩塌（灾难性遗忘）。EWC 的解法：估计每个权重对旧任务的**重要性**（用 Fisher 信息近似），在学新任务时给重要权重加**二次锚定惩罚**（别动它们太多），从而旧新兼顾。
- **可执行参数/做法**：损失加 Σ_i (λ/2)·F_i·(θ_i−θ*_i)²，F_i=Fisher 对角、θ*=旧任务最优；λ 控制保旧强度。
- **适配**：DP 的**机制耦合课程**（[DP-CL-1]）逐级解锁时，新阶段易把上阶段学会的求生技能打没——这就是灾难性遗忘。EWC 或更简单的**回放旧阶段数据 / KL 锚到旧策略**（[DP-KL-1]）可缓解。解释“加新机制后旧能力倒退”的根因。
- **决策钩子**：`课程升级后旧技能倒退=灾难性遗忘 [DP-CF-1]：加新阶段时回放旧阶段数据或对旧策略 KL 锚定([DP-KL-1])，必要时上 EWC(Fisher 加权权重锚)，别指望网络自己记住。`

### [DP-DT-1] Decision Transformer：把 RL 当条件序列建模
- **来源**：Chen et al. 2021, *Decision Transformer: Reinforcement Learning via Sequence Modeling*（https://arxiv.org/abs/2106.01345）。
- **总结**：不做 TD/自举/动态规划，而是把轨迹当 token 序列 (return-to-go, state, action, …) 喂给 GPT，**以“还想拿多少回报”(return-to-go)为条件预测下一个动作**。在 Atari/D4RL 等**离线**数据上匹配甚至超过传统离线 RL，训练像监督学习一样稳。
- **可执行参数/做法**：输入 token=(R̂_t, s_t, a_t) 序列；推理时设定目标 return-to-go 引导生成动作；上下文窗口 K 步；纯监督交叉熵/回归损失，无 bootstrap。
- **适配**：DP 已有 oracle/BC 轨迹数据。若想用**离线**数据稳定起步（避开 PPO 在线探索的 0% 坑），DT 是“监督式”路线：用 oracle 轨迹训 DT、设高目标回报采样动作，再 [DP-KL-1] 衔接在线微调。列为离线起步备选。
- **决策钩子**：`想用 oracle/离线数据稳起步、绕开在线探索 0% 时参 [DP-DT-1] Decision Transformer：以 return-to-go 为条件监督预测动作，训练像 BC 一样稳，再衔接在线微调。`

### [DP-SIL-1] 自模仿学习：把自己过去的高回报轨迹当专家
- **来源**：Oh et al. 2018, *Self-Imitation Learning*（https://arxiv.org/abs/1806.05635）。
- **总结**：智能体偶尔会“撞大运”拿到高回报轨迹，但 on-policy 一带而过、学不牢。SIL 把**过去回报超过当前价值估计**的转移存起来反复模仿（等价于一个 lower-bound soft Q-learning），在稀疏奖励下显著提速——本质是“重用自己的好运气”。
- **可执行参数/做法**：经验池存 (s,a,R)；当 R > V(s) 时做模仿更新（优势裁剪到 ≥0）；与 A2C/PPO 并行（A2C+SIL）；优先回放高优势样本。
- **适配**：DP 觅食偶尔成功（held-out 0% 但训练期未必从不成功）时，SIL 能把这些稀有成功“焊牢”，比纯 on-policy PPO 更快利用稀疏正样本；比加外在 bonus 安全（不引入可刷分项，规避 [DP-HACK-1]）。
- **决策钩子**：`觅食偶有成功却学不牢时上 [DP-SIL-1] 自模仿：把 R>V(s) 的过去成功轨迹存下反复模仿(优势裁≥0)，焊牢稀有正样本，比加 bonus 安全(不可刷分)。`

### [DP-MB-1] Dreamer / DreamerV3：在世界模型的“想象”里训练，单配置通吃
- **来源**：Hafner et al. 2023, *Mastering Diverse Domains through World Models (DreamerV3)*（https://arxiv.org/abs/2301.04104）；Nature 2025 版 *Mastering diverse control tasks through world models*（https://www.nature.com/articles/s41586-025-08744-2）。
- **总结**：学一个 latent 世界模型(RSSM)，actor-critic **完全在想象 rollout 里**训练，几乎不靠真环境交互→**样本效率极高**。**一套固定超参**通吃 150+ 任务(连续/离散、像素/低维、2D/3D、各种 reward scale)；是**首个无人类数据/无课程从零在 Minecraft 挖到钻石**的算法。鲁棒三招：symlog 变换、two-hot 离散回归、return/advantage 归一化；模型越大→数据效率与最终性能都越好。
- **可执行参数/做法**：symlog 压 reward/value 尺度免调；critic 用 two-hot 编码做分类式回归；world model=RSSM(确定性 h+随机 z latent)；想象 horizon H≈15；固定超参跨任务不调。
- **适配**：DP 在 RTX5060(8GB) 算力紧→“想象里训练”比 PPO 在线狂采样**省真环境步数**，symlog+归一化天然治 [DP-PPO-3] 的 reward scale/方差问题；觅食稀疏+像素观测正是 Dreamer 强项。列为**高优先级备选架构**。
- **决策钩子**：`DP 算力紧/觅食稀疏且要省真环境步数时，按 [DP-MB-1] 评估上 DreamerV3：学RSSM世界模型在想象里训AC，symlog+two-hot+return归一化让单配置免调超参，样本效率远高于PPO。`

### [DP-MB-2] MuZero：用“学到的模型”做树搜索规划
- **来源**：Schrittwieser et al. 2020, *Mastering Atari, Go, Chess and Shogi by Planning with a Learned Model*, Nature（https://arxiv.org/abs/1911.08265）。
- **总结**：不需环境规则/模拟器，学一个**只预测对规划有用的三样**(reward、policy、value)的隐式模型，配 MCTS 树搜索。在 **57 个 Atari** 上 SOTA；围棋/象棋/将棋不给规则也匹配 AlphaZero 的超人水平。
- **可执行参数/做法**：模型三件套(representation/dynamics/prediction)端到端学；MCTS 用学到的 dynamics 展开；不需真实模拟器。
- **适配**：DP 若想“先脑内规划再行动”，MuZero 是范式参考；但 MCTS+三网络实现/算力门槛高，对 RTX5060 偏重。**优先级低于 [DP-MB-1] Dreamer**，列为“规划路线”理论参考而非首选落地。
- **决策钩子**：`想让 DP “先规划再动”时参 [DP-MB-2] MuZero：只学 reward/policy/value 三件套+MCTS，免真模拟器；但算力门槛高，RTX5060 上优先级低于 [DP-MB-1] Dreamer。`

### [DP-WM-1] World Models：先学世界，再在“梦里”训策略
- **来源**：Ha & Schmidhuber 2018, *World Models*（https://arxiv.org/abs/1803.10122）。
- **总结**：把 agent 拆成 V(VAE 压观测成 latent)+M(MDN-RNN 预测下一 latent)+C(极小控制器)。可**完全在 M 生成的“梦”里训练** C 再迁回真环境(CarRacing/VizDoom 验证)。是 Dreamer 系思想源头。
- **可执行参数/做法**：VAE 编码帧→latent z；MDN-RNN 预测 z 分布+隐藏态 h；控制器只吃 [z,h]、参数极少，可用 CMA-ES([PE-ES-1])进化。
- **适配**：给 DP 一个**轻量分解思路**——先无监督学世界表征(VAE)，再在小控制器上做 RL/进化，省端到端大网络算力；是 [DP-MB-1] 同源但更易上手的最小版。
- **决策钩子**：`想低成本试“世界模型”时先看 [DP-WM-1]：VAE 压观测+RNN 预测+极小控制器，可在“梦”里训 C，比 Dreamer 轻，控制器小到能用 CMA-ES 进化。`

### [DP-OFF-1] 离线 RL：IQL / CQL（不碰数据外动作）
- **来源**：Kostrikov et al. 2021, *Offline RL with Implicit Q-Learning (IQL)*（https://arxiv.org/abs/2110.06169）；Kumar et al. 2020, *Conservative Q-Learning (CQL)*（https://arxiv.org/abs/2006.04779）。
- **总结**：纯用已有数据集训练、**不在线交互**；难点是分布漂移(query 数据外动作→价值高估爆炸)。IQL **从不评估数据外动作**：把 V 当随机变量取**上分位(expectile τ→1)**逼近最优动作价值，再 advantage-weighted 抽策略。CQL 则给数据外动作的 Q **加保守惩罚**(压低没见过的动作)。
- **可执行参数/做法**：IQL expectile τ≈0.7–0.9(越大越激进)；advantage-weighted regression 温度 β；CQL 加 min-Q 正则系数 α。两者都只需固定数据集。
- **适配**：DP 有 oracle/BC 轨迹。想**零在线探索、纯离线**把 oracle 数据榨干(规避 PPO 在线 0% 坑)，IQL 比 [DP-DT-1] DT 更“RL 原生”且不高估，再用 [DP-KL-1] 衔接在线。与 DT 并列为离线起步两条路。
- **决策钩子**：`想纯离线榨干 oracle 数据、零在线探索时上 [DP-OFF-1] IQL：取 V 的上 expectile(τ0.7-0.9)避免评估数据外动作+advantage-weighted 抽策略，比 DT 更 RL 原生不高估；CQL 则给未见动作加保守惩罚。`

### [DP-RLHF-1] 偏好奖励建模 / RLHF：从“哪个更好”学奖励
- **来源**：Christiano et al. 2017, *Deep RL from Human Preferences*, NeurIPS（https://arxiv.org/abs/1706.03741）。
- **总结**：不手工写奖励，而是给人(或 oracle)看**两段轨迹片段选哪个更好**，用偏好拟合奖励模型再跑 RL。只需对 **<1% 的交互**给反馈就能解 Atari/MuJoCo；约 **1 小时人类时间**能训出新行为。是治 [DP-HACK-1] 奖励黑客的正道(奖励来自偏好而非可刷的代理量)。
- **可执行参数/做法**：成对偏好→Bradley-Terry 模型拟合 r̂；RL 与奖励模型**交替**更新；只对不确定的片段对查询(主动学习)省标注。
- **适配**：DP 觅食“难写对奖励、裸奖励被钻空子”→让 **oracle 当偏好标注器**(它会觅食)，对 PPO 轨迹片段判优劣训奖励模型，比手调 PBRS 势函数更不易被规范博弈钻。与 [DP-PBRS-1]/[DP-HACK-1] 互补。
- **决策钩子**：`DP 奖励难写对/老被钻空子时参 [DP-RLHF-1]：用 oracle 对轨迹片段做成对偏好(Bradley-Terry)训奖励模型，<1% 交互即可、不易被规范博弈钻，比手调 PBRS 稳。`

### [DP-HRL-1] 分层 RL / Options：时间抽象拆长程任务
- **来源**：Sutton, Precup & Singh 1999, *Between MDPs and Semi-MDPs (Options)*（https://www.sciencedirect.com/science/article/pii/S0004370299000521）；Bacon et al. 2017, *The Option-Critic Architecture*（https://arxiv.org/abs/1609.05140）；Vezhnevets et al. 2017, *FeUdal Networks*（https://arxiv.org/abs/1703.01161）。
- **总结**：策略分两层——高层选**子目标/技能(option)**，低层在 option 内出原子动作，option 有自己的终止函数 β。好处：长程稀疏任务被拆短、技能可复用、探索更有结构。Option-Critic 端到端学 option；FeUdal 用 manager 设方向、worker 执行。
- **可执行参数/做法**：定义 option=(初始集, 内部策略, 终止函数 β)；option-critic 同时学内部策略与 β；或预定义少量手工技能(觅食/护火/避雨)做高层调度。
- **适配**：DP 旗舰链(下雨→浇火→无暖→夜冷→护火/避雨/御寒)天然是**多技能序列**→分层把“何时护火/何时觅食”交高层、各技能低层独立训，比单层 PPO 学整条链更易；与 [DP-BENCH-2] Voyager 技能库思路一致。
- **决策钩子**：`DP 旗舰链是多技能序列、单层PPO学整条难时参 [DP-HRL-1]：高层选技能(option:护火/避雨/觅食)+低层执行+终止函数β，长程稀疏被拆短、技能可复用，呼应 Voyager 技能库。`

### [DP-EMP-1] Empowerment：以“对未来的掌控力”当内在动机
- **来源**：Klyubin, Polani & Nehaniv 2005, *Empowerment: A Universal Agent-Centric Measure of Control*；Mohamed & Rezende 2015, *Variational Information Maximisation for Intrinsically Motivated RL*（https://arxiv.org/abs/1509.08731）；综述 Tiomkin et al. 2024, PRX Life。
- **总结**：empowerment = **动作序列与未来状态间的互信息**(信道容量)，即“我现在能把未来推向多少种可区分的状态”。最大化它=偏好**自己最有掌控力**的状态，能在无外在奖励下产生有意义行为(靠近开关、保命)，纯信息论、不可被刷分。
- **可执行参数/做法**：用 MINE/变分下界估 I(a_t…;s_{t+k})；当内在 bonus 加到稀疏外在奖励上；k 步 horizon 控制“看多远”。
- **适配**：DP 求生本质=**保住对未来的选择权**(别死、别熄火)。empowerment bonus 鼓励 agent 避开“动弹不得/快死”的低掌控态，是比 [DP-EXP-1] RND 更“求生对齐”的内在动机，且不引入可刷分代理量(规避 [DP-HACK-1])。
- **决策钩子**：`想给 DP 一个“求生对齐”的内在动机时参 [DP-EMP-1] empowerment：最大化动作→未来状态互信息(变分下界估)，agent 自发避开“快死/动不了”的低掌控态，比 RND 更对齐求生且不可刷分。`

### [DP-GAE-1] GAE：用 λ 在偏差-方差间调优势估计
- **来源**：Schulman et al. 2016, *High-Dimensional Continuous Control Using Generalized Advantage Estimation*（https://arxiv.org/abs/1506.02438）。
- **总结**：优势 Â_t = Σ_l (γλ)^l δ_{t+l}，δ_t=r_t+γV(s_{t+1})−V(s_t)。**λ=0**→一步 TD(低方差高偏差)；**λ=1**→蒙特卡洛(高方差低偏差)；中间折中。配 γ(有效 horizon≈1/(1−γ))共同控制“看多远+偏差方差”。是 PPO 默认就用的优势算法。
- **可执行参数/做法**：常用 **γ=0.99, λ=0.95**；觅食这种长程信用分配可调高(γ0.995/λ0.98，更看远但方差大需更多种子)；优势务必标准化([DP-PPO-3])。
- **适配**：DP 觅食是**长程稀疏信用分配**——奖励在叉到鱼时才来，中间一长串动作要正确归因。若怀疑“学不会是因信用传不回去”，按本条调 γ/λ(先 0.99/0.95，长程试 0.995/0.98)，再看 advantage 归一化。
- **决策钩子**：`DP 觅食疑似“信用传不回长程”时按 [DP-GAE-1] 调 GAE：默认 γ0.99/λ0.95，长程稀疏可升 γ0.995/λ0.98(更看远但方差大,需多种子)，并务必标准化优势。`

### [DP-EXP-5] 大规模纯好奇心学习：没有外在奖励也能学
- **来源**：Burda et al. 2018, *Large-Scale Study of Curiosity-Driven Learning*（https://arxiv.org/abs/1808.04355）。
- **总结**：在 **54 个环境**(含全 Atari)上做**纯好奇心、零外在奖励**训练，表现意外地好——因为很多游戏的内在好奇目标与人工外在奖励**高度对齐**。关键发现：① 用**随机固定特征**算预测误差对很多基准已够用，**学到的特征泛化更好**(如 Super Mario 新关)；② 致命软肋=**noisy-TV 问题**：环境里不可预测的随机源(雪花屏)会永久吸引 agent(预测误差永远高)。
- **可执行参数/做法**：内在奖励=特征空间预测误差(同 ICM)；特征用随机网络(省)或学到的(泛化好)；务必规避不可控随机源，否则 agent 卡死盯噪声。
- **适配**：给 DP 一个判断——**觅食/求生若与“探索新状态”天然对齐**，纯/混合好奇心可在稀疏奖励下推动学习；但 DP 世界若有随机渲染/不可控扰动，要警惕 noisy-TV 把探索带偏。配 [DP-EXP-1] RND(RND 正是为缓解 noisy-TV 设计)。
- **决策钩子**：`DP 想靠探索破稀疏时记 [DP-EXP-5]：纯好奇心在54环境意外有效(内在与外在常对齐)，随机特征够用、学到特征泛化更好；但小心 noisy-TV(不可控随机源永久吸引)，故优先用抗噪的 RND([DP-EXP-1])。`

### [DP-MEM-1] R2D2：循环网络+经验回放治部分可观
- **来源**：Kapturowski et al. 2019, *Recurrent Experience Replay in Distributed RL (R2D2)*, ICLR（https://openreview.net/forum?id=r1lyTjAqYX）。
- **总结**：把 LSTM 与优先经验回放结合训 RL agent。难点=**表征漂移/循环状态陈旧**(replay 里存的隐藏态与当前网络不符)。解法：存储**整段序列的隐藏态**+**burn-in**(先用前若干步只前向不训练来“热身”隐藏态)。单一架构+固定超参，**Atari-57 翻 4 倍 SOTA、52/57 游戏超人**。
- **可执行参数/做法**：回放存序列+起始隐藏态；burn-in 前 40 步只恢复状态不算损失；训练序列长 80；用 LSTM 处理部分可观。
- **适配**：DP/PE 都是**部分可观(POMDP)**(视野有限、机制需记忆)。若 DP 用 off-policy/replay 且观测部分可观，R2D2 的“存隐藏态+burn-in”是处理记忆的标准做法；与 [PE-POMDP-1] 记忆策略呼应。
- **决策钩子**：`DP/PE 部分可观需记忆且用 replay 时参 [DP-MEM-1] R2D2：回放存整段序列+起始隐藏态、burn-in 前40步热身隐藏态再算损失(治表征漂移)，LSTM+固定超参即 52/57 Atari 超人。`

### [DP-BC-4] 因果混淆：模仿学习里“信息越多反而越差”
- **来源**：de Haan, Jayaraman & Levine 2019, *Causal Confusion in Imitation Learning*, NeurIPS（https://arxiv.org/abs/1905.11979）。
- **总结**：BC 是**非因果**的判别模型——只学“观测→专家动作”的相关，不懂因果。在分布漂移下会发生**因果误判(causal misidentification)**：给 agent **更多信息反而更差**(经典例:仪表盘上显示“上一步刹车灯”的特征会被 BC 当成刹车的原因→学会“看到刹车灯就刹车”而非看路况)。解法=用**定向干预**(环境交互或专家查询)找出真正的因果变量。
- **可执行参数/做法**：别一股脑喂全部观测给 BC；做特征级干预/消融找因果特征；用 DAgger([DP-BC-2])式专家查询纠偏。
- **适配**：DP 有 BC base(bc_v9)。本条是**红线警告**：给 BC 加观测(如加上一帧动作/状态指示)可能让觅食**更差**而非更好；若 BC base 在新 bank 崩，先怀疑因果混淆——砍掉可能是“伪因果线索”的输入特征，再配 DAgger 纠偏。
- **决策钩子**：`DP 给 BC 加观测后反而更差时查 [DP-BC-4] 因果混淆：BC 非因果、分布漂移下会把伪相关当因果(看到刹车灯就刹车)，砍掉伪因果输入特征+DAgger([DP-BC-2])定向纠偏，别盲目加信息。`

### [DP-IRL-1] AIRL：学到可迁移、抗动力学变化的奖励
- **来源**：Fu, Luo & Levine 2018, *Learning Robust Rewards with Adversarial Inverse RL (AIRL)*, ICLR（https://arxiv.org/abs/1710.11248）。
- **总结**：GAIL([DP-GAIL-1]) 只学到“像专家”的策略，但**判别器不是真正的奖励**(换环境就废)。AIRL 改造对抗式 IRL，把判别器结构化成 **reward + shaping** 形式，从而**恢复出真正可迁移的奖励函数**——在训练时动力学变化下仍能用(迁移设定大幅超 GAIL)。
- **可执行参数/做法**：判别器 f(s,a,s')=g(s,a)+γh(s')−h(s)(g≈奖励、h≈势函数，正是 PBRS 形式 [DP-PBRS-1])；交替训判别器与策略。
- **适配**：DP 若想从 oracle 轨迹**提取一个能复用的奖励函数**(而非每个新 bank 重训)，AIRL 比 GAIL 更值——它给的是带 PBRS 结构的可迁移奖励，正好缝合 [DP-PBRS-1] 势函数与 [DP-GAIL-1] 对抗模仿。
- **决策钩子**：`DP 想从 oracle 轨迹提取“可跨 bank 复用”的奖励时上 [DP-IRL-1] AIRL：判别器结构化成 g(s,a)+γh(s')−h(s)(奖励+PBRS势)，恢复抗动力学变化的可迁移奖励，比 GAIL([DP-GAIL-1])只学策略更值。`

### [DP-PER-1] 优先经验回放：按 TD 误差挑重要样本
- **来源**：Schaul et al. 2016, *Prioritized Experience Replay (PER)*, ICLR（https://arxiv.org/abs/1511.05952）。
- **总结**：均匀回放浪费在“已学会”的转移上。PER 按 **|TD 误差|** 给转移定优先级多采高误差样本，学得更快；但优先采样引入偏差，需 **重要性采样(IS)权重**校正。
- **可执行参数/做法**：优先级 p=|δ|+ε；采样概率 ∝ p^α(**α=0.6**)；IS 权重指数 **β 从 0.4 线性退火到 1**；用 sum-tree O(log N) 采样。
- **适配**：DP 若走 off-policy/replay 路线(如配 SAC[DP-MAXENT-1]/R2D2[DP-MEM-1])，PER 能让稀疏的“成功觅食”转移被优先重放(高 TD 误差)，加速稀疏奖励学习；与自模仿 [DP-SIL-1]“焊牢稀有成功”同精神。
- **决策钩子**：`DP 走 off-policy/replay 且正样本稀疏时加 [DP-PER-1] PER：按|TD误差|^α(α0.6)优先采样+IS权重(β0.4→1)校偏，让稀有“成功觅食”转移被优先重放，配 SAC/R2D2/[DP-SIL-1]。`

---

# PE 段 — protagonist_ecology（NEAT 群体进化，验证环境感知→真节律，尺=phase-lead 预判）

### [PE-NEAT-1] NEAT：拓扑与权重共进化
- **来源**：Stanley & Miikkulainen 2002, *Evolving Neural Networks through Augmenting Topologies*（https://nn.cs.utexas.edu/?stanley:ec02=，TR https://www.cs.utexas.edu/ftp/AI-Lab/tech-reports/UT-AI-TR-01-290.pdf）。
- **总结**：NEAT 三支柱——① 历史标记(innovation number)解决拓扑交叉的对齐；② speciation(物种隔离)保护"刚出现还没优化好的新结构"；③ 从最小结构起步、逐步复杂化。三者缺一显著变慢（论文有消融）。
- **可执行参数/做法**：兼容距离 δ = c1·E/N + c2·D/N + c3·W̄（E=多余基因, D=不相交基因, W̄=权重均差），典型 c1=c2=1.0, c3=0.4，兼容阈值~3.0；保留最小拓扑起步；speciation 内共享适应度(fitness sharing)。
- **适配**：PE 引擎已生产级；当怀疑"创新被过早淘汰/多样性塌缩"时回看这三支柱与 speciation 阈值。
- **决策钩子**：`PE 多样性塌缩/新结构存活差时，按 [PE-NEAT-1] 检查 speciation 兼容阈值(~3.0,c3=0.4)与 fitness sharing，而不是只调变异率。`

### [PE-NOV-1] 新颖性搜索：放弃目标反而更易到达
- **来源**：Lehman & Stanley 2011, *Abandoning Objectives: Evolution through the Search for Novelty Alone*（https://www.cs.swarthmore.edu/~meeden/DevelopmentalRobotics/lehman_ecj11.pdf）。
- **总结**：当适应度函数有"欺骗性"(deceptive)，直接奔目标会卡死在死胡同；改为只奖励"行为新颖度"反而常更快到目标。揭示单点目标指标的根本局限。
- **可执行参数/做法**：定义 behavior characterization(行为特征向量)；新颖度 = 在行为空间对 k 近邻的平均距离（k≈15）；维护新颖度 archive；可与目标做加权(novelty + fitness)。
- **适配**：PE 已发现"单轴打法穷尽"(cold-threshold PLATEAU、thermal-damage CAPPED 且把预判打负)——典型目标欺骗。新颖性/行为多样性目标可能跳出平台。
- **决策钩子**：`单轴 PLATEAU/CAPPED 是目标欺骗 [PE-NOV-1] 的信号：引入行为新颖度(behavior 空间 k=15 近邻距离)作辅助目标，跳出单指标死胡同，而非继续磨该轴。`

### [PE-POET-1] POET：环境与解共进化（自生成课程）
- **来源**：Wang et al. 2019, *Paired Open-Ended Trailblazer*（https://arxiv.org/abs/1901.01753）。
- **总结**：同时进化"环境难度"和"解决它的 agent"，并允许把一个环境里的解迁移到另一个环境当垫脚石(stepping stone)。很多复杂行为单靠直接优化做不出，靠这种"自生成课程+迁移"才涌现。
- **可执行参数/做法**：环境用最小可行准则(minimal criterion)筛选(不能太易/太难)；定期把各环境的精英 agent 跨环境迁移测试，更优则替换；ES/进化作内循环优化器。
- **适配**：PE "一锅炖 A–F co-tune"+FOV 退火本质是手工课程；POET 思路=让环境难度也参与进化，可作 Phase D–F 的扩展方向（属"蓝图改向"，需 ping 用户）。
- **决策钩子**：`PE 若考虑让环境难度自适应进化(而非手工 FOV 退火)，参 [PE-POET-1]，但这是蓝图改向级别——先 ping 用户再动。`

### [PE-CIRC-1] 共振时钟提升适应度（关键实证）
- **来源**：Ouyang, Andersson, Kondo, Golden & Johnson 1998, *Resonating circadian clocks enhance fitness in cyanobacteria*, PNAS（https://www.pnas.org/doi/10.1073/pnas.95.15.8660）。
- **总结**：内源时钟周期**与环境光暗周期匹配(共振)**的蓝藻菌株，在竞争中胜出。这是"时钟有适应价值"的经典直接证据——关键在**周期/相位匹配**，不是有振荡就行。
- **可执行参数/做法**：评估节律价值要在**竞争/相对适应度**下测，且对比"周期匹配 vs 失配"两组；失配组应被淘汰。
- **适配**：PE 北极星——验证环境感知是否产生真节律。这条给出"真节律"的适应度判据：匹配环境周期的个体应在选择压下占优。
- **决策钩子**：`PE 证"真节律"要按 [PE-CIRC-1]：设置周期匹配 vs 失配竞争组，匹配组适应度占优才算节律有用，单看是否振荡不够。`

### [PE-CIRC-2] 在硅演化时钟：选"预测相位"才长出真时钟
- **来源**：Troein, Locke, Turner & Millar 2009, *Weather and Seasons Together Demand Complex Biological Clocks*, Current Biology（http://msturner.warwick.ac.uk/research/publications/currbiolclockpaper.pdf）。
- **总结**：在硅演化基因调控网络、**以"正确预测一天中特定相位"为选择压**，结果：要长出具备自然时钟特征(恒定条件下持续振荡、可被光 entrain、多反馈环+多光输入)的网络，**必须**有季节性光周期变化 + 现实噪声；只给简单光暗周期长不出复杂真时钟。
- **可执行参数/做法**：选择压直接定义为"相位预测准确度"(=PE 的 phase-lead 思路)；环境要含 ① 季节性昼长变化 ② 天气/噪声；评估网络是否在恒定光下仍自持振荡。
- **适配**：**与 PE 方法论高度同构**。PE 用 phase-lead 当尺、环境realism roadmap(A–F)正对应"逐步加真实度"。这条直接支持"必须加季节/噪声才能逼出真预判"。
- **决策钩子**：`PE 若 phase-lead 转不正，按 [PE-CIRC-2] 先确认环境是否含季节性昼长变化+噪声——简单固定光暗周期理论上长不出真预判时钟，应优先加环境真实度而非调网络。`

### [PE-CIRC-3] 自持节律由"季节性日照变化"促成
- **来源**：2022, *Evolution of self-sustained circadian rhythms is facilitated by seasonal change of daylight*, Proc. R. Soc. B（https://pmc.ncbi.nlm.nih.gov/articles/PMC9682437/）；综述 Jabbur & Johnson 2022, *Spectres of Clock Evolution*（https://www.frontiersin.org/articles/10.3389/fphys.2021.815847/full）。
- **总结**：独立工作再次表明——**自持(self-sustained)振荡**(撤掉外部信号仍维持)的演化，被"季节性日照变化"显著促进。区分"被动跟随光"和"内源自持时钟"的判据=撤光后是否继续振荡。
- **可执行参数/做法**：判据实验=free-running test(恒定条件)；只有内源时钟在恒定条件下保持~24h 周期；环境设计含季节项。
- **适配**：给 PE 一个干净的"真 vs 假节律"操作判据(free-running)，补充 phase-lead。
- **决策钩子**：`PE 加一个 free-running 判据 [PE-CIRC-3]：撤掉环境光信号后个体是否仍自持振荡，区分"被动反应"与"内源时钟"，与 phase-lead 互证。`

### [PE-POMDP-1] 部分可观下需要记忆策略
- **来源**：Wierstra et al., *Recurrent Policy Gradients*（https://mediatum.ub.tum.de/doc/1289980/910628.pdf）；Ni et al. 2021 recurrent model-free 基线（https://ar5iv.labs.arxiv.org/html/2102.12344）。
- **总结**：当个体只能看到局部(FOV 受限)，问题是 POMDP，无记忆的反应式策略原理上拿不到最优；需要循环网络/记忆把历史观测整合成内部状态。
- **可执行参数/做法**：给控制网络加循环连接/记忆单元；评估"记忆 vs 无记忆"对预判能力的影响。
- **适配**：PE 的 FOV 退火(180°→70°)=逐步加大部分可观性。要"预判"未来(相位超前)，个体必须能整合时间信息→**网络须具备记忆/递归**，否则 phase-lead 理论上做不出。
- **决策钩子**：`PE 收窄 FOV 后要 phase-lead 转正，按 [PE-POMDP-1] 先确认个体网络有记忆/递归能力——纯前馈反应式在 POMDP 下原理上无法"预判"。`

### [PE-ANTI-1] 预判 vs 反应：用"相位超前"而非相关性
- **来源**：Eberle et al. 2018, *Anticipation from sensation: anticipating synchronization stabilizes a system with sensory delay*, R. Soc. Open Sci.（https://pmc.ncbi.nlm.nih.gov/articles/PMC5882674/）。
- **总结**：真正的"预判"表现为输出**相位领先**于驱动信号(领先量>0)，而不是滞后跟随或仅相关。可用预判机制抵消传感延迟。
- **可执行参数/做法**：度量个体行为相对环境周期的相位差，**领先(phase-lead>0)** 才算预判；correlation 高但相位滞后=只是反应。
- **适配**：直接背书 PE "尺=phase-lead 预判，而非简单相关"的方法论选择。
- **决策钩子**：`坚持 PE 的判据 [PE-ANTI-1]：只认 phase-lead>0(行为领先环境)为"预判"，相关性高但相位滞后只是反应，不能算节律达标。`

### [PE-HYPERNEAT-1] HyperNEAT / CPPN：按几何生成连接的间接编码
- **来源**：Stanley, D'Ambrosio & Gauci 2009, *A Hypercube-Based Indirect Encoding for Evolving Large-Scale Neural Networks*, Artificial Life 15(2)（https://doi.org/10.1162/artl.2009.15.2.15202）。
- **总结**：不直接进化每根权重（直接编码规模不住），而是进化一个 **CPPN**（组合模式产生网络）：输入两个神经元在**基板(substrate)上的几何坐标**(x1,y1,x2,y2)→输出这条连接的权重。因 CPPN 由对称/周期/高斯等函数组成，能生成带**对称、重复模式**的连接，把任务的几何规律映射进网络拓扑；同一 CPPN 还能**在任意分辨率**生成网络（换输入/输出数量无需重新演化）。
- **可执行参数/做法**：定义 substrate 几何（把传感器/动作按物理位置摆进坐标系）；CPPN 本体用 NEAT 进化；连接权重<阈值则置 0（稀疏化）。
- **适配**：PE 个体有明确的**空间/几何结构**（FOV 视场、方位传感器）。若未来要放大感官分辨率或网络规模，HyperNEAT 的间接编码能把"视场几何对称性"当先验用上，比直接编码更易 scale。属"编码换代"级别，是蓝图改向——先 ping 用户。
- **决策钩子**：`PE 若要放大感官分辨率/网络规模又不想参数爆炸，参 [PE-HYPERNEAT-1] 间接编码(CPPN 按 substrate 坐标生成权重、可跨分辨率)；但这是编码换代级改动，先 ping 用户。`

### [PE-ES-1] 进化策略（OpenAI-ES / CMA-ES）：黑盒、免反传、易并行
- **来源**：Salimans et al. 2017, *Evolution Strategies as a Scalable Alternative to RL*（https://arxiv.org/abs/1703.03864）；Hansen 2016 CMA-ES 教程（https://arxiv.org/abs/1604.00772）。
- **总结**：OpenAI-ES：给参数加高斯噪声生成一群扰动，按适应度加权平均估计梯度（无需反向传播）。因仅需**通信标量**(随机种子+适应度)，可扩到 1000+ worker：1440 核上 **10 分钟**训出 3D MuJoCo humanoid，1 小时内 Atari 与 A3C 相当。ES **对动作频率不敏感、耐长程延迟奖励、无需折扣/价值函数**——这正是稀疏/长程任务的痛点。CMA-ES：自适应全协方差矩阵，低维(≤90参数)下样本效率极高。
- **可执行参数/做法**：ES 群体 ~ 数百个扰动；镜像采样(antithetic)减方差；适应度排名归一化(rank-based)抗场鲁棒；权重衰减。CMA-ES 适合参数量小的控制器。
- **适配**：PE 本就是进化/群体路线。当节律/预判的适应度信号**稀疏且延迟**（要跨多个周期才看出 phase-lead）时，ES 比基于梯度的 RL 更鲁棒——它不需逐步奖励信号，只看一整局的总适应度。与 NEAT 互补（NEAT 进拓扑，ES 调权重）。
- **决策钩子**：`PE 的节律适应度稀疏且跨周期延迟时，按 [PE-ES-1] 用进化策略而非梯度 RL：ES 只看整局总适应度、免反传易并行、耐延迟奖励；低维控制器可用 CMA-ES。`

### [PE-QD-1] MAP-Elites：质量多样性，点亮整个行为空间
- **来源**：Mouret & Clune 2015, *Illuminating Search Spaces by Mapping Elites*（https://arxiv.org/abs/1504.04909）。
- **总结**：不只找单个最优解，而是用户选几个**行为维度**→离散成**网格**，每个格只保留该行为的**最优个体(elite)**；反复变异随机 elite 填格。结果既给出**一大批多样且高质的解**，又能看清"行为维度 × 性能"的地形；因探索更全，常能经"踏脚石"找到比直接优化更好的全局解。(已用于 Llama3 rainbow teaming、AlphaEvolve。)
- **可执行参数/做法**：选 2–3 个行为特征做网格轴；每格存 (基因型, 适应度)；变异产物落入哪格就与该格 elite 竞争；填充率/QD-score 衡量覆盖。
- **适配**：PE "单轴打法穷尽"（[PE-NOV-1] 提到的 PLATEAU/CAPPED）可用 MAP-Elites 破：以"节律相位 / FOV / 能耗"等为行为轴，保留各种行为的精英，避免单一适应度把群体压向同一解；还能一眼看出"哪些行为区域从没被探索"。
- **决策钩子**：`单轴 PLATEAU 时除 [PE-NOV-1] 外可上 [PE-QD-1] MAP-Elites：选 2–3 个行为轴(相位/FOV/能耗)建网格、每格存精英，保多样性防群体坍缩到单解，并暴露未探索区。`

### [PE-COEVO-1] 捕食者-猎物共进化：军备竞赛与红皇后陷阱
- **来源**：Nolfi & Floreano 1998, *Coevolving Predator and Prey Robots: Do "Arms Races" Arise in Artificial Evolution?*, Artificial Life（https://doi.org/10.1162/106454698568620）。
- **总结**：两个适应度**耦合**的群体互为选择压，可能驱动**复杂度递增的军备竞赛**；但也常陷入**红皇后动态**(Red Queen)——双方不断互相克制、适应度振荡但**无真正进步**。论文还发现：有时"简单但易改"的策略反而打败"复杂但通用"的策略。
- **可执行参数/做法**：衡量"真进步 vs 循环"要用 **CIAO 图 / master tournament**（拿当前个体打历史所有对手，而非只打同代）；保留历史对手池(hall of fame)防遗忘。
- **适配**：PE 若引入多物种/捕食压力来逼节律演化，务必用 CIAO/名人堂判定是否真进步——否则"适应度在涨"可能只是红皇后振荡，与 [PE-CIRC-1] 的竞争适应度判据互补。
- **决策钩子**：`共进化/捕食压力场景下别被"适应度在涨"骗，按 [PE-COEVO-1] 用 CIAO 图/master tournament(打历史全部对手)判是否真进步，保留 hall-of-fame 防红皇后循环。`

### [PE-CIRC-4] KaiABC：体外 3 蛋白重构的无转录生物钟
- **来源**：Nakajima … Kondo et al. 2005, *Reconstitution of Circadian Oscillation of Cyanobacterial KaiC Phosphorylation in vitro*, Science（https://pubmed.ncbi.nlm.nih.gov/15831759/）；综述 Chavan et al. 2023（https://pmc.ncbi.nlm.nih.gov/articles/PMC10772220/）。
- **总结**：只把 3 个纯化蛋白 **KaiA/KaiB/KaiC + ATP** 混在试管里，就能产生**自持的 ~24h KaiC 磷酸化节律**，且**温度补偿**、可维持多天相位相干——**完全不需转录/翻译反馈环**。证明生物钟可以是纯**翻译后(蛋白级)**的极简机制。
- **可执行参数/做法**：机制上"振荡器"不需复杂基因网络，三组件负反馈(KaiC 自磷酸化/去磷酸化循环 + KaiA 加速/KaiB 抑制)即可出 24h 周期。
- **适配**：给 PE 一个**最小钟架构启示**：要进化出真节律，个体网络**不需很大**，关键是有一个带延迟的**负反馈环**产生自持振荡(限环)。与 [PE-POMDP-1] 互补：记忆/递归提供"延迟"，是振荡的结构基础。
- **决策钩子**：`担心 PE 网络太小长不出钟时引 [PE-CIRC-4]：KaiABC 仅 3 蛋白+ATP 就出 24h 温补振荡，说明真钟靠的是带延迟的负反馈限环而非网络规模——优先确保个体有记忆/递归([PE-POMDP-1])。`

### [PE-CIRC-5] 相位响应曲线 PRC：量化 entrainment 的正式工具
- **来源**：*Phase Response Curves*, NCBI Bookshelf (Biological Clocks, Rhythms, and Oscillations)（https://www.ncbi.nlm.nih.gov/books/NBK544600/）。
- **总结**：PRC = 把"在不同相位给一个扰动"引起的**相移 Δφ** 画成曲线。Type 1(弱、连续) vs Type 0(强、跳变重置)；某些相位的强扰动会落到**奇点(singularity)**把节律打死。**entrainment(外同步)** 就是周期性扰动通过 PRC 不断相移、最终锁定到驱动周期。
- **可执行参数/做法**：测 PRC=在各相位打一下光脉冲、记稳态后的相移；能被稳定 entrain= 驱动周期落在该钟的**夹带范围(range of entrainment)**内。
- **适配**：给 PE 一套**比 phase-lead 更细粒的诊断工具**：对进化出的个体测 PRC，看它是否能被环境周期 entrain、夹带范围多宽；能 1:1 entrain 且相位领先才是真预判。与 [PE-ANTI-1][PE-CIRC-1] 串起来。
- **决策钩子**：`PE 除 phase-lead 外加测 [PE-CIRC-5] PRC：在各相位打扰动看相移、看夹带范围，能被环境 1:1 entrain 且相位领先才算真钟，比单看相关严谨。`

### [PE-CIRC-6] 温度补偿：区分"真钟"与"普通生化振荡"的定义性质
- **来源**：综述 Johnson, Egli & Stewart 2008；PLOS Biology 2007, *Elucidating the Ticking of an In Vitro Circadian Clockwork*（https://journals.plos.org/plosbiology/article?id=10.1371/journal.pbio.0050093）。
- **总结**：真正的昼夜节律钟有三大定义性质：① 恒定条件下**自持振荡(~24h)**；② 可被环境**entrain**；③ **温度补偿**——周期几乎不随温度变（Q10 ≈ 0.8–1.2，近 1），而普通生化反应速率 Q10 常 2–3。温度补偿是区分"真钟"与"随温度快慢的裸振荡"的关键。
- **可执行参数/做法**：在几个环境"温度"下测周期，算 Q10=(周期@T)/(周期@T+10°)；接近 1 才算温补。
- **适配**：若 PE 环境有"温度/代谢速率"类变量，可加一个**温补判据**：真节律个体的内生周期不该随代谢速率大幅漂移。这比"能振荡"更严、更难作弊，能挤掉"伪钟"。
- **决策钩子**：`PE 若环境含温度/代谢速率变量，加 [PE-CIRC-6] 温补判据：真钟周期 Q10≈1(不随速率变)，能挤掉"随速率快慢的伪振荡"，比单看能否振荡严得多。`

### [PE-KURA-1] Kuramoto 耦合振子：弱耦合即可群体同步
- **来源**：Kuramoto 1975；综述 Strogatz 2000, *From Kuramoto to Crawford*；Dörfler & Bullo 2011, *On the Critical Coupling for Kuramoto Oscillators*（https://arxiv.org/abs/1011.3878）。
- **总结**：N 个有不同固有频率 ω_i 的振子按 dθ_i/dt = ω_i + (K/N)Σ_j sin(θ_j−θ_i) 耦合。存在**临界耦合 K_c**：K<K_c 各转各的(序参量 r≈0)，K>K_c **自发同步**(r→1)；无限均匀分布下 K_c=2/(πg(0))(g=固有频率密度)。是“个体钟→群体同步节律”的最简数学模型。
- **可执行参数/做法**：序参量 r·e^{iψ}=(1/N)Σ_j e^{iθ_j} 量化同步度(0=散,1=全同相)；固有频率越集中(方差小)→K_c 越小越易同步。
- **适配**：PE 群体若各体演化出自己的钟，**群体是否涌现同步节律**可用 Kuramoto 序参量 r 量化；想群体协同(同步觅食/休眠)需个体间有耦合通道(感知彼此/共享环境相位)且耦合过 K_c。给 PE 一个“群体节律涌现”的度量与门槛。
- **决策钩子**：`PE 想量化“群体是否涌现同步节律”时用 [PE-KURA-1] Kuramoto 序参量 r=|(1/N)Σe^{iθ}|(0散1齐)；同步需个体间耦合(感知彼此/共享环境)且强度过临界 K_c，固有频率越集中越易同步。`

### [PE-SCN-1] SCN 耦合：噪声单细胞钟靠耦合变精准群体钟
- **来源**：综述 Welsh, Takahashi & Kay 2010, *Suprachiasmatic Nucleus: Cell Autonomy and Network Properties*, Annu Rev Physiol（https://pmc.ncbi.nlm.nih.gov/articles/PMC3758475/）。
- **总结**：哺乳动物主钟 SCN 里**单个神经元的钟又糙又噪**(周期散、易漂)，但上万细胞**经 VIP/GABA 等耦合**后，群体输出**极稳、抗扰**的 24h 节律——精度来自网络耦合而非单细胞；耦合还赋予对光 entrain、季节编码等群体特性。
- **可执行参数/做法**：机制启示——把“精准钟”做成**多个粗糙振子+耦合**，比追求单个完美振子更鲁棒、更易演化。
- **适配**：与 [PE-KURA-1] 配套的直接设计启示：**别指望单个体长出完美钟**，群体耦合能把一堆噪声钟“平均”成稳钟。PE 个体钟方差大时，先看能否引入个体间耦合(共享环境相位即一种弱耦合)而非逼单体更准；呼应 [PE-CIRC-4] KaiABC“小网络也能出钟”。
- **决策钩子**：`PE 个体钟太噪/不稳时参 [PE-SCN-1]：上万粗糙单细胞钟靠耦合→稳准群体钟，精度来自网络非单体；优先引入个体间耦合(共享环境相位)而非逼单体更完美。`

### [PE-COSINOR-1] Cosinor：把“有没有节律”做成统计回归
- **来源**：Cornélissen 2014, *Cosinor-based rhythmometry*, Theor Biol Med Model（https://pmc.ncbi.nlm.nih.gov/articles/PMC3991883/）；Python 库 CosinorPy。
- **总结**：把时间序列拟合 y(t)=M+A·cos(2πt/T+φ)。三参数：**MESOR M**(节律调整均值)、**振幅 A**、**acrophase φ**(峰相位)。当作最小二乘回归→给每参数**置信区间**、做**零幅检验**(振幅是否显著≠0=有没有节律的统计判定)，且适用非等间隔/稀疏数据。
- **可执行参数/做法**：已知/扫描周期 T 拟合；报 A 的 CI 与 p 值判节律显著性；多周期可用 extended cosinor。
- **适配**：给 PE 节律分析一个**统计严谨标尺**——别只看曲线“像不像周期”，用 cosinor 的振幅 CI+零幅检验客观判定个体/群体是否真有节律；acrophase φ 直接量化**相位**(喂 [PE-ANTI-1] 算 phase-lead)。与 [SH-STAT-1] 同精神:别靠肉眼。
- **决策钩子**：`PE 判“是否真有节律/相位多少”别靠肉眼，用 [PE-COSINOR-1]：拟合 M+A·cos(2πt/T+φ)，报振幅 A 的 CI+零幅检验定显著性，acrophase φ 直接给相位喂 [PE-ANTI-1] 算 phase-lead。`

### [PE-MASK-1] Masking vs 真钟：把“被环境牵着走”和“内源钟”分开
- **来源**：Aschoff 的 masking 概念；Roenneberg, Daan & Merrow 2003, *The Art of Entrainment*, J Biol Rhythms。
- **总结**：**masking**=环境直接驱动行为(光一来就动)，看着像节律其实是**被动跟随**，非内源钟；**zeitgeber**(授时因子:光/温/食)是 entrain 内源钟的输入。区分关键：撤掉 zeitgeber 进**恒定条件(自由运行)**，真钟以接近(非恰好)24h 继续**自持振荡**(free-running period≠环境周期)，masking 则立刻消失。
- **可执行参数/做法**：做“恒定条件释放”测试——关掉环境周期信号，看节律自持(=真钟,[PE-CIRC-3])还是塌掉(=masking)；真钟自由运行周期通常略≠环境周期。
- **适配**：PE **最易自欺的坑**——个体看起来跟着昼夜动，可能只是 masking(对光即时反应)，并非演化出钟。必须加**恒定环境释放判据**：关掉光周期后还能自持+phase-lead 才算真钟。直接收紧 [PE-CIRC-2]/[PE-ANTI-1] 判定，防伪钟过关。
- **决策钩子**：`PE 判“真内源钟 vs 被环境牵着走(masking)”必做 [PE-MASK-1] 恒定条件释放：关掉环境光周期，自持振荡(周期略≠24h)才是真钟、立刻塌=masking；别把对光的即时反应误判成演化出钟。`

### [PE-OE-1] 开放式进化：放弃单一目标、追踪 stepping-stone
- **来源**：Stanley & Lehman 2015, *Why Greatness Cannot Be Planned*；Stanley, Lehman & Soros 2017, *Open-Endedness: The Last Grand Challenge*（呼应 [PE-NOV-1]/[PE-POET-1]）。
- **总结**：复杂、有趣的产物常**不是直奔目标优化出来的**，而是沿新颖性/可达性的 stepping-stone 累积涌现。一味压目标函数会卡在欺骗性局部最优；开放式系统持续产生新颖且可学的挑战，才能长出真正复杂的行为。
- **可执行参数/做法**：用新颖性/多样性([PE-NOV-1]/[PE-QD-1])或共进化课程([PE-POET-1]/[PE-COEVO-1])驱动；评估看“是否持续产生新行为”而非单一适应度爬升。
- **适配**：PE 北极星是“拟真涌现”而非刷指标——本条是**方法论总纲**：单轴适应度([PE-CIRC-1])PLATEAU/穷尽时(项目已遇到)，应转向多样性/共进化/自动课程让节律作为副产物涌现，而非继续逼单一目标。统领 NOV/QD/POET/COEVO 四条。
- **决策钩子**：`PE 单轴适应度 PLATEAU/穷尽时(已发生)，按 [PE-OE-1] 开放式总纲转向：用新颖性([PE-NOV-1])/QD([PE-QD-1])/共进化课程([PE-POET-1])驱动，让真节律作为复杂行为副产物涌现，别死逼单一指标(欺骗性最优)。`

### [PE-EVOLV-1] 可进化性：演化出“更易演化”的模块化结构
- **来源**：Kirschner & Gerhart 1998, *Evolvability*, PNAS；Clune, Mouret & Lipson 2013, *The Evolutionary Origins of Modularity*, Proc R Soc B（https://arxiv.org/abs/1207.2743）。
- **总结**：**evolvability**=种群产生有益遗传变异的能力。关键发现：**连接代价(connection cost)** 压力会自发演化出**模块化**网络，而模块化**更可进化**(改一模块不破坏其他功能)、更能适应变化环境、表征更可解释。即“怎么编码”决定“能演化出多复杂”。
- **可执行参数/做法**：适应度里加一项**惩罚连接数/连接长度**(connection cost)→促模块化；或用间接编码([PE-HYPERNEAT-1])天生带规整/对称偏好。
- **适配**：PE 个体网络“长不出复杂功能/改一处崩全局”时，加**连接代价**促模块化提升可进化性——让“感知→记忆→节律”成独立模块，改节律模块不毁觅食；配 [PE-HYPERNEAT-1] 间接编码，是“为什么有的编码能长出钟”的底层解释。
- **决策钩子**：`PE 网络长不出复杂功能/牵一发动全身时参 [PE-EVOLV-1]：适应度加连接代价(罚连接数/长度)促模块化→更可进化、改一模块不毁其他；或换间接编码([PE-HYPERNEAT-1])带规整偏好。`

### [PE-ARNOLD-1] Entrainment 范围 / Arnold 舌：钟只在一定范围被牵引
- **来源**：振子 entrainment 理论(Arnold tongues)；Granada, Bordyugov, Kramer & Herzel 2013, *Human Chronotypes from a Theoretical Perspective* 及 PRC-entrainment 工作（配 [PE-CIRC-5]）。
- **总结**：固有周期 τ 的振子被周期 T 的 zeitgeber entrain，**只在 T 落入 τ 附近某范围**才能 1:1 锁相，范围随 zeitgeber 强度增大而变宽——在(强度,周期)平面上是楔形的 **Arnold 舌**；范围外则相位漂移/出现 n:m 锁相或失锁。entrainment 范围与稳定相位由 PRC([PE-CIRC-5])形状决定。
- **可执行参数/做法**：扫环境周期 T(如 20/22/24/26/28h)，看个体能 1:1 锁相的 T 范围(entrainment range)；强 zeitgeber 范围更宽；锁相后测稳定相位差。
- **适配**：给 PE 一个**更硬的真钟判据**：真内源钟应在**一段** T 范围内 entrain 且保持稳定相位(不是只在恰好等于环境周期时)，并在范围外**失锁**(证明它有自己的固有周期 τ≠T，而非纯 masking 跟随)。比单点测试更难作弊，补强 [PE-MASK-1]/[PE-CIRC-2]。
- **决策钩子**：`PE 想更硬地证真钟时用 [PE-ARNOLD-1]：扫多个环境周期 T(20-28h)测 entrainment range——真钟在一段范围内 1:1 锁相、范围外失锁(说明有固有周期 τ≠T)，纯 masking 则任何 T 都“跟随”不失锁。`

### [PE-CTRNN-1] CTRNN：进化能“长出振荡”的连续时间神经网络
- **来源**：Beer & Gallagher 1992, *Evolving Dynamical Neural Networks for Adaptive Behavior*, Adaptive Behavior（https://folk.idi.ntnu.no/keithd/master-projects/2024/beer-1992.pdf）。
- **总结**：CTRNN(连续时间递归网络)是动力系统的通用逼近器，**只需指定整体性能度量**(不需指定精确电机轨迹)，用遗传算法就能演化出有用控制器——包括**自发产生节律/极限环振荡**的网络(实验演化出趋化与运动控制器)。是“演化神经振子”的奠基工作。
- **可执行参数/做法**：每神经元有时间常数 τ_i、偏置 θ_i、连接权 w_ij；**center-crossing 初始化**(把每神经元偏置设到 sigmoid 拐点)极大提升演化出振荡的概率；适应度只评整体行为。
- **适配**：PE 若想让个体**内生**长出节律(而非外部注入时钟信号)，CTRNN 是天然基底——它能在无周期输入下自激振荡。结合 NEAT([PE-NEAT-1])演化拓扑+用 center-crossing 偏置初始化，提高“长出真内生钟”的概率；为 [PE-CIRC-2]“选预测相位才长钟”提供可演化的振子载体。
- **决策钩子**：`PE 想让个体内生长出节律(非外注时钟)时用 [PE-CTRNN-1]：连续时间递归网(τ/θ/w)能无周期输入自激振荡，center-crossing 偏置初始化大幅提升演化出振荡概率，只评整体性能即可，配 NEAT 演化拓扑。`

### [PE-GOODWIN-1] Goodwin 振子：延迟负反馈生成分子节律
- **来源**：Goodwin 1965, *Oscillatory Behavior in Enzymatic Control Processes*；Gonze & Abou-Jaoudé 2013, *The Goodwin Model: Behind the Hill Function*, PLOS ONE（https://journals.plos.org/plosone/article?id=10.1371/journal.pone.0069573）。
- **总结**：最经典的分子钟模型——**3 变量延迟负反馈**(基因→mRNA→蛋白→抑制自身转录)。数学上要产生**极限环振荡**，抑制的 **Hill 系数必须 >8**(单步协同难达，需多位点磷酸化等机制等效放大)。是转录-翻译反馈环(TTFL)型生物钟的原型，与 [PE-CIRC-4] KaiABC(纯翻译后)互为两类钟机制。
- **可执行参数/做法**：三个 ODE+一个 Hill 抑制项；振荡需 Hill n>8 或等效强非线性+足够时滞；周期由降解率与反馈强度共同定。
- **适配**：给 PE 一个**最小可演化钟结构模板**——若想在个体内显式装一个生化型钟，Goodwin 式“延迟负反馈+强非线性”是比纯 CTRNN 更有生物先验的载体；也提示**演化要长出振荡需要足够强的非线性/时滞**(对应 CTRNN 的 center-crossing 与时间常数)。
- **决策钩子**：`PE 想要“生化型可演化钟”模板时参 [PE-GOODWIN-1]：3变量延迟负反馈(基因→mRNA→蛋白→抑转录)，产生极限环需 Hill 系数>8(强非线性+时滞)，与 KaiABC([PE-CIRC-4])纯翻译后钟互补两类机制。`

### [PE-CPG-1] 中枢模式发生器/半中心振子：无周期输入也能出节律
- **来源**：综述 Marder & Bucher 2001, *Central Pattern Generators*, Curr Biol；半中心振子 Brown 1914；现代 PRE 2020 *Generalized half-center oscillators*（https://journals.aps.org/pre/abstract/10.1103/PhysRevE.102.032406）。
- **总结**：CPG 是能在**无节律性输入下自发产生节律输出**的神经回路(驱动行走/呼吸/游泳)。最简单元=**半中心振子**：两组神经元**相互抑制**+各自有适应/疲劳机制→交替放电产生节律。节律是回路结构的涌现性质，不需外部时钟。
- **可执行参数/做法**：两神经元互抑(inhibitory)+慢适应电流/突触抑制衰减→反相振荡；频率由适应时间常数与抑制强度调。
- **适配**：给 PE 一个**比单振子更易演化的节律基元**——若想群体/个体出现稳定节律行为(觅食-休息交替)，互抑半中心是简单稳健的结构先验。与 [PE-CTRNN-1] 一致(CTRNN 里两神经元互抑即可实现半中心)，为节律行为提供可解释的最小回路。
- **决策钩子**：`PE 想要“觅食-休息交替”等节律行为的最小回路时参 [PE-CPG-1] 半中心振子：两组神经元互抑+慢适应即反相自激振荡、无需外部时钟，是比单振子更易演化的节律基元(CTRNN 两神经元互抑即可实现)。`

### [PE-MCC-1] 最小判据共进化：用“活到能繁殖”驱动开放式
- **来源**：Brant & Stanley 2017, *Minimal Criterion Coevolution (MCC)*, GECCO（https://dl.acm.org/doi/10.1145/3071178.3071186）。
- **总结**：开放式搜索新流派——不显式测新颖性、不建 novelty archive，而是让**两个共进化种群**(如 agent 与迷宫)各自只需满足一个**最小判据(minimal criterion)**(如“agent 能解开某迷宫”/“迷宫能被某 agent 解开”)即可繁殖。这个**类自然约束**(“活到能繁殖”)在单次运行里就持续产出**越来越复杂、越来越多样**的迷宫拓扑与求解轨迹。比新颖性搜索更接近自然的发散机制。
- **可执行参数/做法**：两群体各设最小判据(可繁殖门槛)；满足者配对繁殖；无需行为表征/novelty 距离/archive；复杂度自发递增。
- **适配**：PE 的持久世界+群体进化天然适合 MCC——可把**环境难度**与**个体能力**做成两个共进化群体，各设“最小判据”(如个体须存活 N 天/环境须可被存活)，让节律作为“为满足最小判据而涌现”的副产物自发复杂化。比 POET([PE-POET-1])更轻(无需 novelty/archive)，是 [PE-OE-1] 开放式总纲下的极简实现。
- **决策钩子**：`PE 想要极简开放式(不建novelty archive)时用 [PE-MCC-1]：环境与个体两群体各设最小判据(个体须存活N天/环境须可被存活)即可繁殖，复杂度自发递增，比 POET([PE-POET-1])轻，让节律作为副产物涌现。`

---

# SHARED 段 — 跨项目通用方法论

### [SH-EVAL-1] 评估 / 消融方法论（防自欺）
- **来源**：综合 NEAT 消融范式([PE-NEAT-1])、HER 消融([DP-SR-1])、on-policy 大规模实证([DP-PPO-2])。
- **总结**：单点指标会骗人。可信结论需：① held-out 泛化(全新未训 bank/环境)；② 多随机种子(报均值±方差，别单种子)；③ 消融(逐个关组件证明其必要)；④ 与简单基线/oracle 对比。
- **可执行参数/做法**：DP——held-out bank 泛化 + 与 oracle(47%) 对比；PE——多种子 + 撤组件消融 + 竞争适应度([PE-CIRC-1])。
- **适配**：两个项目所有"是否升级"的门都该套这套(对应协议 §3.6 多轴防跷跷板门)。
- **决策钩子**：`任何"升级/采纳"决策先过 [SH-EVAL-1]：held-out 泛化+多种子+消融+基线对比；单种子单指标提升不足以下结论。`

### [SH-IMPL-1] 实现细节决定深度 RL 复现
- **来源**：Engstrom 2020([DP-PPO-1])；Andrychowicz 2020 *What Matters in On-Policy RL*（https://arxiv.org/abs/2006.05990）。
- **总结**：归一化(obs/reward/advantage)、初始化(正交)、裁剪(grad/value)、lr 退火这些"非算法"细节，常比换算法影响更大。比较两个方法前必须先把这些对齐，否则比的是工程不是想法。
- **可执行参数/做法**：固定一套实现细节做对照基线；任何性能差异先排除"细节没对齐"。
- **适配**：DP 调 PPO、PE 调进化超参时，先锁实现/评估管线再比较方案。
- **决策钩子**：`比较两套方案前按 [SH-IMPL-1] 先对齐实现细节(归一化/初始化/裁剪/退火)，否则差异可能只是工程噪声，不是方案优劣。`

### [SH-REPRO-1] 深度 RL 复现性危机（为什么必须多种子）
- **来源**：Henderson et al. 2017/2018, *Deep Reinforcement Learning that Matters*, AAAI（https://arxiv.org/abs/1709.06560）。
- **总结**：同一算法仅因**随机种子不同**就能跑出差异巨大甚至结论相反的曲线；超参、网络架构、reward scale、甚至不同代码库都带来大方差。只报"最好/少数种子"会严重误导。
- **可执行参数/做法**：报结果至少 **5–10 个随机种子**的均值±方差/置信区间；固定并报告种子、环境版本、超参；不同方法用**同一代码库/评估管线**比。
- **适配**：DP/PE 所有"是否升级"决策都不能靠单种子；尤其 DP 觅食 0%/47% 这种高方差场景，单次结果不足以下结论。
- **决策钩子**：`任何"变好了/变差了"的结论先过 [SH-REPRO-1]：至少 5–10 种子报均值±方差、同管线对比；单种子差异很可能只是随机性。`

### [SH-STAT-1] rliable：少 run 下的统计严谨（IQM + bootstrap CI）
- **来源**：Agarwal et al. 2021, *Deep RL at the Edge of the Statistical Precipice*, NeurIPS (outstanding paper)（https://arxiv.org/abs/2108.13264，库 https://github.com/google-research/rliable）。
- **总结**：计算贵、只能跑少量 run 时，均值/中位数等**点估计不可靠**。应用：① **IQM(四分位均值)**——去掉最高/最低 25% 再平均，既鲁棒又高效；② 报 **95% bootstrap 置信区间**；③ **性能 profile**(得分分布曲线)；④ **probability of improvement**。Atari 100k 案例重算后之前一些结论被推翻。
- **可执行参数/做法**：用 rliable 库出 IQM+CI+性能 profile；少 run 下优先 IQM 而非 mean(抗离群)或 median(太浪费样本)。
- **适配**：DP/PE 都是"跑不起很多种子"的算力受限场景——正是 rliable 针对的"few-run"例。报告/对比时用 IQM+CI 而非裸均值。与 [SH-REPRO-1] 配套。
- **决策钩子**：`算力有限只能跑少量 run 时，按 [SH-STAT-1] 用 IQM(去首尾25%再均)+95% bootstrap CI+性能 profile 代替裸均值/中位数，避免被离群/噪声误导。`

### [SH-GEN-1] Procgen：用程序生成关卡测泛化（训/测分集）
- **来源**：Cobbe et al. 2020, *Leveraging Procedural Generation to Benchmark RL (Procgen)*, ICML（https://arxiv.org/abs/1912.01588）。
- **总结**：16 个程序生成的游戏；**在有限关卡上训练、在全分布上测试**→直接度量泛化。发现：智能体会对小关卡集**过拟合**；需 **~数千万关卡 + 更大模型**才能泛化；多样的环境分布是训出可泛化策略的必要条件。
- **可执行参数/做法**：训练集与测试集用**不同随机种子生成的关卡**；报告 train-test gap；泛化差时先加环境多样性而非只加训练量。
- **适配**：DP 的 held-out bank 泛化评估本质上就是 Procgen 思路；DP "觅食在新 bank 掉崖"很可能是**过拟合训练分布**而非真会觅食——应加世界多样性(配 [DP-DR-1] 域随机化)。PE 的持久世界跨代不 reset 也应抽出测试分布。
- **决策钩子**：`DP 觅食在新 bank 掉崖先怀疑过拟合 [SH-GEN-1]：按 Procgen 做训/测分集(不同种子生成)、报 train-test gap，泛化差先加世界多样性([DP-DR-1])而非只加训练量。`

### [SH-FEP-1] 自由能原理 / 主动推断：用“减小惊奇”统一预测与行动（跨项目理论锚）
- **来源**：Friston 2010, *The Free-Energy Principle: A Unified Brain Theory?*, Nat Rev Neurosci（https://www.fil.ion.ucl.ac.uk/~karl/The%20free-energy%20principle%20A%20unified%20brain%20theory.pdf）；Friston et al. 2017, *Active Inference: A Process Theory*, Neural Comput（https://activeinference.github.io/papers/process_theory.pdf）。
- **总结**：自适应系统持续**最小化变分自由能**(≈预测误差/“惊奇”)。两条途径：① 改内部模型让预测更准(感知/学习)；② **行动**去改变世界使其符合预测(主动推断)。把感知、学习、行动统一成“用内部生成模型预测未来、再缩小预测与现实的差”。
- **可执行参数/做法**：理论框架而非单一算法；落地常见于“预测下一观测+用预测误差驱动学习/探索”(与 [DP-EXP-2] ICM、[DP-MB-1] 世界模型同源)。
- **适配**：**统一两个项目的理论锚**——DP“原始人理解世界”=学一个能预测机制耦合(下雨→灭火→冷)的内部模型并据此行动；PE“预判未来相位(phase-lead)”=个体用内部钟**预测**环境周期、提前行动减小惊奇。两者本质都是“建预测模型+提前行动”。给决策者一句框架：**凡‘预判/理解世界’的进展都可表述为‘内部模型预测误差下降’**，可作跨项目统一评估视角。
- **决策钩子**：`需要跨 DP/PE 统一“理解世界/预判”视角时引 [SH-FEP-1] 自由能原理：智能=最小化预测误差(惊奇)，靠①修模型②行动改世界两途；DP 机制耦合理解=建可预测世界模型，PE phase-lead=用内部钟提前预测环境，都可统一度量为“预测误差下降”。`

### [SH-GOODHART-1] Goodhart 定律：指标一旦成靶就不再是好指标
- **来源**：Goodhart 1975；Manheim & Garrabrant 2018, *Categorizing Variants of Goodhart's Law*（https://arxiv.org/abs/1803.04585）（呼应 [DP-HACK-1]）。
- **总结**：“当一个度量变成优化目标，它就不再是好度量。”优化压力会找到**让指标高但本意没达成**的捷径(=规范博弈)。四类变体：回归型、极值型、因果型、对抗型。本质是**代理指标 ≠ 真实目标**，优化越狠偏离越大。
- **可执行参数/做法**：用**多个互补指标**+held-out 真目标交叉验证；定期人工/oracle 抽查“高分轨迹是否真达意图”；警惕单指标飙升。
- **适配**：决策者**选评估指标时的红线**——DP 别只盯单一 reward(会刷分,[DP-HACK-1])，用 Crafter 式多成就([DP-BENCH-1])+held-out；PE 别只盯单轴适应度或相关性(会出伪钟/masking)，用 phase-lead([PE-ANTI-1])+恒定释放([PE-MASK-1])+cosinor 显著性([PE-COSINOR-1])多指标交叉。与 [SH-EVAL-1] 配套，是“为什么要多轴门”的理论依据。
- **决策钩子**：`决策者定“用什么指标判成功”时记 [SH-GOODHART-1]：单一代理指标一旦成优化目标必被钻；用多互补指标+held-out 真目标交叉验证+抽查高分轨迹是否真达意图(DP多成就/PE phase-lead+恒定释放+cosinor)，呼应多轴门 [SH-EVAL-1]。`

---

## 版本历史
- v2.2 (2026-06-01): 第三批扩充。新增 9 条（DP +5、PE +4），总计 72 条。
  - DP 新增：大规模纯好奇心(Burda/54环境/noisy-TV)、R2D2循环回放(burn-in/52-57超人)、因果混淆(de Haan/信息越多越差)、AIRL(可迁移奖励 g+γh'−h)、优先经验回放(α0.6/β0.4→1)。
  - PE 新增：CTRNN可演化振子(Beer/center-crossing)、Goodwin分子钟(延迟负反馈/Hill>8)、CPG半中心振子(互抑自激)、最小判据共进化(Brant&Stanley/无archive开放式)。
- v2.1 (2026-06-01): 第二批扩充。新增 17 条（DP +8、PE +7、SHARED +2），总计 63 条。
  - DP 新增：Dreamer/DreamerV3(RSSM想象训练/symlog/two-hot/单配置/Minecraft钻石)、MuZero(三件套+MCTS/57 Atari)、World Models(VAE+RNN/梦里训)、离线RL IQL(上expectile τ0.7-0.9)/CQL、RLHF偏好奖励(Christiano/<1%交互)、分层RL Options/FeUdal、Empowerment(动作-未来互信息)、GAE(γ0.99/λ0.95偏差方差)。
  - PE 新增：Kuramoto(序参量r/临界耦合K_c)、SCN耦合(噪声钟→稳群体钟)、Cosinor(M+A·cos/零幅检验)、Masking vs真钟(恒定释放)、开放式进化(Stanley)、可进化性/模块化(连接代价)、Arnold舌/entrainment范围。
  - SHARED 新增：自由能原理/主动推断(跨DP/PE统一预测锚)、Goodhart定律(多指标交叉防刷分)。
- v2.0 (2026-06-01): 大扩充。新增 23 条（DP +13、PE +7、SHARED +3，含全文精读抽出的具体数值/超参），总计 46 条。
  - DP 新增：SAC/最大熵(自动温度α/双Q/τ0.005)、TD3(双Q取min/d=2/目标平滑σ0.2)、GAIL(判别器当奖励)、奖励黑客/规范博弈(DeepMind 60例/CoastRunners)、ICM、Go-Explore(Montezuma 43k/650k/18M)、NGU/Agent57(Pitfall 8400/57游戏超人)、PBT、Crafter(22成就几何均值)、Voyager(技能库只增不减=KB同构)、灾难性遗忘/EWC、Decision Transformer、自模仿。
  - PE 新增：HyperNEAT/CPPN、进化策略(OpenAI-ES 10min humanoid/CMA-ES)、MAP-Elites、捕食-猎物共进化(红皇后/CIAO)、KaiABC 体外蛋白钟(3蛋白+ATP/无转录)、PRC/entrainment、温度补偿(Q10≈1)。
  - SHARED 新增：Henderson 复现性(多种子)、rliable(IQM+bootstrap CI)、Procgen 泛化(训/测分集)。
- v1.0 (2026-06-01): 初版。DP 13 条 + PE 8 条 + SHARED 2 条，覆盖 PPO 实现/超参/排查、BC·DAgger·VPT、PBRS、RND、HER、KL 微调、域随机化、课程；NEAT、新颖性搜索、POET、circadian 三条实证(共振/在硅演化/自持)、POMDP 记忆、phase-lead 预判判据；通用评估与实现方法论。每条含可执行参数 + 决策钩子。
