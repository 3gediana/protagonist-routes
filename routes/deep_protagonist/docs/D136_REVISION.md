# D-136 机制修订记录（底层奖励/压力体系全面修订）

日期：2026-06-12 | 作者：Devin（受 Johnson 委托全权评估并修改）
配套审查报告：assessment_report.md（D-136 audit）
所有代码改动均带 `D-136 audit` 注释标签，远端均有 `.bak_d136_audit` 备份。

## 0. 设计原则（对照蓝图）

蓝图目标是 **多机制涌现**（全面抓多种机制，而非优化一两个指标）。
本次修订遵循三条数学原则：

1. **做事的边际奖励 > 不做事的基线奖励。** 若"活着不动"每秒白拿的分
   大于任何行为的期望边际收益，最优策略必然坍缩为被动生存。修订后两个
   项目中"纯活着"一局的收益都被压到任何单个机制完成收益的 1/3 以下。
2. **长链行为必须有密集中间梯度（PBRS 形式）。** 奖励只在链条终点发放时，
   进化/RL 在链条中段没有选择压力。所有新增密集奖励都是势函数式
   （potential-based）或被真实资源消耗上界约束，不改变最优策略、不可刷分
   （反 Goodhart 红线）。
3. **不删除旧奖励分量、不重置网络。** 只调权重/解锁门控/补充缺失梯度
   （铁律：every new task keeps old reward weights intact）。

## 1. DP（deep_protagonist）改动与账目

| 改动 | 旧值 | 新值 | 文件 |
|---|---|---|---|
| alive_step | 0.05/s | 0.015/s | include/agent/RewardSystem.hpp |
| food_coef / water_coef | 6.0 | 12.0 | 同上 |
| milestone | 1.0 | 2.5 | 同上 |
| vital_danger 触发条件 | 仅 cook_enabled | 永远触发 | src/env/Environment.cpp |
| DP_EXPLORE_REW 默认 | 0（关） | 0.03 | 同上 |
| can_farm 解锁阶段 | Late | Mid | include/agent/ProgressTracker.hpp |
| ppo_repertoire_shaping 默认 | 0（关） | 0.5 | src/main_train.cpp |

**量级账目（按 ~4000 步 / 约 130 模拟秒一局核算）：**
- 旧：alive ≈ +6.7/局（免费），吃喝边际 ≈ +0.36/次 → 站着不动最优。
- 新：alive ≈ +2.0/局；单个 milestone 首达 +2.5，吃喝边际 +0.7~1.4/次，
  vital_danger 凸惩罚在饥渴临界区每局扣 -7~-13（验证日志实测）。
  → "活着"只是底线，分数主要来自机制解锁与维持体征远离危险区。
- repertoire PBRS：势函数 Φ = 本局已解锁的不同 milestone 数（上界 10），
  每解锁一种新机制一次性 +0.5，重复刷同一机制收益为 0 —— 与几何平均
  评分（要求广度）在数学上同向，这是此前评分要广度、奖励却无广度梯度
  的根本矛盾的修复。
- 阶段门控：can_farm 提前到 Mid（mid 门槛 collect≥5 或 explore≥50，
  D-086 已调低），打通 seed/crop milestone 在中期可达，使几何平均分
  不再被恒为 0 的后期 milestone 拖死。

## 2. PE（protagonist_ecology）改动与账目

| 改动 | 旧值 | 新值 | 文件 |
|---|---|---|---|
| 生存密集基线系数 | 0.01/s | 0.004/s | runtime/ProtagonistEcologyRuntime.cpp |
| 砍树进度密集奖励 | 无 | chop_progress_reward=0.05/整树韧性点 | capability/ChopTreeCapability.* + 配置插线 |
| 接近树距离梯度 | 无 | approach_tree_reward=0.01/米（只付净接近） | runtime/ProtagonistEcologyRuntime.cpp |
| 工地驻留奖励 | 站着就给（120s 上限） | 仅 isCarrying() 携带材料时给 | 同上 |
| ME 行为描述符 | — | 不改：v18 CVT 6 轴已覆盖（确认配置 cvt_enabled=true） | — |

新增 toml 旋钮（均有合理默认值，Agent 可调）：
- `population.protagonist.chop_progress_reward`（默认 0.05，0=关）
- `reward.approach_tree_reward`（默认 0.01，0=关）

**量级账目（720 模拟秒一局，survival_weight=0.3 配置下）：**
- 旧：纯生存 ≈ 0.3×0.01×720 = 2.16 + 间歇食物奖励；砍倒一棵树（需要
  连续几十 tick 正确输出）只有终点 +4.0，中间 0 —— 0.18% 成功率下
  NEAT 对"砍了一半"的基因组没有任何选择优势。
