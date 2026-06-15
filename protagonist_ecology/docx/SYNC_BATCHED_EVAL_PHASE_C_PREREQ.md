# 同步批量 eval（GPU 提速）— Phase C 前置任务备忘

> 状态：**Phase C 开始之前落实**（用户 John 2026-05-31 决定）。本文是给 Phase C 执行者的交接备忘。
> 相关产物：commit `5958f25`（batched forward 修复 + 多 tick 逐位测试）、`inbox/round_8__20260531-085749.md`、单元测试 `MultiTickBitExactVsPerAgentForward`。

## 背景（为什么有这件事）
- batched/dense forward 的**神经网数学已修好且逐位一致**：补回了 CPU 全路在 motor_target 上的 `+0.15×working_memory.summary()[h%8]` 注入（旧 batched 缺它 → 历史"best 8764 vs 基线 2907"乱码根因）。单元测试 `MultiTickBitExactVsPerAgentForward` 跑 30 tick（工作记忆累积、addend≠0），per-agent `forward()` vs batched `prepareInputs→forwardCpu→finalizeOutputs` **worst abs diff = 0（精确零）**。
- **但端到端仍 gen0 即偏离**，根因在共享引擎 `src/core/world/layers/AgentLayer.cpp::tick`：
  - 生产/eval 串行路径（adt=1, 约 :211-220）= **异步更新**：每个 agent `decide(感知+前向)→applyCapabilities(改世界)` 后下一个才开始 → agent i 感知到 0..i-1 本 tick 已落地的动作。
  - batched 路径 `tickBatched`（约 :304-376）= **同步更新**：Phase1 全体感知同一 pre-action 快照，Phase4 才全体 apply。
  - 两种 within-tick「感知↔行动」顺序不同 → 轨迹确定性发散。与 adt>1 偏 58% 同源。
- 结论：batching 必须"先收齐全部 agent 输入再一次 matmul"= 必须先感知全部再行动 = 同步更新。**无法与异步串行 eval 逐位一致**，除非把 eval 也切到同步更新（= 重立 baseline）。**GPU/CUDA 与此无关**——调度差异独立于 matmul 后端。

## 目标
在 Phase C 新血脉实验**之前**，把 eval 一次性切到**同步批量前向（GPU）**，换取单 run 提速，并重立 baseline。

## 收益 / 成本（决策依据，已与用户对过）
- **收益**：单 run 提速 **仅在 GPU 上**有（CPU batched 实测慢 6×）；受 Amdahl 限（脑前向占每 tick ~68%）→ 实际约 **1.5–2×**。注意单世界仅 24 agent，matmul 偏小，**GPU kernel 启动开销可能吃掉大半收益**——要真榨干 GPU 建议把多 world×episode×genome 一起批（更大批量）。附带正面：同步更新是多 agent 同步步进的更标准做法，消除"按 agent 索引先后"的人为偏置，科学上能站住脚。
- **成本**：① 所有 baseline 作废重立（bc_v9 / bc_v12 + 整个 detP2/detP2c 确定性矩阵）；② 拟真结论需在新语义下重验（harmony 7/7 + 趋火预判节律 +11~12s 可能变）；③ 必须有能用的 CUDA（现 nvcc NOTFOUND，实体在 `G:\ai\nvidia\bin\nvcc.exe`，configure 时未进 PATH）；④ 动的是共享 `AgentLayer`，deep_protagonist 也用。
- **为什么放到 Phase C 才做**：当前 Phase B 靠"实验级并发跑 N 个 run"（吞吐 ~5.5×）已够，单 run 慢不卡科学。到 Phase C 会反复迭代**同一条长 run**、单 run 延迟变真瓶颈且无法靠并发绕过——那时提速才兑现，且 baseline 本来就要随新血脉重立，正好一次性做。

## 硬约束（用户 John 明确要求）
1. **做成独立的**：synchronous-batched eval 走 protagonist_ecology **route 私有路径**；对共享 `AgentLayer` 只做**附加式 flag-gated** 改动，**flag 关时 deep_protagonist 执行路径逐字节不变**。运行时 `batched_decision_hook_` 本就是 per-world 设置，deep_protagonist 不设 hook 就走不到 `tickBatched` → 天然运行时隔离。
2. **不干扰隔壁**：绝不改 deep_protagonist 默认语义；若必须动 AgentLayer 公共代码，先确认 `hook==null` 路径无变化，并**单独跑一遍 deep_protagonist 验证**。
3. **时序**：Phase C **开始之前**落实完（含重立 baseline + 重验 harmony/节律），否则新血脉实验又得重来。

## 落实步骤（建议）
1. 起**独立 CUDA build 目录**（不动现有 `build_ninja_cuda` 缓存、不碰生产 exe），用 `G:\ai\nvidia\bin\nvcc.exe` 让 CMake 启用 CUDA 语言，编出带 GPU kernel 的 `-batched` exe。
2. 先验 **batched-GPU vs batched-CPU 逐位一致**（runner 同时有两条路；`BatchedDenseGpu.cu` 与 `forwardCpu` 数学已对齐）。
3. 确认 `tickBatched` 在 adt=1 下**确定性可复现**（同 seed→同结果；它本身已是串行 4 阶段，确定性 OK，只是与异步 baseline 不同）。
4. 在同步语义下**重跑确定性矩阵**重立 bc_v9/bc_v12 等口径 baseline；重验 harmony 7/7 + 趋火预判节律。
5. 全程默认仍 flag-gated；只有 protagonist eval 显式开启 batched，deep_protagonist 永不开。

## 一句话
脑前向已逐位修好（commit 5958f25）；剩下的是"把 eval 整体切到同步更新"这个**阶段性、需重立 baseline 的工程项**，按用户决定放在 **Phase C 之前**做，做成 route 私有、不碰 deep_protagonist，且先把 CUDA 跑通。
