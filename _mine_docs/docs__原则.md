# deep_protagonist 训练原则（autonomous 模式，长期守则）

> 用户在 D-064 训练中途明确提出。所有 D-0XX 决策必守。

## 原则

### 1. 不要为了某个指标，给太直接的奖励
- ❌ 反例：D-064 给 "per-hit prey +0.5" — 直接奖励"打 prey"动作 = 在写指令而不是让 PPO 悟
- ✅ 正例：D-063 起手包 = 改变环境结构（降低探索成本），不改 reward shape
- 偏好顺序：**结构改动 > 课程解锁 > PBRS（潜力差）> sparse milestone > per-event shaped**

### 2. 不要纠结单一小指标
- 蓝图覆盖度是北极星，不为某一项 +5% 而牺牲整体
- 任一指标"死活上不去"先排除非 step 原因（reward shape / obs schema / world params / 探索成本），最后才考虑加步数

### 3. 2MB shadow 网络是试水品，不为它过度调参
- 最终目标 50MB（S5/S6）；shadow 是验证机制对不对
- 沉没成本警告：少在 shadow 上 squeeze 最后 0.01 分

### 4. autonomous 决策权
- 技术细节（reward weight / obs dim / 超参 / 库）→ AI 自己拍
- 蓝图改向 / 阶段切换 / 范围扩散 → ping 用户

### 5. 每训练完一批 git 提交
- 用户指令（2026-05-28 凌晨）：每个 D-0XX 训完即 commit
- Commit message: `deep_protagonist D-0XX: 一句话改动 + last-60 关键指标`
- push 由用户管（暂不自动 push）

## 反例库（避免重蹈）

| 行为 | 错在哪 | 替代 |
|---|---|---|
| D-064 per-hit prey +0.5 | 直接奖励"打"动作 | PBRS proximity（靠近 prey 给 potential，不杀也不硬奖） |
| 蓝图改向不问 | 范围扩散 | 任何"换蓝图阶段"先 ping 用户 |
| 在已稳态 ckpt 上加 reward 改动 | 容易 PPO collapse（D-055~D-059 验证 4 次） | smoke 5K 步先验证；崩了 fresh restart |
| 在数据未足前加 step | 沉没成本浪费 | 先排除非 step 原因 |
