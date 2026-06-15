============================================================
★★ FINAL 2026-06-07 — DP 最终状态（本号收尾）★★
============================================================

> 最新。旧的蓝图/D100 知识与 06-06 块保留在下方查史。详见 `routes\deep_protagonist\handoff\START_HERE.md`（含 FINAL 块）。

【一句话】unlocks 卡死的 2.8 被打破 → **3.33/10**，6 种机制 98–100% 触发，整局存活 0→4。`ft_gen` 跑完完整 4M 步、干净结束、checkpoint 已存。

【为什么 2.8 是硬上限（钉到根）】
- 会打猎的底座 `ft_bc10` 23 个动作里只有 8 个是活的；shelter/plant/盖屋/科技树等动作的概率被压成 0（死动作）。**奖励再大也点不亮一个死动作** —— 这就是 2.8 天花板，不是模型笨。
- 保暖/广度/高熵三个变体 A/B 全部干净地卡在 2.8、动作数恒 8/23，证明"光给奖励点不亮死动作"。

【怎么破的（复用已验证的 BC→PPO 管线，零重编）】
1. 发现老冠军 `r1_cook_s24` 活着的动作（drink/sleep/grassdress/blueprint/water）跟 `ft_bc10`（hunt/collect/deposit/spear）正好互补。
2. 从两个 checkpoint 各录示范：demos_hv2(900k 条) + demos_cook(624.6k 条)，合计 152 万条。
3. BC 合并出全能底座 `bc_gen.pt`（3 epoch，disc_ce 5.39→5.01；冻结评测确认 10/23 动作活、5 个复活）。
4. 从 bc_gen PPO 微调（广度 1.5 + 保暖势函数 2.0 + 熵 0.02）→ `ft_gen.pt`。

【最终触发率（ft_gen）】eat 99% / drink 98% / HUNT 96% / spear 99% / sleep 98% / grassdress 98% / furcloak 98% / wear 99% / water 98%。死因：cold 15751 / food 29333。

【还没开 + 下一步】
- shelter/播种/盖屋/收成/招随从：在两个老师里都是死动作，要各自录示范再 BC（路径已验证，是后续工作）。
- collect 在 PPO 微调里退化回 0（底座本来活的，被保暖奖励带偏）。
- 下一步：给上述死机制录各自老师示范 → BC 进动作并集 → PPO，目标 5-6/10。

【checkpoint】`runs\bc_gen.pt`(全能底座) / `runs\ft_gen.pt`(最终策略)，均 48.8MB。分析器：`handoff\scripts\analyze_spectrum.py runs\ft_gen.jsonl`。

============================================================
（以下为历史内容）
============================================================


<!-- ===== BLUEPRINT POINTER (2026-06-01, 用户指令更新) ===== -->
> **当前治理蓝图已升级**：`deep_protagonist_blueprint_world_coherence.md`（同目录）+ 仓库 `outbox/decision_blueprint_world_coherence__20260601-064921.md`。
> 升级要点（用户 John 直接下达）：**机制要相互耦合**（旗舰：下雨浇灭火→无暖→夜冷→须护火/避雨/御寒），项目终态正式定义为 **原始人「理解世界」**＝跨天气/昼夜/群系把所有机制串成因果链连贯求生、多轴不退。
> 接力顺序：先修 §4 营养惰性过可生存性 gate → Stage W1 天气系统+雨灭火（单测→smoke→obs surgery 新血脉）→ §5 评估重建新世界 BC base + PPO 退火 KL 精炼 → §6 全机制多轴门立新冠军。`decision_blueprint_nutrition` 仍有效，本蓝图在其之上叠加。
<!-- ===== END BLUEPRINT POINTER ===== -->

<!-- ===== 最新状态 2026-06-06（D-128→D-135 + P0），先读这段 ===== -->
> **接手模型：开局先 SSH 上机器读 `D:\claude-code\c++\routes\deep_protagonist\handoff\START_HERE.md`**（本轮新整理的权威当前状态，UTF-8 干净，带单命令脚本 `handoff\scripts\`）。下面这条 note 是更早的历史契约，规则/红线/用户画像仍然有效，但"当前状态"以 handoff\ 为准。
>
> **D-128→D-135 三大进展：**
> 1. **D-133 随机出生点** `DP_SPAWN_RANDOM`（默认关）：解决"没机会碰猎物"，每局随机生到全图，大量局贴到猎物 0m。
> 2. **D-134 两阶段狩猎** `DP_HUNT_V2`（默认关）★突破★：猎物平时只走路+靠近不逃→能下第一矛；被扎中才冲刺~0.35s 再停下喘→补第二矛。主角两档移速(推杆≤0.6 走/>0.6 冲)，**没加动作维度，基线可 resume**。脚本老师 205 局 kills=139。
> 3. **D-135 BC→PPO**：从会打猎的老师录示范(`runs\demos_hv2_s24.bin`)→BC 克隆 `bc_hv2.pt`→PPO 微调。**必须从 BC 底座微调**(`ft_bc10.pt` kills 长出来)；从老冠军 `r1_cook_s24.pt` 微调纹丝不动。
>
> **⚠️ 假指标已拆穿**：日志 `kills=+0.0` 是**击杀奖励(反-Goodhart 设计=0)，不是击杀数**！用新加的真计数 `prey_kills=N`。之前所有"kills=0"都被骗了。
>
> **P0 涌现谱（冻结评测，详见 `handoff\P0_baseline.md`）**：23 动作只用 8–10 个；10 里程碑平均解 2.3–2.9；科技树(cook/mine/axe/pickaxe/monument)+造屋+种地+驯养全 0%；存活 0（基本冻死）；`tendfire` 狂按 416 次/局照样冻死＝**空按**。诊断＝标量爬坡→单策略坍缩，约 1/3 机制宽度。
>
> **P1 方向（待用户拍范围，方案见 `handoff\训练方案_全面涌现.md`）**：不优化单标量，改优化行为覆盖度(QD/MAP-Elites)，把狩猎势函数推广成 `phi_repertoire`（奖励一条命用过几种机制），从 `ft_bc10.pt` resume（零维度变化、安全）。
>
> **关键资产（别删，见 `handoff\PITFALLS.md`）**：`ft_bc10.pt`/`ft_bc05.pt`/`bc_hv2.pt`/`r1_cook_s24.pt`/`demos_hv2_s24.bin`。连接见 `handoff\CONNECT.md` + `03_ssh_remote_access_v2.md`（隧道地址固定、私钥可从 secret `DEVIN_SSH_KEY_SANS` 重建）。
<!-- ===== END 最新状态 ===== -->

<!-- id: note-3d82ad11da0144e5820392972e4e6417 | name: deep_protagonist 项目契约 | author: user | scope: 当用户提到 deep_protagonist、PPO、libtorch、C++ 课设、原始人/agent 强化学习、3D 生存模拟、或要在 D:\claude-code\c++\routes\deep_protagonist 下工作时 -->

> Note ID: `note-3d82ad11da0144e5820392972e4e6417`
> Trigger: 当用户提到 deep_protagonist、PPO、libtorch、C++ 课设、原始人/agent 强化学习、3D 生存模拟、或要在 D:\claude-code\c++\routes\deep_protagonist 下工作时
> 导出时间：2026-05-31 ~13:30 UTC（round_22 收尾版：冠军仍=**bc_v9**（永久锁定）；BC 夜宿主线 v10–v14 **五连 NO-GO** 已证伪；S4 PPO 夜宿（KL 锚定 bc_v9 + mask attack/fire）首个不崩但 Φ_day 扫描呈**结构性跷跷板**→旧世界 known-gap=c1；按用户蓝图已切**营养系统双条**主线（round_20→22）。**⚠️ round_22 §4 发现营养机制在实时 loop 里惰性、训练前必修**，详见下方「当前状态（round_22）」一节最前。）

---

# deep_protagonist 项目契约

> 用户的 C++ 课程设计：单 agent + PPO + libtorch 在 1024m 真 3D 自然世界自学求生。
> 项目位置：**`D:\claude-code\c++\routes\deep_protagonist`**（用户 Windows 电脑，SSH 连接见另一条 note）
> 平行项目：`protagonist_ecology`（NEAT 群体路线）
> **最后同步**：2026-05-29 ~20:00 UTC（D-101 BC→PPO finetune 完成，得"双墙"结论：BC 可靠教会建房但 PPO 自训练会侵蚀过夜。用户否决"古法自训练"，拍板走 DAgger（机器人纠错）+ 精致 oracle 决策树两条杠杆。D-102 实证 DAgger 单调有效：camp 达标 0→6→12/30，逆势逼近多数。D-103 把本地 BC 训练提速 ~7.2×（两轮纯吞吐优化、数学等价、reward/数据/超参全没动）。下一步：BC warm-start 增量训练 + 继续 DAgger r4→bc_v6 推过多数。以 SSH 上去读 `docs/PROGRESS.md` / `docs/D102_STATUS.md` 为准）

---

## 🔄 接力规则（多账号 handoff，绝对先读）

用户多账号轮换，本号烧完下号接力。**新会话开局必做 4 步**：

1. **SSH 上去** `Get-Content docs\PROGRESS.md -TotalCount 50` — 拿最新 D-0XX 状态、in-flight 训练状态、未闭环蓝图项
2. **看本 note** — 拿规则 / 红线 / 训练铁律 / 蓝图覆盖度表
3. **翻 `docs\决策记录.md` 末尾 3-5 个 D-0XX 决策**（500 字 5 件事，含 ①做啥 ②参数 ③学到啥 ④坑 ⑤多久）
4. **检查 `runs\ppo_d0*_2M.jsonl` 最后 60-100 行 + ckpt 时间戳** — 拿到上一稳态指标 + 下一轮 resume 锚点

**接力守则**：
- 不要 reset / 重做上一号已完成的工作
- 用户授权 "$270 ACU 随便烧 / 一直跑一直修不要停 / 不用问"，接力号继承这个授权直到用户明说停
- 决策权（2026-06-03 用户升级）：**AI 有这个项目的完全决策权**——技术细节、方向、阶段切换、范围、参数全自己拍，不必问、不必等签字。用户原话「别问我自己决策」「你有这个项目的完全决策权」。只在最终交付或用户明令停时 block。
- **持续维护四件事**：(a) Knowledge note (本) (b) docs/PROGRESS.md (c) docs/决策记录.md (d) docs/术语速查 + 参数速查（如有新词）
- **每轮决策结束都刷一次** Knowledge note + PROGRESS.md，不是只在 session 末

---

## 用户画像（极重要）

- 大一，**零 RL/DL 基础**，不读代码
- 看效果不看术语
- 时间/资源充足，RTX 5060 Laptop 8GB
- 痛点：上个项目"决策完不知道决定了啥"，不能重演
- C++ 课程设计，做到极致

---

## ⭐ 用户做事风格（autonomous 工作约定，本号必懂，违反 = 失职）

用户多次明说的工作方式，**所有接力号必须按这套做**：

### 一直跑一直修，不要停
- **"$270 ACU 随便烧 / 一直跑一直修不要停 / 不用问"** — 不要在中途停下来等用户确认细节
- 用户出门/上课/睡觉/换号期间, 接力号自己干主线, 不要等
- 不要因为"任务长"或"重复"就找理由停, 持续 polling + 持续 commit 是默认

### 自己做决策, 不要问废话
- **"你自己做决策行不行"** (2026-05-28 D-082 launch 前用户语)
- 技术细节 (算法/超参/seed/step depth/启动/清理) 全部 AI 拍, 不开 user_question
- **2026-06-03 升级：连"方向/阶段切换/范围"也归 AI** — 用户明确「你有这个项目的完全决策权」「别问我自己决策」。原先要 ping 的"蓝图改向/阶段切换/范围扩散"现在也自己拍。
- 仅在以下情况 ping 用户:
  - 缺凭证 / 用户机器需要操作 (chisel 重启, secret 等)
  - 最终交付汇报 / 用户明令停
- **不要为请示而请示** — 决策报告写 5 件事入 决策记录.md 即可

### 加速不可能就赶紧回主线
- **"不是我让你加速, 只是想一劳永逸... 能加速就加速, 不能加速赶紧回到主线"** (D-081 后用户原话)
- 不要为了优化某个工程指标 (GPU util, wall time, throughput) 而牺牲质量
- 已实证: D-077~D-081 速度优化全部 -36%, 已红线锁死
- 凡是结构性 / device / batch / parallel 改动都先小规模 smoke test, 一旦 quality 掉就立刻 revert

### 每训完一批必 commit
- **"此外, 每训练完一批, 记得提交"** (2026-05-28 凌晨用户语)
- 每个 D-XXX 训练完, 在 docs 更新后**必须 git commit** (本地, 不 push)
- commit message 格式: ``deep_protagonist D-0XX 一句话改动 + 关键指标 (score/drink/hunt 等)``
- push 用户自己管, AI 不主动 push

### 不要为指标给太直接的奖励
- **"不要为某个指标给太直接的奖励"** (D-064 后用户原则)
- 实证: D-064 加 per-hit kill reward → col 68%→37% 全崩 (reward-hacking)
- 启发: 用结构性改动 (env / curriculum / starter kit) 引导 PPO 自悟, 不要直接给行为奖励
- D-065 起所有改动都走结构方向: 起手包 / spawn 位置 / thirst_decay 等
- D-099 例外：用户授权"自主推进+改架构改算法都行+不能松口"，在家 r_alive 翻倍是边界 push（仍属结构改动，不是单纯加 reward），但 PPO 仍失败 → 直接进 D-100 oracle + BC 路线

### 维护四件事, 不是 session 末才做
- **"你不仅得维护本地文档还有云端那个 note, 你也要持续地去维护"** (用户原话)
- 每轮决策结束都刷 4 件事:
  1. **Knowledge note** (本) — 用 ``suggest_knowledge`` 提议 update, **value 字段必须 inline 真实 body 内容, 不要写 file:/// 占位符** (2026-05-28 实证 file:/// 不被 suggest_knowledge 自动展开, 那是 devin_mcp 的特性)
  2. **docs/PROGRESS.md** — 末尾追加 D-XXX 行
  3. **docs/决策记录.md** — 500 字 5 件事
  4. **docs/术语/参数/Q&A 速查** (有新词才加)
- 不能"等到 session 末统一刷"或者"等到 user 回来再刷"

### 清理日志, 不要爆磁盘
- **"你记得清理自己的日志"** (2026-05-28 早晨, 用户 160GB 日志事件后)
- 每 D-XXX 训完跑 ``scripts\prune_runs.ps1`` (保留最近 5 个 + gold ckpt)
- 不要把 deep_protagonist 日志写到 ``c++\tmp\`` 或 ``c++\data\`` (那是 ecology 的)

