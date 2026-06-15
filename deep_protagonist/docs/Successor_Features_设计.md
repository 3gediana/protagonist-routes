# Successor Features 落地设计（D-122 写，留给下一个号实现）

> 状态：**设计稿，未实现**。本轮只写蓝图，下一个号照此落地。
> 一句话目的：**训一次，学出"从此刻起未来会积累多少各类奖励分项"(ψ)；之后任何新的奖励权重 w′，用 ψ·w′ 当场估出"这套配方大概值多少分"，不用重训。** 把"每改一次奖励就赌 4~6h"变成"秒级筛选"。

---

## 0. 一分钟背景（写给非技术的你）
我们的总奖励本来就是一堆分项的加权和：
`reward = w_alive·顶着活 + w_food·顶饿 + w_water·解渴 + w_kills·打猎 + … + w_potential·手里有家当`。
- 每个"分项"叫一个**特征 φ**；前面的旋钮叫**权重 w**。
- SF 学的不是"分数"，而是**"未来会累计多少每一种 φ"**这一串预测，记作 **ψ**（successor features，继承特征）。
- 学会 ψ 之后：想试"把打猎权重 ×3 会怎样"，不用重训——`新价值 ≈ ψ · 新权重`，点个乘就出来。
- 这就是你说的"花一份算力、读出多种奖励配方下的结果"。**前提：奖励是 φ 的线性加权和——我们正好是。**

---

## 1. ① 选哪些特征 φ（直接复用现有奖励分项，零发明）
现成的 `RewardSystem::StepReward`（见 `include/agent/RewardSystem.hpp`）已经把每步奖励拆成 12 个具名分项，**这就是天然的 φ 向量**：

| idx | φ 分项 | 来源字段 | 现权重 w |
|----|--------|---------|---------|
| 0 | 顶着活 alive | `alive` | 0.05/s |
| 1 | 顶饿 food | `food` | food_coef 6.0 |
| 2 | 解渴 water | `water` | water_coef 6.0 |
| 3 | 打猎 kills | `kills` | wolf_killed 0.0 |
| 4 | 挨咬 bites | `bites` | -1.0/dmg |
| 5 | 死亡 death | `death` | -10.0 |
| 6 | 体感压力 vital | `vital` | decay 0.00005 + danger 0.02(凸) |
| 7 | 采集 collect | `collect` | 0.5 |
| 8 | 建造 shelter | `shelter` | built 5.0 + deposited 1.0 |
| 9 | 制作 craft | `craft` | 3.0 |
| 10 | 里程碑 milestone | `milestone` | 1.0 |
| 11 | 家当势能 potential | `potential` | state_bonus 0.005 |

**关键设计：φ 要存"去掉权重的原始量"，w 单独存。** 现在 `StepReward` 存的是"已乘权重的贡献"。为了让 `reward = Σ wᵢ·φᵢ` 严格成立、且能换 w，需让 RewardSystem 额外暴露**未乘权重的 φ**（如 food 存 drive_reduction 本身、kills 存"这步杀了几只"），权重向量 w 从 `Weights` 读。
- 落地：给 `RewardSystem` 加 `std::array<float,F> last_features() const;`（F=12，返回未乘权重的分项）和 `std::array<float,F> weight_vector() const;`（把 `Weights` 摊平成同序的 w）。校验 `dot(last_features, weight_vector) ≈ StepReward.total()`（允许 clip 误差），作为第一条自测。
- F 先取 12；以后加新奖励就 F+1（**append-only，和训练铁律一致**）。

---

## 2. ② ψ 头长什么样、怎么接
现网络（`ActorCriticImpl`，`include/policy/PPO.hpp`）：
`obs[582] → encoder(Linear→1408) → GRUCell(1408) → h` 然后并排三个头：`cont_mean`、`disc_logits`、`value(Linear 1408→1)`。

**ψ 头 = 再并排加一个 `value` 的兄弟：**
```cpp
torch::nn::Linear successor{nullptr};   // HIDDEN(1408) -> F(12)
// ctor: successor = register_module("successor", nn::Linear(HIDDEN, F));
// 取 ψ：psi = successor->forward(h);   // [B, F]
```
- **强烈建议首版用"detached 主干"**：`psi = successor->forward(h.detach())`。
  含义：ψ 只是挂在冻结特征上的**线性读数**，它的训练梯度**不回流**到 encoder/GRU/策略头 → **冠军策略一个 bit 不动，零风险**。这直接满足红线"不许乱搞/不许动旧本事"。