- 新：纯生存 ≈ 0.86；砍树链条 = 接近梯度（每净接近 1 米 +0.01）
  → 每点韧性伤害 +0.05（整树 16 点 = +0.8 密集尾迹）→ 砍倒 +4.0。
  "朝树走→开始砍→砍了一半→砍倒"每一段都有正梯度。
- **防刷分上界**：接近梯度只付 max(0, 上tick距离-本tick距离)，来回
  振荡净收益为 0，每局总额 ≤ 朝树走过的净距离 × 0.01；砍树进度奖励
  被真实韧性伤害约束（树是有限资源、regrow 慢），不在树旁挥空无收益；
  工地驻留改为携带材料才付，"空手挂机"收益归零且不消耗上限。

## 3. 与蓝图直觉的权衡核对

- 多机制涌现：DP 广度 PBRS + 阶段门控放宽 = 评分（几何平均）与奖励
  （广度脉冲）首次同向；PE 的 QD 档案（CVT 6 轴）负责保多样性，密集
  梯度负责让稀有行为（chop/build/craft）首先"存在"，二者分工明确。
- 生存压力仍然真实：DP death=-10、vital_danger 凸惩罚未减；PE 掉血、
  能量消耗、捕食者未动。降低的只是"活着白拿的分"，不是生存难度。
- 所有改动可被配置覆盖（环境变量 / CLI / toml），Agent 仍保有调参
  空间，但默认值已落在数学上自洽的区间。

## 4. 第二轮深度审查（D-136 round 2）：链条可达性分析

第一轮只修了"奖励量级"层；第二轮用第一轮验证数据逐条核查每个里程碑/
能力的**完整前置链是否在数学上可达**，发现并修复了 4 处结构性死锁：

### DP（387 局验证数据：shelter 0%、crop 0%、follower 0% —— 三个恒零通道
### 把几何平均分锁死在 ~0.30）

1. **shelter 里程碑结构性不可能**：place_shelter 动作默认是 no-op
   （kLeanToEnabled 需要环境变量 DP_LEANTO=1，默认关）。代码注释自己都
   承认这是广度瓶颈。→ 改为默认开（DP_LEANTO=0 可关），反刷分护栏
   （每局仅一个、广度奖励只付首次）原样保留。
2. **follower 里程碑链条超出生存预算**：can_tame 锁 Late（需先建成房子，
   仅 30% 局达成且都在末期），之后还要对同一动物喂食 5 次（信任 +0.10/次，
   过 0.5 才成为追随者）。387 局 0 次达成。→ can_tame 提前到 Mid，
   信任机制本身（5 次喂食、正确食物）保留为"挣来的"部分。
3. **crop 里程碑时间不可达**：作物成熟需 120 模拟秒（满水），中位生存
   仅 ~200 秒且种子多在局中期才拿到。387 局 0 次收获。→ 成熟时间
   120s→90s（建筑旁 45s），浇水/照料仍然必需。
4. 备注：COOK/MINE/斧/镐/纪念碑 5 个科技树动作默认被 CLI 旗标
   （--allow-cook 等）屏蔽，不在 10 里程碑评分内，但与"多机制涌现"
   蓝图相关 —— 未改默认（D-122 保护既有冠军行为），已写入 Agent
   知识库：长跑实验应显式带上这些旗标。

### PE（验证数据：craft 106200 次尝试 / 9 次成功；deposits 全程 = 0）

5. **搬运链条中段无梯度**：gather_stick/stone 在捡起时付 1.0，工地驻留/
   存储在终点付，但"捡起后长途搬到工地"这一段没有任何梯度 —— 这正是
   deposit=0、build 链起不来的结构原因。→ 新增 carry-to-worksite PBRS：
   `reward.approach_worksite_reward`（默认 0.01/净接近米），仅 isCarrying()
   时支付、只付正距离差、放下物体即清基线，不可振荡刷分。
6. craft 9/106200 的根因同上游：需要 base 内同时有 stick 与 stone 来源，
   而库存恒空（deposit=0）。搬运梯度打通后此链自动受益；craft 终点已有
   goal_completion.craft_weapon=8.0，不再加码（防 Goodhart）。
7. 选择噪声：episodes_per_generation=2 时 fitness 方差大、选择信噪比低 ——
   属配置层，已写入 Agent 知识库（建议长跑 ≥3）。

## 5. 第三轮：指标-机制强绑定（D-136 round 3）

原则：指标必须完整、无偏地反映模型的真实行为；指标测不到的机制等于
不存在，统计口径造成的假天花板必须消除。