### 不接受冗长反馈
- 用户喜欢**简短直接**的报告, 不要铺垫 / 不要"我会..."开头 / 不要解释显而易见的事
- 关键数据放上面 (score, 对比, 决策), 长 explanation 放下面
- 不要用 emoji / checkmark ✅, 用 ✓ / ❌ / 表格

### ScriptedPolicy 不能死板 (D-099 新增, 用户原话)
- **"不要太死板了 ... 不同条件下是不成立的 ... 你得多嵌套几层 ... 把各个场景都覆盖到 ... 本质上他还是一个那种反应机嘛, 你得多嵌套几层, 让它复杂起来"**
- 任何作为 BC demo source 的脚本机器人必须:
  - 多层嵌套决策树 (D-100 ScriptedSettler = 7 层)
  - 每层多 sub-condition (按 day/night, 距离, vitals, inventory, threat 等分支)
  - Pure obs→action 反应式, 不存绝对坐标, 不存 episode 历史
  - 不同 seed 走不同路径但同决策逻辑
- 单条件"见 X→做 Y"的脚本 会让 BC 学到 surface mapping 而不是机制关联

### 蓝图扩展是终极目标 (D-100 用户拍板)
- **"我希望你创造一个机器人,带它完整的跑满这个世界的每一个集聚,让它对每一个集聚都有反应"**
- 当 D-101 BC + PPO finetune 验证可行后, 路线: D-102 ScriptedFarmer (S4 农耕) → D-103 ScriptedCrafter (S5/S6) → D-104+ ScriptedMaster (S7-S10 跨 biome / 山洞 / 河流 / 暴雨 / 多 site 村落)
- 每一轮: scripted oracle → BC 增量训练 → PPO 微调 → 验证机制 (S 阶段) 学到
- 增量, 不推翻前轮 (BC 网络可以堆叠)

### 接力号继承授权
- 本号烧完下号接力, 上面所有授权 ("一直跑 / 不用问 / 自己决策") 直接继承, 不需要新会话重新 ping 用户确认

---

## 协作规则（2026-06-03 用户大幅放权后）

> 用户原话：「基本上没有什么红线。根本就没有什么红线，唯一的红线可能就是不能够让你们去阉割功能去乱搞。」
> ⇒ 治理类"必须问/必须签字/必须写满 500 字才能动"的旧红线**全部作废**，AI 有完全决策权。下面保留的只是"怎么做"的工程纪律（= "不许乱搞"的具体化），不是"要不要问用户"的门。

1. **术语必解释**（仍保留，给用户看的）：第一次出现的 AI 词（PPO、batch、entropy 等），当场一句大白话 + 入 `docs/术语速查.md`
2. **决策记账**（不再是签字门）：拍完板直接做，但把 5 件事（①做什么 ②算法/参数 ③学到啥 ④坑 ⑤多久）补进 `决策记录.md`，方便复盘。**不必问"开始执行吗"、不必事前复述等签字。**

---

## 红线（2026-06-03 用户重定义：基本无红线，唯一红线=不许阉割功能/不许乱搞）

> 🔴 **唯一的真红线（用户 2026-06-03 亲口）：不许阉割功能、不许乱搞。** 具体化：
> - ❌ 把功能做成假的/占位/no-op 却口头汇报成"已做"（伪造交付 = 最严重的乱搞）
> - ❌ 为了让指标好看而缩水机制、删功能、改测试迁就代码
> - ❌ 重置神经网络 / 删旧奖励权重（训练铁律，灾难性遗忘）
> - ❌ 为某个指标直接发奖刷分（Goodhart，D-064 已证）
> 以上之外**没有别的红线**——方向、算法、参数、阶段切换全部 AI 自己拍，不必问用户。

下面是"怎么做"的工程纪律（避免重复踩坑，不是"要不要问用户"的门，可按需自行权衡）：
- 术语必解释、速查里别用术语解释术语、文档别啰嗦
- 别偏离 `docs/最终蓝图.md` 的"不做的事"清单（那是用户砍过的范围，属"不许乱搞"）
- ❌ **主动加任何 HUD / mini-map / debug overlay / 视觉化**（D-038 禁令）
- ❌ push（用户自己管）；**commit 每个 D-XXX 训完后必做**（2026-05-27 用户新规）
- ❌ **过度调 2MB shadow 网络**（沉没成本警告，2026-05-27 用户新原则）：最终目标 50MB（S5/S6），shadow 只是试水，**先排除非 step 原因再加 step**
- ❌ **速度优化追求**（2026-05-28 D-077~D-081 5 次实证）：任何 device/batch/parallel 改动都破坏 PPO 收敛。**接受 D-076 96-107 min 单 env GPU 为永久 baseline。** 红线下细：
  - ❌ VecEnv 多/单种子并行训练（D-078/D-079/D-080 三次 -36% quality）
  - ❌ CPU mode inference `--cpu`（D-081 -36% quality）
  - ❌ profile timers 留在 prod code（D-081 已 revert，下次也别留）
- ❌ **continued-training `--load ckpt`**（D-066~D-070, D-075 共 6 次 PPO collapse 实证）：所有 D-XXX 必须 fresh random init
- ❌ **per-hit shaped reward**（D-064 实证 col 68%→37%, reward-hacking）：用户原则 "不要为某个指标给太直接的奖励"
- ❌ **依赖环境压力调 PPO 定居**（D-091~D-099 8 次实证）：cold pressure / 食物可用度 / shelter 大小 / obs 信号 / vital regen / 折扣率 / r_alive bonus 全失败. PPO 在 attractor (出门觅食 +5/min) 锁死, 探索无法逃出. **解法是给 PPO 起手解 (BC pretrain), 不是改环境**
- ❌ **写死板的单条件 ScriptedPolicy**（D-099 用户拍板）：用作 BC source 的脚本必须多层嵌套 + 每层多 sub-condition
- ❌ **用 PowerShell here-string + scp 推中文文件**：D-094 entry 前 session 用此方法推 决策记录.md, 全 GBK 乱码. 改用本地 cat heredoc + awk LF→CRLF + scp

---

## 训练铁律（防灾难性遗忘，锁死）

- ✅ **fresh random init 是默认** (D-071 PIVOT 后), 所有 D-XXX seed 不一样 / 不 --load 旧 ckpt
- ✅ **5M steps 是 shadow 上限** (D-082 7M -8.3%, D-076 5M GOLD)
- ✅ **single env GPU 是 quality 上限** (D-077~D-081 5 次实证)
- ✅ **观察 obs 568 维不动** (D-038 起锁死, D-094..D-100 全部守住)
- ✅ **save-every 100 ep** (避免一次崩盘全丢)
- ✅ **判据看 jsonl 的 niche 指标, 不看 score** (D-091 score 0.395 但 settlement 0)

---

## 当前状态（D-122，2026-06-03 ~22:45 UTC，本号收尾）

> ⚠️ **这一节是最新真相**，覆盖其下所有更早的 round_22 / D-113 块（那些是 PE/nutrition 历史，仍可参考但已被本块覆盖）。

**这一轮在干啥**：DP 单体 RUNG1（生火→烤肉/照明）那条血脉，目标让 agent 学会"打猎→烤肉→囤粮"。

**系统审计（用户要的"站在原始人视角看机制是不是闭环"）结论**：机制"能做"但"没理由做"——链是闭的、经济是开的。根因=基准线是张吊床：饿/渴每分掉12，但窝棚里每分回15→躺着摘果子永生、零风险；吃东西只看饱腹不看吃的啥（果子=鹿肉同分）；杀了当场就吃饱（生肉≈+40），囤熟肉只多+5→烤肉天生没需求。科技树每级都是"白加负担"。账本见 `docs/机制闭环审计_账本.md`。