- 代价：ψ 的表达力被现有特征上限锁死（够用于"筛配方"）。等确认有价值，第二版再放开主干（去掉 `.detach()`、小 `sf_coef`）让特征为 ψ 服务，但那会扰动策略，需 A/B 重验。
- `forward_step` 不动签名（避免牵连全代码），单加一个 `torch::Tensor psi(const torch::Tensor& h)` 方法即可。

**存档兼容**：老存档（`r1_cook_s24*.pt`）没有 `successor` 参数。加载时用"缺失即新初始化"（`load_state_dict` 容错 / 手动跳过缺失键），然后像 D-101 critic-only 预热那样，**先只训 ψ 头几百步**把它从随机喂到收敛（策略/价值此时被 detach 保护，纹丝不动）。

---

## 3. ③ 用什么 TD 规则训 ψ
ψ 满足和价值函数同构的 Bellman 方程，只是把"标量奖励"换成"向量 φ"：
```
ψ(s_t)  ←  φ_t  +  γ · (1 - done_t) · ψ(s_{t+1})
```
- γ 复用现有 `PPOConfig::gamma = 0.995`。
- 损失：`L_ψ = MSE( ψ(s_t),  [φ_t + γ(1-done)·ψ(s_{t+1})].detach() )`，对 F 维取均值。
- 在现有 `PPO::update` 的 minibatch 循环里加这一项：`total_loss += sf_coef * L_ψ`（`sf_coef≈0.5`）。
  - **detached-主干版**：L_ψ 只更新 `successor` 头自己的参数 → 不碰 policy/value/trunk。
- 需要的数据：每条 `Transition` 多存一个 `std::vector<float> phi;`（F 个）。`ψ(s_{t+1})` 用 rollout 里下一步的 h 重新前向（和 value bootstrap 同套路；序列末尾用 `last_value` 的同位 `last_psi` 收尾）。
- 自洽校验：训好后 **`ψ(s)·w ≈ value(s)`**（同一策略下，特征加权和的期望 = 价值）。这是 ψ 是否学对的最强体检。

---

## 4. ④ 怎么自测验证（含负例，纯 CPU、进 `--self-test`）
全部不依赖 GPU/真世界，几秒出 PASS：
1. **φ 重构**：随机一步，`dot(last_features, weight_vector) ≈ StepReward.total()`（clip 内）。
2. **ψ TD 收敛（合成轨迹）**：构造一条已知 φ 序列（如每步 φ=固定向量、无 done），解析解 `ψ = φ/(1-γ)`；跑若干 TD 更新后 ψ 应逼近解析解（误差 < ε）。
3. **done 截断**：序列中插一个 `done=true`，验证该步 ψ **不把后续 φ 算进来**（bootstrap 被 (1-done) 清零）。
4. **价值自洽**：合成轨迹上 `ψ·w` 与用同 w 直接累计的折扣回报一致。
5. **负例·换权重必须改排名**：两套 w 喂同一条 ψ → `ψ·w_A` 与 `ψ·w_B` 给出**不同且符合直觉**的价值（把 kills 权重调高，含打猎的状态价值必须上升；不变就是 bug）。
6. **负例·全零特征**：某分项在数据里恒为 0（如当前 kills），其 ψ 分量必须 ≈0，**且不许污染别的分量**（防串扰）。

---

## 5. ⑤ 非线性项怎么处理（寒夜闸门 / 平方体感惩罚）
SF 的线性假设是 `reward = Σ wᵢ·φᵢ`。我们有两处非线性，处理原则：**把非线性"烤进 φ 里"，让 φ 本身已是非线性算好的标量，于是对 w 仍是线性。**
- **平方体感惩罚 `vital_danger`（凸）**：直接把"这步算出来的惩罚值"当作 φ₆ 的一部分存进去。SF 可以自由**线性缩放**它（调 `vital_danger_coef`），但**不能改平方这个形状**——想换指数/换成立方，得重训。
- **寒夜闸门（cook_enabled / 夜间不回饥饿）**：闸门改变的是"world 动力学/φ 怎么产生"，不是某个线性权重。把"寒夜窗口内的食物/惩罚"理解成它自己的 φ 通道即可（值由闸门决定），SF 能 re-weight 它，但**不能用 SF 去搜"闸门阈值/夜长"**——那要重训。
- **一句话边界**：**线性旋钮（每个 φ 的权重大小）→ SF 免费试；非线性形状（指数、闸门阈值、什么时候触发）→ SF 不管，得真训。** 选 φ 时把所有想"免费试"的旋钮都做成独立线性通道。