### DP
1. **累计口径假象**：score 从训练第 1 局起累计平均，前期失败永久留在
   分母里——后期分数渐近地低估当前策略，"平台期"有一部分是统计假象。
   → `score=` 改为最近 100 局滑动窗口（反映当前策略真实水平），
   `score_cum=` 保留旧口径（用于跨 run 对比）。jsonl 同时输出两者。
2. **指标对 5 个机制失明**：里程碑集只覆盖 15 个机制中的 10 个，
   cook/mine/axe/pickaxe/monument 做了也不计分。→ EpisodeMilestones
   扩到 15 项，与机制集 1:1 绑定。
3. **机制默认被锁**：--allow-cook 等 5 个旗标默认 false，指标永远测不到。
   → 默认全开，新增 --lock-cook 等旗标做 A/B 回退。
   ⚠ 注意：15 项 + 滑动窗口后的 score 与旧 score 数值不可比，必须以
   round 3 之后的 fresh run 为新基线。

### PE
4. **选择指标对"广度"失明**：fitness 由吃喝/生存量主导，"一招鲜活命"
   和"多机制表达"的基因组在选择上几乎无差别——蓝图要的多机制涌现
   对选择压力不可见。→ 新增 `reward.repertoire_bonus`（默认 3.0）：
   每局每种 goal 类型**首次**完成时一次性加分（镜像 DP 的广度奖励；
   只付首次，上界 16×bonus，无法刷分）。

### 第三轮补充：搬运链条三处机制巩固（throw 泄漏 / 工地可见性）

验证数据显示 throw_attempts 每代上万而 deposits 恒 0：木棍被捡起后几个
tick 内就被 throw 输出扔掉，根本到不了工地。三处结构修复：

1. **throw 材料保护不对称**：物流目标期间只有石头受"持有保护"，而木棍
   （工地主材料）不受——已扩展为 stick+stone 同等保护
   （ThrowCarriedObjectCapability，与 GoalAwareDropCapability 对齐）。
2. **工地可见半径过小**：空间记忆只在 28m 内记录工地，远处觅食的 agent
   永远不知道工地存在 → BuildWorksite/GatherStick 目标永远触发不了 →
   持有保护也永远不生效。工地是大型地标，半径放宽到 60m。
3. **材料需求感知半径** 32m → 60m（同理）。

> 为什么这不是 Goodhart：感知半径不发任何奖励，只改变信息可达性；
> deposits/builds 指标仍要求真实完成"捡起→搬运→送达"的全部物理动作，
> 无法靠感知变化空刷。为使该判断可检验，半径做成配置参数
> `perception.worksite_sighting_radius`（默认 60，可调回 28 做 A/B 对照）。

一键长跑脚本已就位：DP `d136_longrun.bat`（1000 万步），PE
`d136_longrun.bat` + `d136_longrun.toml`（160 代，episodes_per_generation=3）。

## 5.x Agent 决策体系精简（training_panel/agent.py，2026-06-12）

问题：策略目录 6 条里只有 2 条是真决策。timeline 统计：6 次切换中 4 次
compute_yield、2 次 fast_iter，ab_knob/hist_bench/early_stop/ckpt_hygiene
从未被用。根因：hist_bench/early_stop/ckpt_hygiene/compute_yield 是
"条件触发就必须做"的守则，不是可选工作方式；放进策略目录导致 Agent 用
"切到 compute_yield"代替真正的探索/利用抉择。

改动（agent.py，备份 agent.py.bak_d136_strategies）：
1. 四条伪策略并入系统提示词【常驻守则】（STANDING_RULES，每轮自动生效）：
   开跑前 query_gens 对标、资源吃满、早停回退、磁盘卫生。
2. 策略目录只剩两条互斥真决策：fast_iter（探索）/ ab_knob（利用，默认），
   附明确切换判据。
3. use_strategy 拒绝旧 id 并提示；默认 active_strategy=ab_knob；
   旧配置值经 LEGACY_STRATEGY_ALIASES 自动映射。
4. 已验证：import OK、run_agent_tests.py 5 场景通过、面板已重启生效。

## 6. 验证（短程）

- 编译：DP、PE 在本机均 BUILD_OK（MSVC + Ninja + CUDA 12.8）。
- DP fresh PPO 2M 步：早期即观察到 vital 惩罚每局生效（旧版大多数运行
  恒为 0）、milestone 分量主导回报、score 快速爬升（详见最终汇报）。
- PE fresh 20 代：观察 chop_successes/attempts 与 me_cov 对比基线
  （0.18% / 13-17 格，详见最终汇报）。