**落地的两个根因修复（全部只加不删、cook_enabled 闸门内、v7 世界字节一致、可回滚 .bak_econ_20260603）**：
1. **寒夜稀缺**（`Environment.cpp` L762-785）：cook_enabled 时，过夜窝棚**不再回饥饿**（仍护渴+挡3x夜寒）→"躺着摘果子"不再是免费最优解→囤熟肉成过冬的命。
2. **凸的按指标溯源惩罚**（`RewardSystem` event_vital_danger，coef 0.02）：每个指标亏空**平方**计罚，单个垮掉的指标罚得最重→PPO 梯度+GAE 死亡回溯精确落到"放任那个指标下滑"的决策串，不再死后一刀切 -10。折进 `vital` bucket（schema 不变）。（用户原话："你得要精确到是哪一个节点犯错，得溯源"。）

**验证**：build_d122_v2 独立编译 BUILD_OK；`deep_protagonist --self-test` 我那两条 vital_danger 单测 PASS（另 4 条 river/animal/wardrobe/observation FAIL 是 pre-existing、与本改无关，且我的改 cook_enabled 闸门内 self-test 默认不触发）；`--cook-selftest` 9/9 OVERALL PASS。

**在跑的训练（本号收尾后挂着，跑完这轮就收手）**：
- **PID 32556 = 修复版血脉 `_e`**：从 48M 步 `r1_cook_s24.pt` 真续训（dims 同→载 .opt 带动量、网络没重置）、critic-warmup 64 预热价值头防塌。起步 score 0.255~0.264 未塌、ent 3.0~3.5、kl 0.02~0.035、craft+milestone 老本事在、vital 项已含新溯源惩罚。launch 脚本 `launch_r1_cook_e.ps1`。
- **PID 7984 = 旧经济对照 `_c2`**（build_d122 旧 exe）：保留当 A/B，对比"加了寒夜稀缺到底有没有逼出打猎/cook"。
- 之前的 hunt-nudge run（PID 19956）已杀（为腾 exe + 它是出不了 cook 的旧经济对照）。

**待观察**：`_e` 的 kills 何时>0 → raw_meat>0 → "cook OK" 首现；跟 `_c2` 的差异。判据看 jsonl，不看 score。

**PE（D/E/F + mark 真逻辑）**：代码全落地、自测全过、**未接入训练**（接入要重启进化+改观测不可逆，用户把火力给了 DP）；等推 PE 那天起新 run 自动生效。

---

## 当前状态（round_22，2026-05-31 ~13:30 UTC，PE 历史，已被上面 D-122 覆盖）

> ⚠️ 本块（round_15→22）是 PE/nutrition 历史；其下「截至 D-113」块是更早历史，仍可参考但被上面 D-122 块覆盖。

### 🚨 round_22 §4 阻塞发现（新执行者训练前必修，否则白训）
跑 §4 survivability gate（settler / noop / bc_v9-surgery 各 18000 步、nutrition ON）发现**营养机制在实时 env loop 里基本是惰性的**，三条必修：
1. **decay 太慢 → 营养永不成约束**：默认 `protein/vitamin_decay=20/min` → 满条 100→0 正好耗满整个 300s episode，`nutri_death_grace=8s` 根本来不及触发；且旧 food/thirst 死亡先发生。结果 `deaths_protein/deaths_vitamin` 全程 **0**（noop/bc_v9/settler 全 0），`protein==vitamin` 永远相等（纯衰减无回填）。**修法**：CLI 调快 decay（`--nutri-protein-decay 50 --nutri-vitamin-decay 50` 量级，让条在 ~100–150s 耗尽，episode 内留出 grace 死亡窗口）。
2. **vitamin 回填路径与暖启动策略错配**：`Environment.cpp` L553 `if(action.eat)` 才走觅食→`eat_vitamin`，但 **bc_v9 觅食用的是 `collect`（act3），从不按 eat（act0=0，collect=236）** → 暖启动策略**无法回填 vitamin**，必 vitamin 饿死。**修法**：把 `eat_vitamin` 也挂到 `action.collect` 的采集路径（L665+ 的 collect 块），或确认 collect 是否已喂食。
3. **打猎可行性未证**：settler oracle 本轮 `atk=0 / collect≈0`，没真打猎/觅食（疑 prey 不在身边 + 旧 hunger 被 regen 顶住没驱动）。**训练前必须先证世界可生存**：先让一个能处理旧 vital 的策略在调快 decay 下，靠 hunt 把 protein 养活、靠 forage 把 vitamin 养活，否则 PPO 会直接学"无视营养"。

### round_22 §3a：obs 567→569 + 首层 warmstart surgery（已完成+验证，含一处关键 bug fix）
- **obs**：`Observation` 末尾追加 `protein/100, vitamin/100`（index 567,568），`SIZE 567→569`，旧 0..566 字节稳定。nutrition OFF 时两槽恒 1.0/1.0。self-test：SIZE==569 + 槽位 + shelter-flag 全过（仅 2 个已知历史 FAIL：yaw_rate*dt、Wardrobe）。
- **surgery bug（重要教训）**：`torch::load(module,path)` 在形状不符时**不抛异常**，而是 `set_data` **直接采纳 checkpoint 形状**（567）→ 原先寄希望于 try/catch 的 surgery **永不触发**；且旧 `surgery_load_into` 用 `InputArchive::try_read` 按名读，读不到嵌套子模块权重（`copied=1 padded=0 missing=12`）= 等于没做。
- **正确做法（已落地，commit `ad81af6`+`e1cdd7d`）**：`torch::load` 已把 bc_v9 全部权重正确加载（含采纳成 [256,567] 的 encoder）→ 只需 `expand_encoder_if_needed()` 把唯一不匹配的 `encoder.weight [256,567]→[256,569]` 补零扩列（旧列照抄、新 2 列置零）+ 重建 Adam。`load()`/`load_reference()` 都走此路。验证：日志 `encoder.weight [256x567]->[256x569] cols 567..568 zero-init`、exe EXIT=0、**bc_v9 nutrition-OFF 行为与旧世界一致**（atk=0、照常 collect/建造、存活）→ 暖启动数学等价 bc_v9，符合设计。

### round_15→20：BC 夜宿证伪 + S4 PPO 历程（结论锁死，勿重做）
- **BC 五连 NO-GO（v10–v14）**：v13 软退、v14 全新 bank **崩盘**（1700000 pass 80→6、food-death≈2×）。减小扰动非单调改善 → "夜宿回家挤白天觅食"是 BC 全局克隆的**结构性专化/泛化权衡**。**bc_v9 永久锁为最终 BC 冠军，勿再 BC 修夜宿**。可复用资产：fire-free 采集链、`action17==0` 训前断言、9-seed 多轴门、`--bc-low-os` 夜宿降权通道。
- **S4 PPO 夜宿（round_17→18）**：v1 裸 PPO 暖启动→漂出 bc_v9 basin、attack 崩（8–303/seed）。**C(A+B)=mask attack+fire（结构性保证 atk=fire=0）+ KL 锚定 bc_v9（kl≤0.005）+ 严格夜宿 PBRS** = 首个不崩候选：train+new 中位 pass 100、残差 2100000 nShel 0.25→0.47。瑕疵 new-bank food-death +9。
- **Φ_day 白天觅食势函数（round_19→20）= 结构性跷跷板 NO-GO**：扫 day-coef{0.5,1.0,2.0}+night{2.0,1.5}，30ep CI95 复核发现 food 缺口**不消失、只在 seed 间搬家**（强夜势达标残差但退白天 food；给白天让度就掉残差）。根因=target-KL 把漂移夹死，多个 shaping 项抢同一份有限更新预算。旧世界最佳候选=**c1（night2.0，残差 2100000 nShel 达标 0.32 + atk=fire=0，代价轻度 food 真退）记为 known-gap**；bc_v9 仍不覆盖。

### round_20→21：营养系统机制（已落地，12 自测 PASS）
按 `decision_blueprint_nutrition`（用户蓝图）切主线：双营养条（**蛋白=打猎吃肉回 / 维生素=采集吃果回**），衰减→<30% debuff（`nutrition_speed_mult` 乘性降速）→归零 grace 后死（**独立死因 deaths_protein/deaths_vitamin**）。`--nutrition` 总开关（默认 OFF=旧世界，bc_v9 不受影响）。解禁 attack 打猎（`--ppo-mask-fire-only`，fire 仍 mask）。**红线**：营养条只回营养值、**绝不为吃肉/打猎直接发奖**（anti-Goodhart）。回填挂点：forage→`eat_vitamin`、meat/fish kill→`eat_protein`（见上 §4 第 2 条的路径错配问题）。

### round_21 §2 KL-anchor 教训（决策者给，训练 s4ppo_nutrition_v1 必用）
KL-to-reference 适合 in-basin 精修、**不适合学 reference 支撑集外的新技能**（Stiennon 2020 / Ouyang 2022）。`target_kl=0.02` 会把 PPO 锁死在 bc_v9 策略 basin → **学不会打猎**（打猎在 bc_v9 策略之外）。处方：**放松/退火 target_kl 到 ~0.05–0.1**（早期松、探索打猎；后期紧、稳定）。**勿用 0.02 训新世界冠军。**

### round_22 待办（handoff，已写 inbox/round_22）
1. 修 §4 三条（decay 标定 + vitamin 回填挂 collect + 证打猎可行/prey 密度）→ 重跑 survivability gate（forager+hunter 活、lazy 营养死、bc_v9-surgery vitamin 活但 protein 饿死）。
2. 训 `s4ppo_nutrition_v1`：bc_v9-surgery 暖启动 + KL-ref=bc_v9-surgery + **放松/退火 target_kl 0.05–0.1** + mask fire only（attack 开）+ **无竞争 potential 项**（Φ_day 跷跷板教训）。
3. 新世界多轴门 + CI95 + round_23。冠军独立于 bc_v9（bc_v9/c1 永久保留勿改）。

---

## 当前状态（截至 D-113，2026-05-31 ~16:30 UTC，EOD 收尾）

> ⚠️ 本块是 round_15 之前的历史，已被上方 round_22 块覆盖；保留作参考。下方旧的「D-102 DAgger 突破」路线图/历史表仍可参考，但**当前真实状态以最上方 round_22 块为准**。

**冠军（唯一在用）= `runs/bc_v9.pt`（+ bc_v9_backup.pt）**。Stage2 **双 bank 真泛化已确认**：
- 训练 bank（700k/800k/900k/1000k/1100k/1200k 6 seed）跨 seed 中位达标 **90%**，6/6 世界 ≥6/10、**无 0% 塌方世界**；
- 全新 bank1（1600/1700/1800k，从未训/从未采样）中位 **80%**；全新 bank2（1900/2000/2100k）90%（2100k=40% 是残差瓶颈）；
- **attack 全 seed 恒 0**（没有"修狼崩建造"跷跷板）；夜宿率 nShel~0.95；死因埋点：**死亡 93% 是饿/渴死、仅 7% 冻死**。
- 训练集 = 349 demo 文件：demos_d102(100) + demos_d102b(100) + dagger_d102\r4(60)+r5(60)+r6(14)+r7(15)。配方：从零重训、8 epoch、batch16、bptt64、lr3e-4、trigger 过采样。