---

## 6. ⑥ 和"advantage 符号仪表盘 + forced-hunt 探针"配合 → 试配方近乎免费
这是 SF 的最大实战价值，三件套组合拳：

**(a) 离线换配方秒筛（核心）**：每条 `Transition` 已存 `phi` 和 `h`。给任意新权重 w′：
- 新每步奖励 `r′_t = w′·φ_t` —— **直接点乘，零成本**（φ 已存）。
- 新价值底 `V′(s) = ψ(s)·w′` —— ψ 已训好，点乘即得。
- 用 r′ 和 V′ **离线重算 GAE advantage** → 直接读**该配方下的 `hunt_adv` 符号**（仪表盘那套 `PPO::mean_adv_for_action`，见 commit 44c6646）。
- ⇒ "试 N 种奖励配方"= 跑一遍 rollout、对每个 w′ 做点乘+离线 GAE，**秒级出 N 个 hunt_adv 符号**：负的当场掐、正的才真训。**这就是"花一份算力试多路线"的落地形态。**
- 落地：加 CLI `--sf-eval "wA;wB;wC"`，读一个 rollout 缓存 + 存档，打印每个配方的 `mean V′`、`hunt_adv 符号`、`cook_adv 符号`。

**(b) advantage 符号仪表盘（已上线，commit 44c6646）**：训练时实时读 `hunt_adv/cook_adv` 符号判单调趋势。SF 让你**离线**对未训的配方也能读这个符号。

**(c) forced-hunt 探针（待做）**：SF 有个死穴——**数据里从没打过猎(kills≡0)→ψ_kills≈0→它估不出"打猎到底值多少"**。探针通过"临时强制打猎几次"把这个行为注入数据，让 ψ 第一次学到打猎的后果；再用 (a) 去筛"加多大打猎权重才划算"。**探针补探索，SF 补调参，两把刀缺一不可。**

---

## 7. 老实交底的局限（别又口头吹大）
1. **ψ 是"当前策略"的继承特征**，不是新配方下的最优策略。`ψ·w′` 估的是"用现在这套行为、在新奖励下值多少"——是**筛选信号**，不是"免费白捡一个练好的 agent"。要真拿到 w′ 的最优体，仍需 GPI（多策略取 max）或短微调。
2. **只对线性奖励精确**（见 §5）。
3. **不解决探索瓶颈**：kills=0 时 SF 对打猎无能为力，必须配探针。
4. **detached 版 ψ 的上限 = 冠军特征已编码的信息**；想更准要放开主干、承担扰动策略的风险。

---

## 8. 红线自检（实现时照此守）
- ✅ append-only：新增 `successor` 头 + φ 通道，**不删任何旧奖励、不改旧权重、不重置网络**。
- ✅ 首版 detached 主干 → **冠军策略零扰动**（满足"不许乱搞"）。
- ✅ 老存档加载容错（缺 ψ 参数→新初始化→critic-only 式预热），与 D-101 预热同源。
- ✅ 全部带自测（含 §4 的两条负例），编译过、`--self-test` 绿了才接训练。

---

## 9. 实现清单（下一个号照着做）
1. `RewardSystem`：加 `last_features()` / `weight_vector()`（未乘权重 φ + 摊平 w），加 §4-1 自测。
2. `Transition`：加 `std::vector<float> phi;`，rollout 时填。
3. `ActorCriticImpl`：加 `successor` 头 + `psi(h)` 方法（首版 `h.detach()`）；存档加载容错。
4. `PPO::update`：加 ψ 的 TD 损失（§3），`sf_coef≈0.5`，detached 主干只更新 ψ 头。
5. ψ-head 预热模式（仿 D-101 critic_only）：先训 ψ 几百步收敛。
6. `--self-test`：加 §4 的 6 条（含 2 条负例）。
7. CLI `--sf-eval`：离线换配方秒筛（§6a），打印 mean V′ + hunt_adv/cook_adv 符号。
8. 编译 + 自测绿 + 在 `docs/决策记录.md`/`PROGRESS.md` 记落地。
