# decision — protagonist_ecology 今日收尾 / 交接（EOD handoff）

## 0. 一句话
**今天就到这里了——用户要换号了。** 本轮把手上的活收尾、把文档全部写好，然后**更新交接文档**，今天不再开新一轮长跑。

## 1. 收尾要求（请执行者完成后即可停）
1. **把当前正在跑的活干净收尾**：
   - 若 gen79→~160 代续跑（decision_round_6 的候选3 诊断）正在跑：要么跑完出结论、要么**干净停在可恢复 checkpoint**（保留 gen 进度，下次可续）。
   - commit 本地（前缀 `protagonist_ecology:`，不 push）。
2. **把该写的文档全部写好/更新到最新**：
   - 本项目 `PROGRESS / STATUS / 决策记录`：写到「截至今日」的真实状态。
   - 最后一轮若有结论，照常写 `inbox/round_<N>__<UTC>.md`（我已不再回 outbox，标注「决策者今日收尾、本轮自归档」即可）。
3. **更新交接文档（关键）**：把 `C:\Users\sans\Desktop\agent\prompt\protagonist_ecology_knowledge_note_2026-05-30.md` 更新到**最新状态**，让明天新上岗的执行者读完即可接上。至少含：
   - **阶段定向**：判据已切到**「节律拟真」**——env-perc 永远开、OFF 只做阴性对照、不再用 ON>OFF fitness 当门槛；**标准尺 = phase-lead（预判/相位超前），不是同期相关**。
   - **Phase B 首个诚实阳性（里程碑）**：在可存活选择压世界（`cold_temp_threshold 30→22` + `thermal_damage_per_second 0.5→0.3`，C7 复活 harmony 7/7、仍有冷致死梯度）下，**ON 进化出 OFF 没有的"趋火预判"节律**：趋火峰值领先体温最低点 **+11~12s**，3 seed 一致(fp0.95×3)，gen0 无、OFF 反应式(+4s)。**但很弱**（趋火谐波 R²≤0.08、振幅~0.005；活动量通道仍纯机械）。
   - **营火物理已核实**：fire_warming +1.0°C/s、平衡点 ambient+20，暖源净正本就成立（warmth_recovery=0.06 只是次要回血）→ Step2/3 未触发，单 Step1 即满足"好梯度"判据。
   - **当前未决诊断（下次第一步）= decision_round_6 候选3**：gen79→~160 代续跑，看趋火节律 R²/振幅是否随更长选择**继续上升**：涨→编码够用、继续选择即增强、**不必开新血脉**；plateau→坐实**编码=天花板** → 才开新血脉上候选1+2（显式昼夜相位输入 + 体温趋势/预测项 + goal 词表加"趋暖/趋火"目标通道），**改输入维=新血脉、smoke 先行**。
   - **红线**：节律绝不靠 reward 塑形（必须从"感知是适应性的"涌现）；调 warmth/火半径=物理调参≠reward；C7 不灭绝是硬地板；harmony 7/7 维持；adt=1+pool 确定性可复现；fitness 仅附录。
   - 配置/数据位置：`data/runs/realism_detP2b_th22_*`；agg `tmp/trace_export/agg_b_*.csv`；脚本 `phase_harmonic.py / rhythm_agg.ps1 / goal_rhythm.ps1`。

## 2. 红线（不变）
见上。重点：**节律不靠 reward 逼**、**改输入维=新血脉显式**、C7 地板、harmony 7/7、确定性可复现。

→ **今日收尾：干净停续跑(保 checkpoint) + commit + 写全 PROGRESS/STATUS + 把 `protagonist_ecology_knowledge_note_2026-05-30.md` 更新到最新（节律拟真定向 / 首个阳性"+11~12s 趋火预判" / 好梯度物理参数 / 下次=gen→160 候选3诊断 / plateau才开新血脉候选1+2 / 全套红线）。完成即可停，不再开新一轮。**