**三连 NO-GO（都已回退，冠军始终是 bc_v9，勿重做）**：
- **bc_v10（r8 加固）NO-GO**：r8「混采夜宿地形+出生早狼、从零重训」→ 训练 bank 90%→65%、两全新合并 85%→60%，**容量稀释**（连无狼纯定居 seed 也退）。教训：**只做聚焦增量，绝不从零稀释重训**。`bc_v10.pt` 已删/不用。
- **bc_v11（火-营地）NO-GO**：把"夜里搭火留守"教给 BC → 与已赢的"夜里进屋"**在同一决策点互斥**，BC 无法像 oracle 脚本那样紧条件化 → 过度泛化成默认夜行为、**覆盖夜里进屋**：nShel 0.95→0.15、训练 bank 90%→10%、**冷死爆炸 0→20~52/seed**（火取暖 45/min 远不如房子，agent 赖在火边冻死）。`bc_v11.pt` 保留**仅作诊断**。
- **bc_v12（夜宿聚焦增量，EOD 本轮）NO-GO**：bc_v9 349 + 5 夜宿残差文件，loss 单调 1.07→0.41。多轴 eval 中位 **60%** vs bc_v9 90% → 退。**但残差杠杆是真的**：瓶颈 seed 2100000 **40→100**、2200102 50→60。**退步根因=数据污染**：5 个 night 文件采集时 oracle 火层是开的，夜困态吐了 **107 个 tend_fire(action17) 标签** → bc_v12 又学了点火-营地 → 冷死在已赢 seed 复现（700000 cold 0→25、1100000 0→36、fire-token 14~35）。`bc_v12.pt` 不采用、不覆盖 bc_v9。

**火（Stage3 Level-2）结论（重要）**：
- 火**世界机制已完整落地且无悔**：`FireSystem`（取火/搭火/续柴/半径取暖挂进 `VitalSystem::tick()`，动作 index17 不破坏旧 demo 血脉），**14/14 单元测试全过**，oracle 火层烟雾门全过。sim/机械层可直接复用到 S4 PPO。
- 但**火的"用法行为"走 BC 不可行**（bc_v11 实证 + oracle-only 退出判据）：oracle 标签正确、clone 怎么练都学不会"何时点火 vs 何时进房"的尖锐取舍 → 按蓝图 + 决策者裁定 **defer 给 S4 PPO**（PPO 适合学这种条件取舍）。
- **decision_round_13 = 采 Path A**：火机制保留，火用法交 S4；BC 主线回到夜宿进屋残差修复。

**当前 BC 主线 = 夜宿进屋（真瓶颈）**：93% 饿渴死的根因是"夜间无遮蔽 3× 饿渴惩罚"，而真问题是 **agent 建了房却夜里不进可达房**（1000000 型 nShel≈0.0、2200102 型 nShel≈0.5）。火治不了它（火-营地反而让 agent 不进房→冻死）。解法 = r7 式**聚焦增量夜宿 DAgger** 修残差尾巴。

**评测 bank 台账（下次绝不采样/绝不当 demo 源）**：
- 训练 bank：700k/800k/900k/1000k/1100k/1200k
- 全新 bank1：1600k/1700k/1800k｜全新 bank2：1900k/2000k/2100k
- 火 DAgger 已采（保留勿动）：2200110–2200139
- 夜宿 DAgger 已采（保留勿动）：2300100–2300159
- sanity：555000

**多轴防跷跷板门（decision_round_12/13 锁死，下次照用）**：① 残差 seed nShel↑+食死↓；② attack=0 且已赢轴不退（达標 house-only、camp/狼存活/建造）；③ 训练 bank 中位 ≥90%；④ 在不在采集集里的 bank 上验泛化；⑤ 看客观指标(nShel/食死/fire-token/houses/attack)不看達標%（達標 设计上不会跳）。任一轴退 → 回退 bc_v9 并归因。

**🔭 下一个执行者的第一步（已替你想好）**：
1. **纠正版夜宿 DAgger**：重采 night（全新 seed，**关掉 oracle 火层 或 在选择器加 fire==0 过滤**，避免本轮的火标签污染），选残差 seed（nShel<0.6 & builds & attack==0 & **fire==0**）→ 基 bc_v9 聚焦增量重训（≤5~8 文件，<r7 规模，绝不从零）→ 过多轴门则 GO，否则回退 bc_v9。2100000 40→100 证明这条杠杆能成，只要 demo 干净。
2. 火用法**不要再走 BC**，留给 S4 PPO（机制已在码上）。

**⚖️ 火 A/B 未决点（留给新决策者，用户换号前未最终拍）**：
- **A（决策者+执行者都倾向，今日默认按 A 收尾）**：火世界机制保留、火"用法"交 S4 PPO；BC 主线只做夜宿进屋聚焦增量。
- **B（仅当用户坚持"必须用 BC 把火练出来"才启）**：再用 BC 硬试一次火，但带硬止损线——oracle 严格"房可达必进房、仅真无房才搭火"+ 火权重砍到极低 + **房可达 seed 的 nShel 一旦低于 bc_v9 立即止损回退**。bc_v11 已证默认配方会崩，B 风险高。

**EOD 状态**：本轮 bc_v12 已干净跑完+评测+结论（NO-GO，自归档 round_14 见 inbox）。冠军 bc_v9 完好。已清理 runs 22.16→10.61GB（删 r2/r3/r8 旧轮 + fire_dagger + night 非 KEEP 散样 + scratch；保留 bc_v9/v11/v12.pt + 全部训练 demo + 5 个 night KEEP bin）。本地已 commit、未 push。**未开新轮**。

### 🗺️ 开发路线图（全自动执行手册 —— 用户睡觉/不在时照此自己决策，绝不空等）

> **怎么用这张图**：D-103 提速后单轮训练从 ~2.2h 降到分钟级。**默认行为 = 全自动连续跑**：一轮"训练+eval"跑完，立刻按本图判断在哪一阶段、GO 判据满没满足，满足就进下一阶段，没满足就照"出问题→自主决策"那列**立刻执行对应动作再开下一轮**，抓住用户电脑每一段空闲时间不停训。
> **不要因为"要不要继续/怎么选"而停下等用户**——所有岔路口的决策都已在下表替用户拍好，直接照做。每轮把做了啥+结果写进 `决策记录.md`（500字5件事），用户醒来一看就懂。
> **唯一会发消息给用户的情形 = 物理阻塞**：连不上用户机器 / chisel 隧道断 / 缺凭证 / 磁盘满 / 编译环境坏。即便如此也**边发消息边干能干的**（比如先 eval 已有 ckpt、整理数据、写文档），绝不空转。

**北极星（当前主任务，数周可达）**：让原始人在**多数 seed 上稳定"建房 + 整夜过夜"**（bldg≥5000 且 nShel≥0.8），达标率从 12/30 推过多数(>15/30)→逼近全部(~25-30/30)，同时把死亡率（现 15/30）压下来。这是通往蓝图终极愿景（见阶段4）的当前里程碑。

**路线（每个岔路口的决策已替用户拍好，照做不停）**：

| 阶段 | 干什么 | GO（满足→自动进下阶段） | 出问题 → 自主决策（立刻照做，不停、不问用户） |
|---|---|---|---|
| **阶段0 基建**（收尾中） | D-103 提速（已完成 ~7.2×）+ **warm-start 增量训练**（下一步第一件事：加 `--bc-init <ckpt>`，load 上轮权重只在新数据微调几 epoch + 少量旧数据复习防遗忘） | warm-start 单轮恒定分钟级，且与"从零重训"在 lr=0/小 smoke 上等价（不引入行为差异） | ①warm-start 退化（旧能力掉）→ 自动把旧数据复习比例从少量提到 新:旧≈1:1 重训；②仍退化 → 自动回退"从零全量重训"（慢但安全，提速后也只十几分钟），继续推进，**不停** |
| **阶段1 DAgger 推达标过多数**（⭐当前所在） | 循环：eval bc_vN → 找失败 seed + 失败模式 → 在失败状态用 oracle 生成纠错 demo → 聚合 → warm-start 训 bc_v(N+1) → 再 eval。r4→bc_v6 目标>15/30，r5→bc_v7 逼近 20+/30 | 达标率**单调上升**（或至少不降）+ 夜间在屋率均值上升 | 某轮不升/下降，**别盲目加数据**，按序自动试：①eval 分类失败（白天 bldg 不够 / 夜里 nShel 不够 / 死亡）；②对应改 oracle 决策树相应层（建造层 / 夜间作息层 / 生存复位层），重生成 demo 重训；③仍不升 → 自动加大失败 seed 在 demo 里的占比、并延长录制集数。**全程不停，每轮记账到决策记录.md** |
| **阶段2 稳健 + 降死亡** | 触发（自动进）：达标>15/30。①多 seed bank 验证泛化（不只 555000，自动再跑 700000 及新 seed）②压死亡（白天觅食/保暖/血量复位的 oracle 示范） | ≥2 个**没训过**的 seed bank 达标率与训练 seed 一致(±少量) + 死亡率显著下降 | 换 seed 就崩 = 过拟合 → 自动增加 demo 的 seed 多样性，回阶段1 补数据重训；死亡偏高 → 自动加生存复位 demo。**照做不停** |
| **阶段3 行为质量打磨** | 触发（自动进）：达标稳健 + 死亡低。对照「蓝图覆盖度表」补齐 BC 还没可靠克隆的行为（造衣 line23 / 狩猎 / 中期建造等 oracle 100% 但 BC 没跟上的），让动作连贯拟真（无"原地冻住"） | 蓝图主线条目 BC 覆盖达标 + eval 录像行为自然 | 某行为 BC 学不会 → 自动查 oracle 该行为示范数量/质量并补 demo 重训。此阶段达标后 → **自动进阶段4**（并给用户发一条"定居任务收官"的成果汇报，但不停、直接继续） |
| **阶段4 蓝图终极愿景**（自然延续，不需用户拍板） | 按 `docs/最终蓝图.md`：**同一只网络从 S4 训到 S6、参数永不重置**，渐进扩到 50-60M，把完整生存进程练到"行为和真实世界一模一样"——早期(采草造衣/觅食/狩猎/昼夜躲狼)→中期(真实建造4房型)→后期(农耕/饲养/驯服)→S6 用户实时干预(扔狼/起火/降雨实时反应)。用 EWC/旧奖励权重保留防遗忘 | 每个 progression 阶段行为稳定且不退化旧阶段（训练曲线分项记录，任一退化立刻自动警报+回退 ckpt） | 这是月级长跑：每次只推进**一个最小子目标**（如先把"造衣"练稳再上"建造"），遇架构/参数选择按蓝图选最小可行方案、记账继续；显存不够就缩 batch 或分阶段。**持续推进，不停** |

**一句话当前位置**：阶段0 收尾（提速已完，warm-start 待做）→ 紧接阶段1（DAgger r4→bc_v6）。**下一轮开跑前先把 warm-start 接上并验证等价，再进 r4，然后照表一路自动跑下去。**

**全自动跑这条线时，红线复述（违反=失职，跑多快都不能碰；这些是"怎么做"的约束，不是"要不要停"的理由）**：
1. **走 DAgger + oracle，绝不用 PPO 自训练救 camp**（D-101 双墙：PPO finetune 会主动侵蚀过夜）。
2. **不为指标给直接奖励**（reward-hacking 已实证全崩，D-064）；reward/反馈/观测(568维锁死)/数据/超参一律不动（阶段4 扩网络是蓝图明许的结构演进，按蓝图走）。
3. **提速只走数学等价改写**（D-103a/b 已证），任何会改行为/质量的提速 = 红线（D-077~D-081 锁死）。
4. **每训完一批必 commit**（本地，不 push）；**每轮决策结束刷 4 件文档**（本 note + PROGRESS.md + 决策记录.md 500字5件事 + 速查）——这是"替用户记账"，让他醒来全懂。
5. **save-every 防一次崩盘全丢**；结构/device/batch 改动先小 smoke 验证再放大；任一行为退化立刻自动警报+回退 ckpt。

