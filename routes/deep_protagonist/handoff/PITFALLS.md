# deep_protagonist · PITFALLS（坑 / 防错清单）

> 这些都是我（上一个模型）真踩过、或会误导你的坑。接手前过一遍，能省你几小时甚至几天。

## A. 指标会骗你
1. **`kills=+0.0` 不是击杀数，是击杀奖励**（按反-Goodhart 设计本来就是 0）。
   - 真·击杀数看日志里的 **`prey_kills=N`**（我新加的，和独立埋点交叉验证过一致）。
   - 历史上所有"kills=0"的结论很多是被这个假指标骗的，重看数据要用 `prey_kills`。
2. **jsonl 的 `unlocks` 数组没有 hunt 位**（只有 [drink,eat,collect,spear,shelter,clothing,seed,house,crop,follower] 这 10 个里程碑）。判断有没有狩猎要看 `prey_kills` 或 `r_collect>0.6`(Fur drop)，别指望 unlocks。
3. **"采样到某动作" ≠ "该机制有效"**。`tendfire` 每局按 416 次照样冻死 = "空按"。统计涌现谱要区分"按了几次"和"真产生效果几次"。

## B. 不许删的东西
4. **checkpoint 一律别删**（`runs\*.pt` / `*.opt`）。每个 48.8MB。删了要重训几小时。
5. **PE 的 `protagonist_ecology\data` 里的 checkpoint 尤其别乱删** —— 我上轮误删了 `onepot_sC_long_50002/.../checkpoint_gen79`，它其实是**所有 sD_warm 种子共用的热启动基座**，导致 4 个新种子启动全崩。清理前务必确认一个 checkpoint 不是别的种子的依赖基座。
6. **完成种子 50001-50007**（PE 要分析的资产）一根都别动。

## C. 连接 / 操作环境
7. **chisel 隧道会断**（用户重启电脑或你的 VM 重启 → chisel client 进程死 → SSH 不通）。每个新会话先重连，见 `CONNECT.md`。
8. **别用内联 PowerShell 带引号**通过 SSH 跑（`ssh ... powershell -Command "..."`）——引号转义会炸（"Unexpected token"）。**永远写成 .ps1 文件，scp 上去，再 `powershell -File`**。
9. **后台训练要能脱离 SSH 会话**：用 `Register-ScheduledTask` + .bat 包装 + `(Get-Date).AddSeconds(5)` 触发，SSH 断了也不死。**不要**用 `Start-Process -WindowStyle Hidden`（SSH 断开会被 job-object 杀掉，只剩 1KB stdout、没 PPO update）。
10. **日志可能写到别的项目盘**：别把 deep_protagonist 的日志输出到 `c++\tmp\` 或 `c++\data\runs\`——那两个目录是 protagonist_ecology 的，混用会让用户分不清谁的日志。

## D. 训练本身的坑
11. **PPO collapse 陷阱**：从某些 ckpt + reset_hidden + 任何 reward/world 改动 = PPO 崩溃（D-055~D-059 验证过 4 次）。改动前先 smoke 验证；崩了就 fresh restart 重训（reward landscape 一致就不崩）。
12. **加奖励路径不够、还要降探索成本**：D-062 只加 +5 reward bonus，PPO 在 2M 步里探不到 craft_spear path；D-063 给"起手包"(spawn 时直接发 1 Wood+1 Stone) 把探索成本降到 0 才触发。**当 reward 已给位但 PPO 不学，先查是不是 path 探索成本太高，而不是 step 不足**。
13. **老冠军底座掰不动**：D-135 实测，从 `r1_cook_s24` 微调狩猎仍 0 杀（旧"从不打猎"策略根深蒂固）；必须从 **BC 底座**(`bc_hv2.pt`) 微调才长得出 kills。
14. **训练铁律（防灾难性遗忘）**：同一网络从头训到尾**参数永不重置**；每加新任务**旧奖励权重必须保留**（不删不归零）；网络输出包含所有动作即使早期没用。违反=失职。

## E. 已坐实的负结论（别重跑，浪费 GPU）
15. **count-based 覆盖奖励**（D-130/131，给探索发糖）：5 档剂量梯度跑到 5-6M 步，explore_cells 开/关完全分不开，他只在出生角踱步。**这个方向死了**。
16. **资源稀缺**（D-132，出生点贫瘠+刷远+再分配）：他选择"少吃硬扛慢慢掉血"不"走出去找"。**单独用也不够**（叠加随机出生才有意义）。
17. 这两条的根因：**固定出生点让"龟在角里"成为可行解**。D-133 随机出生点才从根上破了它。

## F. 反-Goodhart（用户红线）
18. 不许"按一下就给分"的直接奖励刷指标。要用 **PBRS 势函数**（potential-based，最优解上刷不动分）。
19. 发现根因就**停下来从根上改**，别堆更多奖励尝试。用户原话："你都发现问题了你就停下来调整啊"。
20. 评估口径用"**用到的机制种类数↑ / 被填满的生态位数↑**"，不是某个 kills/fitness 标量。