### D-085~D-100 final 数据

| D-XXX | 配置 | 结果 | 关键学到 |
|---|---|---|---|
| D-085 | HIDDEN 256→1024 容量扩展 | score **0.226** 仍卡 | 容量不是 root cause |
| D-086 | 起手包 1W + 1G (chain shortening) | score 改善但 0 屋 | starter pack 不够 |
| D-087 | ScriptedPolicy 路线初探 | smoke OK | 单条件死板, 须 D-100 重写 |
| D-088 | auto-Shed at spawn | 31 屋 | env 给 PPO 免费工地 |
| **D-089 canonical** | + Shed 完工立即重生 + lean-to off | 122 屋 | 但 PPO 不住 (D-094 才发现 D-089 自己埋的 conveyor belt bug) |
| D-091 | + cold 18/min + 农田 2x + Shed 5m coverage | 122 屋, bldg 4-139, n_shel 0.001-0.014 | PPO 学到"死掉重生"不学定居 |
| D-092 | + 夜禁野果 | 189 屋, bldg 6-361, n_shel 0.001-0.066 | 断粮还不够 |
| D-093 | + cold 50/min (1 分钟必死) | 233 屋, bldg 7-336, n_shel 0.004-0.119 | PPO 宁愿死 |
| D-094 | 删 D-089/D-090 自加的 Shed conveyor belt + cold 18/min 回退 | 5M 失败, bldg 100→5 | conveyor belt 删了仍崩, attractor 才是根因 |
| D-095 | Shed coverage 5→15m + 夜里 vital 3x drain | 5M smoke 91% → 后期崩 | "院子也算家"机制 OK 但 PPO 不学 |
| D-096 | obs[130..132] = 第一栋完工建筑 dx/dy (家 compass) | 5M smoke 99% → 后期崩 | 有家 compass ≠ 愿意回家 |
| D-097 | + 在家 hunger/thirst +3/min regen | 5M ep 1123→12 | 回血太小 (+0.6 r_food/min < 觅食 +5/min) |
| D-098 | gamma 0.99→0.995 + regen +3→+15/min | 5M ep 502→0, score 0.199→0.223 | 长视野 + 强回血仍输给觅食 attractor |
| **D-099** | **+ 在家 r_alive 翻倍 (0.05→0.10/s)** | 5M last 100: bldg 14.8, n_shel 0.007, houses 147/ep | **PPO 8 连击全失败. RL local optimum trap, 改环境绕不过** |
| **D-100 ScriptedSettler (oracle, 非 PPO)** | **手写 7 层嵌套反应式决策树** (main_train.cpp +460 LOC) | **50 ep: bldg 8505/9000 (94.5%), n_shel 0.996, houses 1/ep, 3 deaths/50 ep** | **oracle 路径完全可行. 同样 env, 同样 obs, 手写 7 层一击命中. 不是 env 不允许, 是 PPO 自学找不到.** |

**结论**:
- PPO 在 single-agent 单步觅食 attractor 下不会自学定居 (8 次跨越 cold/food/shelter/obs/regen/gamma/r_alive 改动全失败)
- 蓝图 line 30 "建造定居" 必须靠 imitation learning + RL finetune (D-101 BC pipeline)
- ScriptedSettler 是 BC demo source, 7 层嵌套保证 demos 多样化 (按 day/night/距离/vitals/inv/threat 分支)

### Self-test：**130+ 全过**（最近 D-100 build OK, 6 warnings 0 errors）

### 蓝图覆盖度（D-100 后实证, oracle/mean 数据）

| 蓝图 line | 状态 | 最强 ckpt |
|---|---|---|
| line 21 觅食 | ✅✅ 100% (D-076), 65% mean | D-076 PPO |
| line 21 找水 | ✅ 93% (D-076), 31% mean | D-076 PPO |
| line 21 累了找庇护 | ✅✅ **100% (D-100 oracle), 0% PPO** | D-100 ScriptedSettler |
| line 22 避狼 | ✅ death 低 | D-076 PPO |
| line 22 狩猎（craft_spear） | ✅✅ 100% / 98% mean | D-076 PPO |
| line 22 狩猎（first_hunt） | 🟧 73% (D-076 only), 25% mean | D-076 PPO |
| line 23 造衣 | 🟧 5% PPO, **100% D-100 oracle Layer 6** | D-100 oracle |
| line 24 视野记忆 | ✅ | D-049 接通 |
| line 26 洞穴庇护 | ✅ | D-049 接通 |
| line 62 累了走慢 | ✅ | D-049 接通 |
| line 30 中期建造 | ✅✅ **100% (D-100 oracle), 0 屋 PPO** | D-100 ScriptedSettler |
| line 34+ 后期农耕/饲养/驯服 | ❌ 未触发 (D-102 ScriptedFarmer 才接) | — |

### D-101 BC pipeline (已完成, commit 1f37eed)

实现了 demo writer + BCTrainer + CLI（`--record-demos` / `--policy bc` / `--bc-trigger-oversample`）+ PPO finetune（`--ent-coef` / `--critic-warmup N`）。

**双墙结论（核心，必懂）**——把"建房"和"camp（整夜待屋里）"拆开看，BC 可学性天差地别：
- **建房**（短程：走到工地 deposit）：BC 可靠克隆 + PPO 保留。5M finetune ckpt 新 seed **30/30 建房**（纯 PPO 8 连击从未达到）。✓
- **camp**（长程：连续 3600+ 步维持）：BC 只能脆弱克隆，**PPO finetune 还主动侵蚀它**（nShel 暖启动 0.31 → 末段 0.016）。根因 = gamma=0.995 的 ~200 tick 视界 credit-assign 不了整夜回报（熵、critic 嫌疑已排除：加 `--ent-coef 0.001` 防熵爆、`--critic-warmup 64` 冻 BC 策略先训 critic，仍侵蚀）。

**踩坑**：trigger-oversample 早期把"走去工地"那段移动也放大 30×，把连续动作头训成"爱乱走" → 改成只放大 deposit 事件小窗口(前8后12步)、不放大走路。

### D-102 DAgger + 精致 oracle (进行中, oracle/dagger 代码 commit 1c4e404)

**用户拍板**：否决"古法自训练"（PPO 自己探索会毁掉刚学会的过夜）。改走两条不碰红线的杠杆：①精致机器人决策树 ②DAgger（机器人在原始人走偏的状态上当场纠错）。

**① 精致 oracle 夜间作息**：旧 oracle 到家后夜里仍掉进 Layer 7 在 12m 内觅食、且在落点"原地冻住"从不演示复位。新增 Layer 5 夜间逻辑：天黑后主动走回 3m 家核心(HOME_CORE_M)再原地睡。更拟真 + 给 BC 补上"离家有偏→往家走"的复位力示范。重验 oracle 更稳：bldg 8965/100%、nShel 0.9978、30 局 0 死亡。

**② DAgger 是 camp 解药（实证）**：光把 oracle 改拟真（bc_v2）不够——oracle 太完美几乎不走偏，示范里没有"走偏纠错"样本，bc_v2 ≈ bc_k（建房 30/30，camp 0）。DAgger 让原始人自己走、oracle 在它走偏处标注正确动作，专收纠错样本。`--dagger` 模式：env 用 clone 动作前进，但记录 oracle 在该 obs 的动作当标签。

**各轮结果（新 seed 555000，30 局，stochastic eval；"达标"= bldg≥5000 且 nShel≥0.8）**：

| 版本 | 数据 | 建房 | bldg≥5000 | nShel 均值 | 达标 | 死亡 |
|---|---|---|---|---|---|---|
| bc_k/bc_v2 (无 DAgger) | 示范 | 30/30 | 0 | 0.03 | 0 | 23 |
| bc_v3 | +DAgger r1(60) | 30/30 | 6 | 0.20 | 6 | 17 |
| bc_v4 | +r2(120) | 30/30 | 9 | 0.31 | 5~6 | 15 |
| **bc_v5** | +r3(180) | 30/30 | **14** | **0.41** | **12** | 15 |

**结论**：DAgger 单调有效，达标率 0→6→12/30，逼近多数。瓶颈从"完全不过夜"转为"夜间精度"（部分局待家但夜里轻微飘出庇护圈）。

**踩坑/教训**：
- DAgger 是标准"聚合数据集→重训"，但我每轮**从零重训全部累积数据** → 单轮耗时 ∝ 累积数据量（bc_v3 ~1.4h → bc_v5 ~2.2h），不可持续。**下一步必做 warm-start 增量**（加 `--bc-init <ckpt>`，load 上轮权重只在新数据上微调几轮 + 少量旧数据复习防遗忘 → 单轮恒定 ~十几分钟）。
- bc_v5 训练 epoch2 loss 尖峰(0.81→0.92)是 Adam 噪声，epoch3 即回落，非发散。
- "扩容到 50MB 能不能理解世界"——D-085 已实证：6.9M 参数(10×)反而 0 houses + 更差。**容量不是当前瓶颈，训练信号才是**。顺序：先用 2MB 小网把行为教稳（DAgger），再按蓝图扩 50MB 提升一致性/泛化。别把顺序搞反。

**关键路径/产物**（远端 D:\...\deep_protagonist）：
- oracle + DAgger 代码：`src/main_train.cpp`（Layer 5 夜间作息；`--dagger` 标签逻辑）
- 精致 oracle demos：`runs/demos_d102/`(100)、`runs/demos_d102b/`(备用 100, seed 200100..299100)
- DAgger 纠错数据：`runs/dagger_d102/r2 r3 r4/`(各 60；r4 已采好待用)
- 模型：`runs/bc_v2.pt … bc_v5.pt`(+ *_backup.pt)；eval jsonl：`runs/dagger_d102/bc_v*_sto_*.jsonl`
- 状态文档：`docs/D102_STATUS.md`

### D-103 本地训练吞吐优化 (用户 2026-05-29 提出, 纯吞吐提速)

**背景**：用户实测本地训练 CPU 只 40~70%、GPU 只 40~60%、显存很低，让我"把资源吃满、把训练速度拉起来，但**绝不碰 reward/反馈/观测/数据/超参**，只动并行度/吞吐/喂数据方式"。

**定位瓶颈（有实测）**：
- 采集**不慢**：机器人(settler)录一局 9000 步仅 **0.66s**；DAgger(clone+oracle) 一局 ~9.9s。采集不是瓶颈。
- 真瓶颈是 **BC 训练 GPU 饥饿**：把一局 9000 步**逐时间步**喂网络(batch 才 16)，每个 BPTT 窗口还做一次 GPU→CPU `.item()` 同步 → GPU 大半在等。实测训练时 GPU 35~67%、显存仅 2.9G/8G。

**改了什么（纯吞吐，数学等价，`include/policy/BCTrainer.hpp`）**：
1. 窗口内**批量化非循环部分**：encoder + cont_mean/disc_logits 头对整个 BPTT 窗口一次大矩阵乘算完（旧版每时间步算一次小的）；GRUCell 循环天生串行，保留逐步。
2. 整组数据**一次性搬上显存**（旧版每窗口 host→device 来回搬）。
3. 去掉每窗口的 `.item()` GPU→CPU 同步，epoch 末只同步一次。
4. 加 `setBenchmarkCuDNN(true)`（仅 autotune，不改结果）。
- **未动**：reward、loss 公式、class 权重、batch_eps=16、bptt=64、lr、数据格式、seed、trigger_oversample——全原样。value/log_std 头照旧不参与 BC。

**实测（smoke：demos_d102 前 6 文件 × 2 epoch，RTX 5060）**：

| | 旧版 | 新版 |
|---|---|---|
| 总耗时 | 19.95s | **7.77s** (≈2.6×) |
| 纯训练 | ~19s | ~6.7s (≈2.8×) |
| epoch1 loss | cont 0.0560 / disc 0.1321 | cont 0.0559 / disc 0.1321 |
| epoch2 loss | cont 0.0552 / disc 0.0164 | cont 0.0555 / disc 0.0165 |

→ **loss 与基线一致（差异仅小数第 4 位 = 浮点重排噪声）= 行为没改，只是更快 ~2.6×**。opt.pt 与 bc_v5.pt 同尺寸结构。

**更大规模 A/B 对照（40 文件 × 3 epoch，346044 帧，groups=3 — 这才覆盖到"跨组累加 loss"路径；旧版 exe 由 git checkout D-103 前源码重编，新旧二进制并存跑同数据同 seed=12345）**：

| | 旧版 exe | 新版 exe |
|---|---|---|
| 墙钟 | 95.67s | **33.49s** (**2.86×**) |
| epoch1 | cont 0.2166 / disc 0.3023 / tot 0.5189 | cont 0.2086 / disc 0.2995 / tot 0.5081 |
| epoch2 | cont 0.1826 / disc 0.1819 / tot 0.3645 | cont 0.1624 / disc 0.1843 / tot 0.3468 |
| epoch3 | cont 0.1506 / disc 0.1814 / tot 0.3321 | cont 0.1418 / disc 0.1841 / tot 0.3259 |

大规模训练时新旧 loss 轨迹有 ~2~6% 漂移（新版还略低、非更差），**不是 bug 而是浮点重排经 Adam 多步放大的混沌**——为坐实这点做了 **lr=0 对照**（冻结权重、无混沌累积）：

| lr=0, 40 文件, groups=3 | cont_mse | disc_ce | total |
|---|---|---|---|
| 旧版 exe | 0.2307 | 3.0373 | 3.2679 |
| 新版 exe | **0.2307** | **3.0373** | **3.2679** |

**lr=0 下新旧逐位一致（4 位小数全等）= 前向 + loss 计算在多组多窗路径上数学完全等价**，铁证。lr=0 前向也快 ~2.6×（旧 ~34.7s / 新 ~13.4s）。结论：**改动完美无缺**——同样的计算、同样的梯度，纯吞吐提升 2.6~2.9×。

#### D-103b 续：融合 GRU 再提速（用户"继续吃显卡"授权后做，commit a092c16）

D-103a 之后的剩余瓶颈 = **GRU 逐时间步递归**：一个 group 一个 epoch 要发 ~9000 次很小的 GRUCell kernel（每次才 B=16 行），launch-bound，GPU 空等。**解法**：把 BCTrainer 里手写的 `for t: h=net->gru(enc[t],h)` 循环换成 libtorch 的**融合多步算子 `torch::gru`**，一次性跑完整个窗口的递归——**复用同一份 GRUCell 权重**（PyTorch GRU 与 GRUCell 公式、(r,z,n) 门顺序完全一致）。

- **只改 BCTrainer 训练循环**；**模型/checkpoint 不变**（存盘仍是 GRUCell 版 ActorCritic，b_new.pt 2190394 字节 == bc_v5.pt，eval/PPO/推理完全不受影响）；reward/数据/超参一个没动。
- **A/B（40 文件×3ep，base=D-103a vs new=D-103b，同 seed）**：墙钟 **35.76s → 13.21s（再 2.71×；累计 vs 最原始 95.67s ≈ 7.2×）**。
- **lr=0 前向仍与 D-103a 逐位全等**（cont 0.2307 / disc 3.0373 / total 3.2679）→ 前向数学等价铁证；lr=0 前向耗时 ~13.3s→~4.8s（≈2.75×）。
- 训练 loss 轨迹漂移（epoch3 total base 0.3259 vs new 0.3353，新版非更差）：与 D-103a 同性质——**前向逐位等价、但反向梯度的浮点求和顺序变了**（融合 GRU 的 backward 归约顺序与展开循环不同），经 Adam 多步放大成混沌；数学仍是同一函数同一解析梯度，两条轨迹都健康收敛。
- **GPU 利用率 ~40% → ~55~62%**（融合后递归只剩少量大 kernel，空等变少），显存仍 ~3.4G。

**坑/注意**：
- 把"逐步小算子"合成"整窗大算子"：梯度数学等价，但浮点求和顺序变了，loss/权重有 1e-4 级差异属正常、非 bug。
- 整组一次上显存：full-ep 组 maxT≈9000、B=16、O=567 → ~0.33GB，8G 够；以后若大增 batch_eps 要留意显存。
- GPU 利用率：大测试时 nvidia-smi 瞬时仍 ~38~40%、显存 ~3.4G。**原因**：按"不改底层"要求 batch_eps 保持 16，每个 kernel 本就小、kernel-launch 主导，所以瞬时 util 提不太高——但**同样的活耗时只剩 1/2.8**，吞吐才是真凭证（"把速度拉起来"已达成）。要把瞬时 util 也吃满得加大 batch_eps，但那会改变梯度批次=行为变化，本轮明令禁止，故未动。warm-start 增量（D-102 待办）仍要做，与本次提速正交、可叠加。
- **与红线"D-077~D-081 提速全 -36% 已锁死"的区别**：那批是会改变行为/质量的提速（结果变差才被锁）；D-103 是**数学等价**改写（同梯度同结果），已用 smoke loss 对齐验证（差异仅 1e-4 浮点噪声），且用户本轮明确重新授权"把训练速度拉起来、别碰 reward/数据/超参"。遵守了红线要求的"结构/device/batch/parallel 改动先小 smoke 验证"。

**产物**：`include/policy/BCTrainer.hpp`（D-103a+D-103b 改动，本地 commit ea9ae95/5421683/a092c16/38cd54a/2063263，未 push）；日志 `runs/spdtest/`：小 smoke `bc_base.log`/`bc_opt.log`、大 A/B `big_old.log`/`big_new.log`、lr=0 对照 `lr0_old.log`/`lr0_new.log`、D-103b A/B `b_base.log`/`b_new.log`、D-103b lr=0 `lr0_base.log`/`lr0_b.log`。

---

## Tooling 教训（背景训练启动方式，每个 D-XXX 都用）

- ❌ `Start-Process -WindowStyle Hidden` → SSH session 断开会被 job-object 杀掉
- ✅ **`Register-ScheduledTask` + .bat 包装 + `(Get-Date).AddSeconds(5)` 触发** → 完全 detach，SSH 断了也跑
- 模板：见末尾 "Background 训练启动" 段
- D-094..D-100 build: `build_d100.bat` (cd /D + vcvars64 + ninja deep_protagonist_train), build 前先 `Get-Process -Id <训练 PID>` 检查 .exe 没被锁
- 中文文件推送: ❌ PowerShell here-string + scp (GBK 乱码), ✅ 本地 cat heredoc + awk LF→CRLF + scp

---

## 决策权

- 技术细节（算法/网络/超参/库）→ **AI 自己拍**
- 方向 / 蓝图改向 / 阶段切换 → **用户拍**
- autonomous 模式（D-062 起）：上面两条同样适用，但常规迭代不阻塞用户
- D-100 起 BC 路线: 网络改动属架构改动, 但用户 D-099 明说"你允许你改架构改算法的", 直接 AI 拍执行

---

## 时间换算（估时必须遵守）

- AI 写 30 分钟代码 ≈ 程序员 1 个月工作量
- 估"多久"分开报：**开发时长（AI 算）** + **训练时长（GPU 算）**
- 2M PPO step ≈ 60 分钟 @ RTX 5060 Laptop（实测 ~548-660 steps/s，D-063 实测 3648.95s wall 548 steps/s）
- 5M PPO step ≈ 96-107 分钟（D-076/D-083/D-084 三次实测 96/104/107 min）
- 5M CPU quiet (D-091..D-099) ≈ 80-100 min (PPO 不动 GPU 时更快)
- ScriptedSettler 50 ep 测 ≈ 10 秒 (60k steps/s, 不用 NN)
- BC pretrain 100k steps (D-101 估) ≈ 10-30 min (CPU OK)

---

## 文档写作规则

- 文档 = 写给未来 AI 看的备忘
- **精简，只写核心，不写解释、示范、铺垫**
- 例外：4 份速查文档要大白话（用户会翻）

## 4 份速查文档（每轮决策后刷）

| 文件 | 内容 |
|---|---|
| `docs/术语速查.md` | 术语 → 大白话 |
| `docs/参数速查.md` | 参数 → 作用/调大调小后果 |
| `docs/决策记录.md` | 拍板 → 选了啥/为啥/影响（autonomous 模式也写） |
| `docs/Q&A速查.md` | 用户问过的 |

## 每轮决策结束自检清单（每轮，不是 session 末！）

1. ☐ docs/PROGRESS.md 末尾追加 D-XXX 行
2. ☐ docs/决策记录.md 写 500 字 5 件事
3. ☐ 术语/参数/Q&A 速查（如有新词）
4. ☐ Knowledge note (本) — `suggest_knowledge` action=update, value 内 inline 完整 body
5. ☐ git commit (本地)

---

## 最终蓝图（北极星 v2，D-034 升级）

完整生存模拟，单 agent + 浅奖励 (alive/death/eat/drink/collect/kill/build/craft/wear/farm/tame/explore) + 主线驱动 milestones：

**S1 生存基线** (D-049/D-052/D-053 闭环): 喝水 / 吃浆果 / 躲狼 / 体温
**S2 探索 + 收集** (D-049 闭环): 木头 / 草 / 石头 / 浆果丛 / 水源
**S3 建造定居** (D-100 oracle 命中, PPO 端 8 连击失败, D-101 BC 接): Shed → Wood → Stone → Big 升级链, 真住进去
**S4 农耕** (env 已支持, D-102 ScriptedFarmer 教): plant_seed / water / harvest 围着家种
**S5 制造** (env 已支持 D-062, D-103 ScriptedCrafter 教): craft_spear / craft_grass_dress / craft_fur_cloak / wear_clothes
**S6 打猎** (env 已支持, D-076 73% / D-103 加强): spear hunt 兔/野猪/熊
**S7 域外探索** (D-104+ ScriptedMaster): 雪山 (寒冷峰), 沙漠 (热区), 沼泽, 山洞 (避难), 河流 (饮用)
**S8 多 site 村落** (D-104+): 在 ScriptedMaster 后期扩展
**S9 动物互动** (D-104+ ScriptedMaster): tame (驯服), 跟随
**S10 极端事件** (env 已支持, D-104+ 整合): 暴雨/暴雪/夜狼群

---

## 决策日志速查（D-001 → D-100）

> 全文 `docs/决策记录.md`（~155 KB）。这里只列重大节点。

**S0-S3.10 历史阶段（D-001 ~ D-034，2026-05-24）**：
- D-001~D-009 路线 / 语言 / 算法 / 蓝图 / AGENTS.md 契约
- D-010~D-019 S0/S1 项目骨架 + libtorch + CUDA + 3D 世界
- D-022~D-027 S2/S3 1024m 大世界 + 动物植被 + 生命系统
- D-029~D-033 推翻视觉整改，S3.7-3.9 训练前置机制
- **D-034 二次蓝图升级**：允许农耕/衣服/饲养/驯服

**S3.10 完整生存模拟主线（D-034）**：
- 进度 / 真实建造 / 衣服 / 农耕 / 驯服 五子阶段全完成
- Observation 124 → 145 维

**S4-pre PPO 训练前置（D-035 ~ D-037）**：
- D-035 S4-pre：AgentAction 19 channels + headless + PPO 骨架 + wander baseline
- D-036 reward hacking 修复（eat/drink return actual_delta）
- D-037 Memory 选型：**GRU + SpatialMemory**（不用 DNC）

**S4 早期训练 + 撩狼塌缩调试（D-038 ~ D-048）**：
- D-038 可视化全部延后
- D-039 离散头 Bernoulli → Categorical
- D-041 主线驱动 reward 重塑（milestone）
- D-043 reward 哲学重设计：Crafter + PBRS + Homeostatic（论文）
- D-047 杀掉 wolf_killed reward（撩狼根因第 2 层）
- D-048 蓝图回归综合实施（5 项一次性改动）

**S4 蓝图早期 closed-loop 实现（D-049 ~ D-054，2026-05-27 凌晨）**：
- D-049 结构修复 3 处断裂：洞穴庇护 / 累慢 / 视野记忆
- D-050 spawn 改到水视野内（6 次 P1-P6 迭代）
- D-052 first_eat/drink critical bonus +5（food 95% death 7%）
- D-053 thirst_decay 8 → 12 让水真有压力（drink 学到 29% 前所未有）

**S4 中期建造尝试 + drink 100% 闭环（D-055 ~ D-061，2026-05-27 早→下午）**：
- D-055 D-053 ckpt eval：981 图纸 / 0 房子 → 单 site 限制 + deposit reward
- D-056~D-059 在 D-055 ckpt 上各种 reward/world 改动 → 全部 PPO collapse abort
- **D-060 fresh PPO restart + Shed 1+1**（绕开 D-055 ckpt 陷阱）→ food 100% / drink 85%
- **D-061 在 D-060 ckpt 上再 +2M** → **drink 100% sustained / R +6.4 POSITIVE — 项目首次 line 21 闭环**

**S4 line 22 狩猎推进（D-062 ~ D-064，2026-05-27 晚 → 2026-05-28 凌晨，autonomous）**：
- D-062：加 first_hunt critical（rabbit/deer/fish only, wolf 排除）+ craft_spear/wear_clothes 升 critical + Fur 进库存计 collect → score 0.182 不退步
- D-063：起手包 1 Wood + 1 Stone @ spawn → spear 100% / hunt 8.3% / score 0.292（旧史最高）
- D-064：per-hit shaped reward → reward-hacking 实证 col 68%→37%, 用户原则验证（不许给指标直接奖励）

**S4 ckpt collapse 5 连败 + fresh init pivot（D-065 ~ D-076, 2026-05-28 早→中午）**：
- D-065：revert D-064 保留 Grass starter → score 0.296 旧金标准
- D-066: prey 密度 ×2.5 (env 改) → score 0.134 全崩 (env dose-response failure)
- D-067~D-070: 在 D-065 ckpt 上各种单变量改动 (ent_coef/Adam/lr) → 全部 0.13 collapse, ckpt continue 死路实证
- D-071: **PIVOT** fresh random init 2M → score 0.158 (env 是 learnable)
- D-072: fresh init 2M + D-065 hyperparam → score 0.193
- D-074: fresh init 3M → score 0.193 (drink 23% 首次)
- D-075: --load D-074 ckpt + 2M → 0.103 collapse (continued-training 6 连败实证)
- **D-076: fresh init 5M** → **score 0.312 GOLD (hunt 73% line-22 首次突破), 永久 baseline**

**速度优化全部失败（D-077~D-081, 2026-05-28 中午）**：
- D-077: epochs/mb 调优 → GPU util 几乎无效 (env step 不是 bottleneck)
- D-078: VecEnv N=8 多种子 → 4.2x 速度 / -36% quality (多世界稀释)
- D-079: VecEnv N=8 单种子 → -44% quality (单种子 trajectory 过相关)
- D-080: VecEnv N=2 (用户 pivot kill)
- D-081: CPU mode → 3.5x 速度 / -36% quality (CPU/GPU 数值精度 + RNG 差异)
- 结论: 接受 D-076 96 min 单 env GPU 为永久 baseline, 5 次失败入红线

**Step depth + cross-seed variance (D-082 ~ D-084, 2026-05-28 14:53 → 21:00)**：
- D-082: fresh init 7M seed 12345 default → score 0.286 (-8.3%), 加 step 反而退步, final ent ~11 (diverge), **5M 是 shadow 上限**
- D-083: fresh init 5M seed 12346 → score 0.247, 卡 stage 1 (0 houses, drink 0%, hunt 0%)
- D-084: fresh init 5M seed 12347 → score 0.226, 建 5 屋 max collect 951 但 drink 0% hunt 2%, 不同 niche
- **三 seed variance**: mean=0.262 std=0.037 CoV=14% range=0.086, D-076 是 1.4σ 上界
- 结论: **D-076 0.312 不是 baseline mean, shadow 545K params 是 root cause**, mainline 必须扩容 S5/S6

**S3 settlement 攻关 8 连击 + ScriptedSettler oracle 突破 (D-085~D-100, 2026-05-28 → 2026-05-29)**:
- D-085: HIDDEN 256→1024 容量扩展 → 仍 score 0.226, 容量不是 root cause
- D-086: starter pack 1W+1G → 改善但 0 屋
- D-087: ScriptedPolicy 路线初探 → 单条件死板, D-100 重写
- D-088: auto-Shed at spawn → 31 屋 PPO 不住
- D-089 canonical: + Shed 完工立即重生 (D-094 才发现是 conveyor belt bug) + lean-to off → 122 屋 0 住
- D-091: cold 18/min + 农田 2x + Shed 5m → 122 屋, bldg 4-139, n_shel 0.001-0.014
- D-092: + 夜禁野果 → 189 屋, n_shel max 0.066
- D-093: cold 50/min → 233 屋, n_shel max 0.119, PPO 宁愿死也不躲
- D-094: 删 D-089/D-090 Shed conveyor belt + cold 18/min 回退 → 5M 失败, attractor 是真凶
- D-095: Shed coverage 5→15m + 夜里 vital 3x drain → smoke 91% / 后期崩
- D-096: obs[130..132] = 家 compass → smoke 99% / 后期崩 (家位置 ≠ 愿意回家)
- D-097: 在家 hunger/thirst +3/min regen → 太小输给觅食 attractor
- D-098: gamma 0.99→0.995 + regen +15/min → 长视野 + 强回血仍输
- **D-099: 在家 r_alive 翻倍 (0.05→0.10/s)** → last 100 ep bldg 14.8, n_shel 0.007, houses 147/ep, **PPO 8 连击全失败实证**
- **D-100 ScriptedSettler (oracle, 非 PPO, main_train.cpp +460 LOC, commit 8f2ca4a)**: 7 层嵌套反应式决策树, pure obs→action, 50 ep 测: bldg 8505/9000 (94.5%) / n_shel 0.996 / houses 1/ep / 3 deaths. **蓝图 line 30 oracle 路径实证可行**. 用户拍板 BC + PPO finetune 路线 (D-101)

### profile 数据（永久参考, 不要再 profile）

- ppo->sample_action GPU 921us (97% 占比, kernel launch overhead 不是 compute) 
- CPU 180us 
- env.step 30us (3%)
- 5 子系统 tick 11us (<1%)
- 结论: 2MB shadow 网络 GPU 主要是 launch overhead 不是 compute, 优化无意义

---

## 技术栈

- C++ 17/20，MSVC 2022 Build Tools
- CMake 3.25+，Ninja
- CUDA 12.8（driver 13.x 兼容）
- libtorch 2.11.0 + cu128
- raylib 渲染
- PPO + GRU + SpatialMemory
- 单体超强 agent + 简单陪衬

---

## 项目布局

```
deep_protagonist/
  AGENTS.md            # AI 协作契约（新会话先读）
  CMakeLists.txt
  README.md
  src/                 # C++ 源码
  include/             # C++ 头文件
  configs/             # *.toml 运行时配置（default.toml）
  docs/                # PROGRESS + 4 份速查（中文）+ 最终蓝图 + HANDOFF
  scripts/             # build (*.ps1) / analysis (*.py)
                       # prune_runs.ps1 (每 D-XXX 训完跑, 保留最近 5 + gold)
  third_party/         # libtorch（gitignored，~2.6GB）
  runs/                # jsonl metrics + .pt checkpoints（gitignored）
                       # 命名：ppo_d0XX_2M.{jsonl,pt} / ppo_d0XX_5M.{jsonl,pt} / ppo_d0XX_7M.{jsonl,pt}
                       # 启动 bat: runs\run_d0XX.bat
                       # settler 测: settler_smoke.jsonl / settler_50ep.jsonl
  cmake/               # 自定义 CMake 模块
  build/               # 生成产物（gitignored）
  build_d100.bat       # vcvars64 + ninja deep_protagonist_train (D-100 起的 build 入口)
```

**⚠️ 接到任务时**：本 note 里的"当前状态"和"决策日志"会随用户开发滞后。**新会话第一件事 = SSH 上去** `Get-Content docs/PROGRESS.md -TotalCount 50`，那里有最准确的最后更新日期 + 当前阶段 + in-flight 训练状态。

---

## Quick start（用户机器跑）

```powershell
# 1) 下 libtorch（~2.65GB，一次性）
powershell -ExecutionPolicy Bypass -File scripts\download_libtorch.ps1

# 2) Configure / Build (D-094+ 用 build_d100.bat: vcvars64 + ninja)
cmd /c build_d100.bat

# 3) Self-test（应报 SELF-TESTS PASSED，130+ PASS lines D-062 起）
.\build\bin\deep_protagonist.exe --self-test

# 4) Headless train (D-100 起多 policy)
.\build\bin\deep_protagonist_train.exe --policy wander  ...  # baseline
.\build\bin\deep_protagonist_train.exe --policy ppo     ...  # 强化学习
.\build\bin\deep_protagonist_train.exe --policy settler ...  # D-100 ScriptedSettler oracle (BC demo source)
```

## Background 训练启动（autonomous 模式必用）

```powershell
# Start-Process 会被 SSH job-object 杀掉。必须用 scheduled task。
$batContent = @"
@echo off
cd /D D:\claude-code\c++\routes\deep_protagonist
build\bin\deep_protagonist_train.exe --policy ppo --steps 5000000 --seed 12345 --metrics runs\ppo_d0YY_5M.jsonl --save-path runs\ppo_d0YY_5M.pt --save-every 100 > runs\ppo_d0YY_5M.stdout.log 2> runs\ppo_d0YY_5M.stderr.log
"@
Set-Content -Path runs\run_d0YY.bat -Value $batContent -Encoding ASCII
Unregister-ScheduledTask -TaskName "dp_d0YY" -Confirm:$false -ErrorAction SilentlyContinue
$action = New-ScheduledTaskAction -Execute "D:\claude-code\c++\routes\deep_protagonist\runs\run_d0YY.bat"
$trigger = New-ScheduledTaskTrigger -Once -At ((Get-Date).AddSeconds(5))
$settings = New-ScheduledTaskSettingsSet -ExecutionTimeLimit ([TimeSpan]::Zero) -AllowStartIfOnBatteries -DontStopIfGoingOnBatteries -StartWhenAvailable
Register-ScheduledTask -TaskName "dp_d0YY" -Action $action -Trigger $trigger -Settings $settings -RunLevel Limited | Out-Null
```

## 训练数据分析（py + ps）

```powershell
python3 scripts\summarize_metrics.py runs\ppo_d064_2M.jsonl
# 也可用 PowerShell 直接分析 jsonl（D-063 示例）：
# Get-Content runs\ppo_d064_2M.jsonl | ForEach-Object { $_ | ConvertFrom-Json }
# unlocks 数组位序: [drink, eat, collect, spear, shelter, clothing, ...]
# hidden first_hunt 启发: r_milestone > 16.5（无对应 visible unlock 新增）
# D-091..D-099 PPO 看 bldg_ticks + night_shelter/night_total ratio, 不看 score
# D-100 ScriptedSettler 看 bldg_ticks >= 1000/ep, n_shel ratio >= 0.5, houses_built <= 2/ep
```

---

## Tool 行为规则（token 经济学，AGENTS.md 原话）

- **shell context 缓存 5 分钟**。开 background 训练 / 长任务后，**polling 间隔必须 ≤ 4 分钟**，超时缓存失效要重读 context，烧钱
- 训练命令一律 scheduled task 启动（见上）；poll 用 `Get-Process` + `Get-Content runs\*.jsonl -Tail 5`
- 间歇期间做：写文档 / 改其他文件 / 准备分析脚本（让等待不浪费）
- exec timeout 也别超过 4 分钟同理
- **bash sleep 不是 wait** (D-099 用户原话)：exec sleep 实际 5s 后 background, 但 get_output(300000) 可以阻塞 5 min, 7 次 polling = 40 min, token 比 wait 略省

---

## chisel 链路守护（2026-05-27 晚故障复盘）

- chisel server (user 端) 可能被 Defender PUA 拦截死掉 → 隧道 503
- **断线复盘**：curl `https://t1dwvu6bzdng.vip3.xiaomiqiu123.top` 返回 503 + "无法连接到 127.0.0.1:3303" = chisel server 死了
- **修法**：让用户开 PowerShell 跑 `D:\tools\chisel\chisel.exe server --port 3303`（窗口别关）
- 建议接力号在 chisel 长期断线时先想办法让用户启个 `schtasks /create /tn chisel_d /sc onlogon /tr "D:\tools\chisel\chisel.exe server --port 3303"` 开机自启，减少 babysitting
- 即使 SSH 断了，**scheduled task 训练不受影响**（completely detached），SSH 回来直接读 jsonl 拿数据


---

## SESSION UPDATE 2026-06-01 晚 (executor, §4营养 + BC→PPO + 域随机化消融 + 公平eval纠错) — 接力必读 (D-101 → D-104)

**一句话：§4 营养修复有效；但学出来的 PPO 在 held-out seed 上仍 0% 存活，根因=策略学不会蛋白觅食的 learning gap（不是 horizon、不是世界难），域随机化单独不够。**

### D-101 §4 营养可生存性 gate PASS + BC 语料 (commit 94ed2e5 / 3fb81fc)
营养惰性修复(decay 标定 / vitamin 回填挂到 collect 觅食 / 岸边可叉鱼 / death_grace 8s) 后，settler oracle 64-seed 鲁棒扫：ep0 clean **28/64=44%**，protein 仅 4 例、vitamin 0 例（**营养已非杀手**），残余=早狼咬死 16 + 寒冷暴露 16（正交捕食/寒冷，PPO 重训该补）。录 28 个 clean-survivor settler demos（demos_s4/，551.8MB，仅存活局）作 BC 暖启动语料。**坑**：`--record-demos` 按 --steps 写满记录，**文件等大≠存活**，必须看 metrics 的 steps/r_death 辨真伪。

### D-102 PPO v1/v2 — BC 锚 KL 调参
- **v1**（KL coef1.0 强锚）：held-out 0/48 全死。强锚把策略钉死在 BC 附近，而 BC 在长局下不会持续觅食。
- **v2**（放松 KL coef0.3/target0.05, lr5e-5, ent8e-4）：训练 seed **62%** 存活，但 held-out **0/48** → **过拟合**坐实（背下训练世界蛋白源位置，没学通用觅食）。

### D-103 过拟合根因 = VecEnv 16 张固定图 + 域随机化修复 (commit a86b2eb)
根因：VecEnv 跑 `--n-envs 16 --seed 12345` **固定 16 张地形/资源图整个 3M 步**（动物每局重 seed，但地形+食物/资源位置建图时锁死永不重 roll）→ 策略背图。写 `--domain-rand`（每局 reset 重建世界、重 roll 地形+资源），build EXIT=0、crash-smoke 验证（80k步16局每局新世界、零崩溃）。**v3 = v2 配方 + --domain-rand 3M 步**（唯一变量，干净消融，checkpoint `runs/s4ppo_nutrition_v3_domrand.pt`）。

### D-104 ★公平协议纠错 + v3 裁定 = 域随机化单独不够★（本棒最重要，接力必读）
**核心教训：评测协议错会得出完全错误的结论。我先后犯两个错并已纠正：**
1. **"horizon 太短"是错的**：episode cap = max_episode_seconds 300s @ fixed_dt 1/30 = **9000 步**；protein 100→0 @ decay40/min + grace8s ≈ 158s = 4740 步 **< 9000** → 训练期蛋白压力**本来就有**，horizon 不是问题。
2. **`--episodes 8` 评测协议本身被污染**：env "relocate 10 fish near spawn" 只在**建图时**发生、per-episode reset **不重定位鱼** → 第 2~8 局鱼远→蛋白饿死。settler oracle 在同 8-episode 协议下也只 **8%**（protein 主导）→ 证明是**协议无效**，非策略问题。我之前报的 v3 "2%" 出自这个污染协议，已作废。

**验证协议**（fair：`--policy ppo --no-update --nutrition --nutri-protein-decay 40 --ppo-mask-fire-only --steps 9000 --episodes 1`，seeds 3000000–3000031 共 32 个 held-out，脚本 `tmp_s4\fair_v3.bat`）：

| 策略 | held-out 存活 | protein 饿死 | 备注 |
|---|---|---|---|
| settler oracle | **47% (15/32)** | **0** | 世界可生存、协议能区分 |
| PPO v2 | **0% (0/32)** | 25 | 过拟合 |
| PPO v3 域随机化 | **0% (0/32)** | 21 (+cold5/food6) | 域随机化没把存活抬离 0% |

**裁定**：域随机化单独**没把 held-out 存活抬离 0%**。oracle 同 seed 47% → 这是**策略学不会蛋白觅食的 learning gap**，不是世界难/horizon 问题。

**下一杆 v4（让蛋白觅食可学，按优先）**：①BC base 本身演示叉鱼/觅食（bc_v9 无此技能，强 KL 锚死学不动）②PBRS 势函数低蛋白时引导趋水/趋鱼（policy-invariant，绝不发直接奖，蓝图 §5 反 Goodhart）③加大探索 entropy ④更丰富域随机化（保留）。**eval 一律 fair 协议**（1ep×9000×多 held-out seed，且认 oracle≈47% 为可生存基线）。

> 注（纠错）：DP 全程是**单个原始人**，无社会/季节机制（我曾误把 PE 的亲缘/领地安到 DP）。DP 蓝图剩余阶段 = §4 营养 gate（本棒）→ Stage W1 天气/雨灭火/淋湿增寒/保暖冗余安全阀 → W2 耦合网(矛↔狩猎/体力/农耕/烹饪/火光吓退) → §5 立新 BC base + KL 退火 PPO → §6 多轴泛化门。DP 版"一锅炖" = 一次 warmstart 把 W1/W2 的 obs 通道全预留补零锁维（同 PE 锁血脉），机制全 wire(flag OFF 保旧 ckpt)、课程退火逐步加压、严格 PBRS。

### D-105 ★2026-06-02 收尾：s5 lineage 平台裁定 + viewer 拟真 + 训练全停★（接力必读）
**s5 PPO lineage（warm-start 自 16M 基线，跑在"逼猎+生态分区"世界）**：s5_v1→v1b→v2→v3 指标 jsonl 全部保留（`runs/s5_v*.jsonl`）。**s5_v2 跑满 16000000 步、干净退出**（日志实锤 `VecEnv done: 16000000 steps, 3354 episodes, 9723.91s wall, 1645 steps/s`），**校准重训成本 = 一条 16M lineage ≈ 2.7h 墙钟（~600 万步/小时）**。结果：**卡死在 ~4/10 里程碑、0 陆地猎杀**——靠"叉鱼拿蛋白 + 草衣御寒"就够活，永不需要打猎拿兽皮（正是用户"撒米"比喻里的"次优平台/躺平点"）。s5_v3 从 s5_v2.pt 热启续跑后**已停**。

**`phi_hunt_progress` 猎杀势能课程（已写、编译通过、未折入重训）**：带矛靠近鹿/兔→0.45 连续拉、得兽皮→0.5、做皮袍→0.75、穿上→1.0。**单调、无躺平平台、策略不变性（纯 PBRS reward shaping = 非破契约，可从 s5_v2.pt 热启）**。落地方式不是给击杀发奖（D-064 实测击杀奖励 8%→0% 被 Goodhart），而是势能 shaping + 弱化草衣保暖/收紧叉鱼蛋白，把"打猎→兽皮→皮袍御寒"逼成唯一连续活路。

**DP viewer 首次入库** `routes/deep_protagonist/viewer/web3d`（commit `4a69282`）：斑驳生态地形(沙漠/水域平滑、山地嶙峋)+34k 实例化草丛+不规则水盆地，已对齐 PE viewer。**唯一未完成 = 洞穴渲染**：散落 boulder→散架感、半球 dome→被否(像蒙古包/平地冒大包/6个雷同/洞口黑管子)。正确做法=移植 PE 的 cell 化小灰岩聚簇(`pe_web3d/src/main.js` L692–725)、不抬大 knoll/不做 dome（`dp_web3d` `raiseCaveHills` L131-149 抬丘过高需压低）。

**训练全停 + D 盘**：DP s5_v3(pid38188) 已杀、PE 保活 seed 50012-50017 清理（D: 213.8GB，维持>200G）。**下一步 = 唯一一次统一重训**：干净重链 exe（训练已停，LNK1104 解除）→ 重 dump → DP 折入猎杀课程切最终 lineage(~2.7h) + PE 起 3 fresh seed(含点火动作+洞穴obs，破契约)。完整交接见仓库根 `PROJECT_STATUS.md` + `routes/protagonist_ecology/docx/realism/REALISM_DECISION.md`。
